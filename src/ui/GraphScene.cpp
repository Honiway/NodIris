/**
 * @file GraphScene.cpp
 * @brief Keeps graph data and canvas items in sync.
 */

#include "ui/GraphScene.h"

#include "core/Layout.h"
#include "core/ProjectFile.h"
#include "core/Thumbnail.h"
#include "ui/Commands.h"
#include "ui/LinkItem.h"
#include "ui/NeuronItem.h"
#include "ui/Theme.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <algorithm>
#include <climits>
#include <cmath>
#include <unordered_map>

namespace Ogt
{

namespace
{

constexpr int kGridSpacing = 24;
// Leave enough space to move the view in every direction.
constexpr qreal kSceneExtent = 100000;

} // namespace

GraphScene::GraphScene(QObject* parent)
    : QGraphicsScene(parent)
{
    setSceneRect(-kSceneExtent, -kSceneExtent, 2 * kSceneExtent, 2 * kSceneExtent);
    // The BSP index makes item search faster on large graphs.
    setItemIndexMethod(BspTreeIndex);
    connect(&Theme::Get(), &Theme::Changed, this, [this]
    {
        for (LinkItem* link : linkItems_)
            link->Adjust();
        update();
    });
}

GraphScene::~GraphScene()
{
    ClearItems();
}

void GraphScene::ClearItems()
{
    // Delete links first because they point to neuron items.
    for (LinkItem* link : linkItems_)
        delete link;
    linkItems_.clear();
    qDeleteAll(items_);
    items_.clear();
    delete draftLink_;
    draftLink_ = nullptr;
    linkSource_ = nullptr;
    dropTarget_ = nullptr;
    multiLinkSources_.clear();
    pressPositions_.clear();
}

void GraphScene::Load(Graph graph, const QString& projectFile)
{
    undo_.clear();
    graph_ = std::move(graph);
    projectFile_ = projectFile;
    ShowFolder(0);
}

void GraphScene::ShowFolder(quint32 folder)
{
    const Neuron* folderNeuron = graph_.FindNeuron(folder);
    if (folder != 0 && (!folderNeuron || folderNeuron->kind != NeuronKind::Folder))
        folder = 0;

    ClearItems();
    folder_ = folder;
    for (quint32 id : graph_.Children(folder_))
        CreateItem(*graph_.FindNeuron(id));
    for (const Link& link : graph_.Links())
        CreateLinkItem(link);
    UpdateLinkGroups();
    ApplyFilter();
    emit FolderShown(folder_);
}

std::vector<quint32> GraphScene::FolderPath() const
{
    std::vector<quint32> path;
    for (const Neuron* neuron = graph_.FindNeuron(folder_); neuron; neuron = graph_.FindNeuron(neuron->parent))
    {
        path.insert(path.begin(), neuron->id);
        if (path.size() > graph_.Neurons().size())
            break;
    }
    return path;
}

int GraphScene::ChildCount(quint32 folder) const
{
    return int(graph_.Children(folder).size());
}

void GraphScene::CreateItem(const Neuron& neuron)
{
    if (neuron.parent != folder_ || items_.contains(neuron.id))
        return;
    auto* item = new NeuronItem(neuron, projectFile_, ChildCount(neuron.id));
    addItem(item);
    items_.insert(neuron.id, item);
}

void GraphScene::CreateLinkItem(const Link& link)
{
    NeuronItem* from = items_.value(link.from);
    NeuronItem* to = items_.value(link.to);
    if (!from || !to || LinkItemFor(link.from, link.to))
        return;
    auto* item = new LinkItem(from, to, link);
    addItem(item);
    linkItems_.push_back(item);
}

void GraphScene::RefreshFolderItem(quint32 folder)
{
    const Neuron* neuron = graph_.FindNeuron(folder);
    NeuronItem* item = items_.value(folder);
    if (neuron && item)
        item->SetNeuron(*neuron, projectFile_, ChildCount(folder));
}

void GraphScene::InsertNeuron(const Neuron& neuron)
{
    graph_.AddNeuron(neuron);
    CreateItem(neuron);
    RefreshFolderItem(neuron.parent);
    ApplyFilter();
    emit Changed();
}

void GraphScene::EraseNeuron(quint32 id)
{
    const Neuron* neuron = graph_.FindNeuron(id);
    if (!neuron)
        return;
    const quint32 parent = neuron->parent;
    for (const Link& link : graph_.LinksOf(id))
        EraseLink(link.from, link.to);
    graph_.RemoveNeuron(id);
    delete items_.take(id);
    RefreshFolderItem(parent);

    // Leave the current folder when undo removes it.
    if (folder_ != 0 && !graph_.FindNeuron(folder_))
        ShowFolder(parent);
    emit Changed();
}

void GraphScene::ReplaceNeuron(const Neuron& neuron)
{
    Neuron* savedNeuron = graph_.FindNeuron(neuron.id);
    if (!savedNeuron)
        return;
    const quint32 oldParent = savedNeuron->parent;
    *savedNeuron = neuron;

    if (oldParent != neuron.parent)
    {
        // Recreate the item when its folder changes.
        delete items_.take(neuron.id);
        CreateItem(neuron);
        for (const Link& link : graph_.LinksOf(neuron.id))
            CreateLinkItem(link);
        RefreshFolderItem(oldParent);
        RefreshFolderItem(neuron.parent);
    }
    else if (NeuronItem* item = items_.value(neuron.id))
    {
        item->SetNeuron(neuron, projectFile_, ChildCount(neuron.id));
    }
    ApplyFilter();
    emit Changed();
}

void GraphScene::PlaceNeurons(const QHash<quint32, QPoint>& positions)
{
    for (auto it = positions.begin(); it != positions.end(); ++it)
    {
        if (Neuron* neuron = graph_.FindNeuron(it.key()))
            neuron->position = it.value();
        if (NeuronItem* item = items_.value(it.key()))
            item->setPos(it.value());
    }
    emit Changed();
}

void GraphScene::InsertLink(const Link& link)
{
    if (!graph_.AddLink(link))
        return;
    CreateLinkItem(link);
    UpdateLinkGroups();
    emit Changed();
}

void GraphScene::EraseLink(quint32 from, quint32 to)
{
    if (!graph_.RemoveLink(from, to))
        return;
    if (LinkItem* item = LinkItemFor(from, to))
    {
        std::erase(linkItems_, item);
        delete item;
    }
    UpdateLinkGroups();
    emit Changed();
}

void GraphScene::ReplaceLink(const Link& link)
{
    Link* savedLink = graph_.FindLink(link.from, link.to);
    if (!savedLink)
        return;
    *savedLink = link;
    if (LinkItem* item = LinkItemFor(link.from, link.to))
        item->SetLink(link);
    UpdateLinkGroups();
    emit Changed();
}

quint32 GraphScene::AddNeuron(Neuron neuron)
{
    neuron.id = graph_.NextId();
    neuron.parent = folder_;
    neuron.position = SnapToGrid(neuron.position);
    const QString commandName = QCoreApplication::translate("Ogt", "Add %1").arg(KindName(neuron.kind));
    undo_.push(new ChangeSetCommand(this, {neuron}, {}, false, commandName));
    return neuron.id;
}

void GraphScene::AddFiles(const QStringList& paths, QPointF scenePosition)
{
    std::vector<Neuron> neurons;
    quint32 id = graph_.NextId();
    QPoint nextPosition = SnapToGrid(scenePosition.toPoint());
    for (const QString& path : paths)
    {
        const QFileInfo info(path);
        if (!info.isFile())
            continue;
        Neuron neuron;
        neuron.id = id++;
        neuron.parent = folder_;
        neuron.kind = KindForPath(path);
        neuron.title = info.completeBaseName();
        neuron.path = RelativeToProject(projectFile_, info.absoluteFilePath());
        if (neuron.kind == NeuronKind::Image)
            neuron.thumbnail = MakeThumbnail(path);
        neuron.position = nextPosition;
        // Place imported files side by side.
        nextPosition += QPoint(NeuronItem::kWidth + 16, 0);
        neurons.push_back(std::move(neuron));
    }
    if (neurons.empty())
        return;

    const int count = int(neurons.size());
    const QString commandName = count == 1
                                    ? QCoreApplication::translate("Ogt", "Add %1").arg(neurons.front().title)
                                    : QCoreApplication::translate("Ogt", "Add %n files", nullptr, count);
    undo_.push(new ChangeSetCommand(this, neurons, {}, false, commandName));

    clearSelection();
    for (const Neuron& neuron : neurons)
        items_.value(neuron.id)->setSelected(true);
}

void GraphScene::Connect(quint32 from, quint32 to)
{
    const Neuron* sourceNeuron = graph_.FindNeuron(from);
    const Neuron* targetNeuron = graph_.FindNeuron(to);
    if (!sourceNeuron || !targetNeuron || from == to || sourceNeuron->parent != targetNeuron->parent
        || graph_.FindLink(from, to) || graph_.FindLink(to, from))
    {
        emit Feedback(QCoreApplication::translate("Ogt", "These neurons cannot be linked."));
        return;
    }
    // A link between two tasks shows their order.
    const LinkKind kind = sourceNeuron->kind == NeuronKind::Task && targetNeuron->kind == NeuronKind::Task
                              ? LinkKind::LeadsTo : LinkKind::Relates;
    const QString commandName = QCoreApplication::translate("Ogt", "Link");
    undo_.push(new ChangeSetCommand(this, {}, {{from, to, kind, {}}}, false, commandName));
    emit Feedback(QCoreApplication::translate("Ogt", "Neurons linked."));
}

void GraphScene::ConnectMany(const std::vector<quint32>& sources, quint32 target)
{
    const Neuron* targetNeuron = graph_.FindNeuron(target);
    if (!targetNeuron)
        return;
    quint32 joinGroup = 1;
    for (const Link& savedLink : graph_.Links())
        joinGroup = std::max(joinGroup, savedLink.joinGroup + 1);
    std::vector<Link> links;
    for (quint32 source : sources)
    {
        const Neuron* sourceNeuron = graph_.FindNeuron(source);
        if (!sourceNeuron || source == target || sourceNeuron->parent != targetNeuron->parent
            || graph_.FindLink(source, target)
            || graph_.FindLink(target, source))
            continue;
        const LinkKind kind = sourceNeuron->kind == NeuronKind::Task && targetNeuron->kind == NeuronKind::Task
                                  ? LinkKind::LeadsTo : LinkKind::Relates;
        links.push_back({source, target, kind, {}, joinGroup});
    }
    if (links.empty())
    {
        emit Feedback(QCoreApplication::translate("Ogt", "No new link could be created."));
        return;
    }
    const int count = int(links.size());
    undo_.push(new ChangeSetCommand(this, {}, links, false,
                                    QCoreApplication::translate("Ogt", "Link %n neurons", nullptr, count)));
    emit Feedback(QCoreApplication::translate("Ogt", "%n neurons linked to the target.", nullptr, count));
}

void GraphScene::Paste(const std::vector<Neuron>& neurons, const std::vector<Link>& links, QPointF center)
{
    if (neurons.empty())
        return;
    QPoint topLeft(INT_MAX, INT_MAX);
    QPoint bottomRight(INT_MIN, INT_MIN);
    for (const Neuron& neuron : neurons)
    {
        topLeft.setX(std::min(topLeft.x(), neuron.position.x()));
        topLeft.setY(std::min(topLeft.y(), neuron.position.y()));
        const QPoint neuronBottomRight =
            neuron.position + QPoint(NeuronItem::kWidth, NeuronItem::SizeFor(neuron).height());
        bottomRight.setX(std::max(bottomRight.x(), neuronBottomRight.x()));
        bottomRight.setY(std::max(bottomRight.y(), neuronBottomRight.y()));
    }
    const QPoint offset = SnapToGrid((center - QPointF(topLeft + bottomRight) / 2.0).toPoint());
    std::unordered_map<quint32, quint32> ids;
    std::vector<Neuron> copies;
    quint32 next = graph_.NextId();
    for (Neuron copy : neurons)
    {
        const quint32 old = copy.id;
        copy.id = next++;
        copy.parent = folder_;
        copy.position = SnapToGrid(copy.position + offset);
        ids.emplace(old, copy.id);
        copies.push_back(std::move(copy));
    }
    std::vector<Link> copiedLinks;
    for (Link link : links)
    {
        if (ids.contains(link.from) && ids.contains(link.to))
        {
            link.from = ids.at(link.from);
            link.to = ids.at(link.to);
            copiedLinks.push_back(std::move(link));
        }
    }
    undo_.push(new ChangeSetCommand(this, copies, copiedLinks, false,
                                    QCoreApplication::translate("Ogt", "Paste")));
    clearSelection();
    for (const Neuron& copy : copies)
        items_.value(copy.id)->setSelected(true);
}

void GraphScene::SetLinkToolEnabled(bool enabled)
{
    if (linkToolEnabled_ == enabled)
        return;
    linkToolEnabled_ = enabled;
    if (enabled && multiLinkToolEnabled_)
    {
        multiLinkToolEnabled_ = false;
        multiLinkSources_.clear();
        emit MultiLinkToolEnabledChanged(false);
    }
    linkSource_ = nullptr;
    delete draftLink_;
    draftLink_ = nullptr;
    emit LinkToolEnabledChanged(enabled);
    emit Feedback(enabled ? QCoreApplication::translate("Ogt", "Click the first neuron to link.")
                          : QCoreApplication::translate("Ogt", "Link tool closed."));
}

void GraphScene::SetMultiLinkToolEnabled(bool enabled)
{
    if (multiLinkToolEnabled_ == enabled)
        return;
    multiLinkToolEnabled_ = enabled;
    multiLinkSources_.clear();
    clearSelection();
    if (enabled && linkToolEnabled_)
    {
        linkToolEnabled_ = false;
        linkSource_ = nullptr;
        delete draftLink_;
        draftLink_ = nullptr;
        emit LinkToolEnabledChanged(false);
    }
    emit MultiLinkToolEnabledChanged(enabled);
    emit Feedback(enabled ? QCoreApplication::translate(
                                "Ogt", "Left-click the source neurons, then right-click the target.")
                          : QCoreApplication::translate("Ogt", "Multiple link tool closed."));
}

void GraphScene::EditNeuron(const Neuron& after)
{
    const Neuron* before = graph_.FindNeuron(after.id);
    if (!before || *before == after)
        return;
    undo_.push(new EditNeuronCommand(this, *before, after));
}

void GraphScene::EditLink(const Link& after)
{
    const Link* before = graph_.FindLink(after.from, after.to);
    if (!before || *before == after)
        return;
    undo_.push(new EditLinkCommand(this, *before, after));
}

void GraphScene::DeleteSelection()
{
    std::vector<Neuron> neurons;
    QSet<quint32> ids;
    auto addNeuronToDelete = [&](quint32 id)
    {
        if (!ids.contains(id))
        {
            ids.insert(id);
            neurons.push_back(*graph_.FindNeuron(id));
        }
    };
    for (QGraphicsItem* item : selectedItems())
    {
        if (item->type() != NeuronItem::Type)
            continue;
        const quint32 id = static_cast<NeuronItem*>(item)->Id();
        addNeuronToDelete(id);
        // Save the folder before its children so undo restores it first.
        for (quint32 childId : graph_.Descendants(id))
            addNeuronToDelete(childId);
    }

    // Remove links connected to deleted neurons and links selected by the user.
    std::vector<Link> links;
    for (const Link& link : graph_.Links())
    {
        LinkItem* item = LinkItemFor(link.from, link.to);
        if (ids.contains(link.from) || ids.contains(link.to) || (item && item->isSelected()))
            links.push_back(link);
    }
    if (neurons.empty() && links.empty())
        return;
    const QString commandName =
        neurons.empty() ? QCoreApplication::translate("Ogt", "Delete link")
                        : QCoreApplication::translate("Ogt", "Delete %n neurons", nullptr, int(neurons.size()));
    undo_.push(new ChangeSetCommand(this, std::move(neurons), std::move(links), true, commandName));
}

void GraphScene::MoveIntoFolder(const std::vector<quint32>& ids, quint32 folder)
{
    const Neuron* targetFolder = graph_.FindNeuron(folder);
    if (!targetFolder || targetFolder->kind != NeuronKind::Folder)
        return;
    QSet<quint32> moving;
    for (quint32 id : ids)
    {
        // A folder cannot move inside itself or one of its children.
        if (id != folder && !graph_.IsInside(folder, id) && graph_.FindNeuron(id))
            moving.insert(id);
    }
    if (moving.isEmpty())
        return;

    // Remove links between the moved group and neurons outside the group.
    std::vector<Link> cut;
    for (const Link& link : graph_.Links())
    {
        if (moving.contains(link.from) != moving.contains(link.to))
            cut.push_back(link);
    }

    // Keep the group shape and place it after the current folder content.
    QRect existing;
    for (quint32 id : graph_.Children(folder))
    {
        const Neuron* inside = graph_.FindNeuron(id);
        existing |= QRect(inside->position, NeuronItem::SizeFor(*inside));
    }
    QPoint groupCorner(INT_MAX, INT_MAX);
    for (quint32 id : moving)
    {
        const QPoint neuronPosition = graph_.FindNeuron(id)->position;
        groupCorner = QPoint(std::min(groupCorner.x(), neuronPosition.x()),
                             std::min(groupCorner.y(), neuronPosition.y()));
    }
    const QPoint landing = existing.isNull() ? QPoint(0, 0) : QPoint(existing.right() + 64, existing.top());

    undo_.beginMacro(QCoreApplication::translate("Ogt", "Move into %1").arg(targetFolder->title));
    if (!cut.empty())
        undo_.push(new ChangeSetCommand(this, {}, cut, true, {}));
    for (quint32 id : moving)
    {
        Neuron after = *graph_.FindNeuron(id);
        after.parent = folder;
        after.position = SnapToGrid(after.position - groupCorner + landing);
        undo_.push(new EditNeuronCommand(this, *graph_.FindNeuron(id), after));
    }
    undo_.endMacro();
}

void GraphScene::SetFilter(const QString& query)
{
    filter_ = query.trimmed();
    ApplyFilter();
}

void GraphScene::ApplyFilter()
{
    matches_ = graph_.Search(filter_);
    const QSet<quint32> hits(matches_.begin(), matches_.end());
    for (auto it = items_.begin(); it != items_.end(); ++it)
    {
        // Show a folder when one of its children matches the search.
        bool lit = hits.contains(it.key());
        if (!lit && it.value()->Kind() == NeuronKind::Folder)
        {
            for (quint32 matchId : matches_)
                lit = lit || graph_.IsInside(matchId, it.key());
        }
        it.value()->SetDimmed(!filter_.isEmpty() && !lit);
    }
}

std::optional<quint32> GraphScene::SelectedNeuron() const
{
    const QList<QGraphicsItem*> selection = selectedItems();
    if (selection.size() == 1 && selection.front()->type() == NeuronItem::Type)
        return static_cast<NeuronItem*>(selection.front())->Id();
    return std::nullopt;
}

std::optional<Link> GraphScene::SelectedLink() const
{
    const QList<QGraphicsItem*> selection = selectedItems();
    if (selection.size() == 1 && selection.front()->type() == LinkItem::Type)
        return static_cast<LinkItem*>(selection.front())->GetLink();
    return std::nullopt;
}

void GraphScene::SelectOnly(quint32 id)
{
    clearSelection();
    if (NeuronItem* item = items_.value(id))
        item->setSelected(true);
}

QRectF GraphScene::ContentRect() const
{
    QRectF rect;
    for (const NeuronItem* item : items_)
        rect |= item->sceneBoundingRect();
    return rect;
}

NeuronItem* GraphScene::NeuronAt(QPointF scenePos) const
{
    for (QGraphicsItem* item : items(scenePos))
    {
        if (item->type() == NeuronItem::Type)
            return static_cast<NeuronItem*>(item);
    }
    return nullptr;
}

NeuronItem* GraphScene::FolderUnderDrag(QPointF scenePos) const
{
    for (QGraphicsItem* item : items(scenePos))
    {
        if (item->type() != NeuronItem::Type || item->isSelected())
            continue;
        auto* neuron = static_cast<NeuronItem*>(item);
        if (neuron->Kind() == NeuronKind::Folder && neuron->CircleContains(neuron->mapFromScene(scenePos)))
            return neuron;
    }
    return nullptr;
}

LinkItem* GraphScene::LinkItemFor(quint32 from, quint32 to) const
{
    for (LinkItem* item : linkItems_)
    {
        if (item->GetLink().from == from && item->GetLink().to == to)
            return item;
    }
    return nullptr;
}

void GraphScene::UpdateLinkGroups()
{
    QHash<quint64, std::vector<LinkItem*>> groups;
    for (LinkItem* item : linkItems_)
    {
        item->ClearJoinGroup();
        const Link& link = item->GetLink();
        if (link.joinGroup != 0)
        {
            const quint64 groupKey = (quint64(link.joinGroup) << 32) | link.to;
            groups[groupKey].push_back(item);
        }
    }
    for (const std::vector<LinkItem*>& group : groups)
    {
        if (group.size() < 2)
            continue;
        for (size_t index = 0; index < group.size(); ++index)
            group[index]->SetJoinGroup(group, index == 0);
    }
}

void GraphScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    const ThemeTokens& theme = Theme::Get().Tokens();
    painter->fillRect(rect, theme.canvas);

    // Hide grid dots below half size.
    const qreal scale = painter->worldTransform().m11();
    if (scale < 0.5)
        return;

    const qreal left = std::floor(rect.left() / kGridSpacing) * kGridSpacing;
    const qreal top = std::floor(rect.top() / kGridSpacing) * kGridSpacing;
    QVector<QPointF> dots;
    dots.reserve(int((rect.width() / kGridSpacing + 2) * (rect.height() / kGridSpacing + 2)));
    for (qreal x = left; x < rect.right(); x += kGridSpacing)
    {
        for (qreal y = top; y < rect.bottom(); y += kGridSpacing)
            dots.append(QPointF(x, y));
    }
    painter->setPen(QPen(theme.gridDot, 1.5 / scale, Qt::SolidLine, Qt::RoundCap));
    painter->drawPoints(dots.constData(), int(dots.size()));
}

void GraphScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (multiLinkToolEnabled_ && event->button() == Qt::RightButton)
    {
        if (NeuronItem* target = NeuronAt(event->scenePos()))
        {
            ConnectMany(multiLinkSources_, target->Id());
            multiLinkSources_.clear();
            clearSelection();
        }
        else
        {
            emit Feedback(QCoreApplication::translate("Ogt", "Right-click a target neuron."));
        }
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton)
    {
        NeuronItem* item = NeuronAt(event->scenePos());
        if (multiLinkToolEnabled_ && item)
        {
            const auto selectedSource = std::find(multiLinkSources_.begin(), multiLinkSources_.end(), item->Id());
            if (selectedSource == multiLinkSources_.end())
            {
                multiLinkSources_.push_back(item->Id());
                item->setSelected(true);
            }
            else
            {
                multiLinkSources_.erase(selectedSource);
                item->setSelected(false);
            }
            emit Feedback(QCoreApplication::translate("Ogt", "%n source neuron(s) selected. Right-click the target.",
                                                       nullptr, int(multiLinkSources_.size())));
            event->accept();
            return;
        }
        if (linkToolEnabled_ && item)
        {
            if (!linkSource_)
            {
                linkSource_ = item;
                clearSelection();
                item->setSelected(true);
                emit Feedback(QCoreApplication::translate("Ogt", "Now click the second neuron."));
            }
            else
            {
                const quint32 from = linkSource_->Id();
                linkSource_ = nullptr;
                Connect(from, item->Id());
            }
            event->accept();
            return;
        }
        const std::optional<QPointF> handle = item ? item->HandleCenterAt(item->mapFromScene(event->scenePos()))
                                                   : std::nullopt;
        if (item && handle)
        {
            linkSource_ = item;
            linkStart_ = item->mapToScene(*handle);
            draftLink_ = new QGraphicsLineItem();
            draftLink_->setPen(QPen(Theme::Get().Tokens().accent, 1.5, Qt::DashLine, Qt::RoundCap));
            draftLink_->setZValue(2);
            addItem(draftLink_);
            event->accept();
            return;
        }
    }

    QGraphicsScene::mousePressEvent(event);

    pressPositions_.clear();
    for (QGraphicsItem* item : selectedItems())
    {
        if (item->type() == NeuronItem::Type)
            pressPositions_.insert(static_cast<NeuronItem*>(item)->Id(), item->pos().toPoint());
    }
}

void GraphScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (linkSource_)
    {
        if (linkToolEnabled_)
            return;
        NeuronItem* target = NeuronAt(event->scenePos());
        if (target && target != linkSource_)
            draftLink_->setLine(LinkItem::Between(linkStart_, 0, target->SceneCenter(), NeuronItem::kRadius));
        else
            draftLink_->setLine(LinkItem::Between(linkStart_, 0, event->scenePos(), 0));
        return;
    }

    QGraphicsScene::mouseMoveEvent(event);

    if ((event->buttons() & Qt::LeftButton) && !pressPositions_.isEmpty())
    {
        NeuronItem* target = FolderUnderDrag(event->scenePos());
        if (target != dropTarget_)
        {
            if (dropTarget_)
                dropTarget_->SetDropTarget(false);
            dropTarget_ = target;
            if (dropTarget_)
                dropTarget_->SetDropTarget(true);
        }
    }
}

void GraphScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (linkSource_)
    {
        if (linkToolEnabled_)
            return;
        NeuronItem* target = NeuronAt(event->scenePos());
        const quint32 from = linkSource_->Id();
        delete draftLink_;
        draftLink_ = nullptr;
        linkSource_ = nullptr;
        if (target)
            Connect(from, target->Id());
        return;
    }

    QGraphicsScene::mouseReleaseEvent(event);

    if (dropTarget_)
    {
        // Move selected neurons into the folder under the mouse.
        const quint32 folder = dropTarget_->Id();
        dropTarget_->SetDropTarget(false);
        dropTarget_ = nullptr;
        std::vector<quint32> ids;
        for (auto it = pressPositions_.begin(); it != pressPositions_.end(); ++it)
        {
            if (NeuronItem* item = items_.value(it.key()))
                item->setPos(it.value());
            ids.push_back(it.key());
        }
        pressPositions_.clear();
        MoveIntoFolder(ids, folder);
        return;
    }

    QHash<quint32, QPoint> before;
    QHash<quint32, QPoint> after;
    for (auto it = pressPositions_.begin(); it != pressPositions_.end(); ++it)
    {
        NeuronItem* item = items_.value(it.key());
        if (item && item->pos().toPoint() != it.value())
        {
            before.insert(it.key(), it.value());
            after.insert(it.key(), item->pos().toPoint());
        }
    }
    pressPositions_.clear();
    if (!after.isEmpty())
        undo_.push(new MoveCommand(this, before, after, QCoreApplication::translate("Ogt", "Move")));
}

void GraphScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return QGraphicsScene::mouseDoubleClickEvent(event);
    if (NeuronItem* item = NeuronAt(event->scenePos()))
    {
        emit NeuronActivated(item->Id());
        return;
    }
    if (items(event->scenePos()).isEmpty())
        emit EmptyDoubleClicked(event->scenePos(), event->screenPos());
}

} // namespace Ogt
