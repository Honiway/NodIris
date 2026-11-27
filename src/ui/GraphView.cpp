/**
 * @file GraphView.cpp
 * @brief Manages zoom, scrolling, previews and file drops on the canvas.
 */

#include "ui/GraphView.h"

#include "ui/GraphScene.h"
#include "ui/LinkItem.h"
#include "ui/NeuronItem.h"
#include "ui/PreviewPopup.h"
#include "ui/Theme.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace Ogt
{

namespace
{

constexpr qreal kMinZoom = 0.15;
constexpr qreal kMaxZoom = 2.5;
// Wait before showing a preview to avoid opening one while the mouse is moving.
constexpr int kHoverDelayMs = 450;

QStringList LocalFiles(const QMimeData* mime)
{
    QStringList paths;
    for (const QUrl& url : mime->urls())
    {
        if (url.isLocalFile())
            paths.append(url.toLocalFile());
    }
    return paths;
}

} // namespace

GraphView::GraphView(GraphScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent)
    , scene_(scene)
{
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    setFrameShape(QFrame::NoFrame);
    setDragMode(RubberBandDrag);
    setTransformationAnchor(AnchorUnderMouse);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Cache the background to avoid drawing the grid on every scroll step.
    setCacheMode(CacheBackground);
    setViewportUpdateMode(SmartViewportUpdate);
    setOptimizationFlag(DontAdjustForAntialiasing);
    setAcceptDrops(true);
    connect(&Theme::Get(), &Theme::Changed, this, [this] { resetCachedContent(); });

    preview_ = new PreviewPopup(viewport());
    hoverTimer_.setSingleShot(true);
    hoverTimer_.setInterval(kHoverDelayMs);
    connect(&hoverTimer_, &QTimer::timeout, this, &GraphView::ShowPreview);
    connect(scene_, &GraphScene::FolderShown, this, &GraphView::HidePreview);
    connect(scene_, &GraphScene::Changed, this, &GraphView::HidePreview);
}

void GraphView::TrackHover(QPoint viewPos, Qt::MouseButtons buttons)
{
    if (buttons != Qt::NoButton)
    {
        HidePreview();
        return;
    }
    NeuronItem* item = scene_->NeuronAt(mapToScene(viewPos));
    const quint32 id = item ? item->Id() : 0;
    if (id == hoverId_)
        return;
    preview_->Clear();
    hoverId_ = id;
    if (id != 0)
        hoverTimer_.start();
    else
        hoverTimer_.stop();
}

void GraphView::ShowPreview()
{
    NeuronItem* item = scene_->ItemFor(hoverId_);
    const Neuron* neuron = scene_->GetGraph().FindNeuron(hoverId_);
    if (!item || !neuron)
        return;
    preview_->SetNeuron(*neuron, scene_->GetGraph(), scene_->ProjectFile());

    // Show the preview on the side with enough space.
    const qreal reach = NeuronItem::kWidth / 2.0 * Zoom() + 8;
    const QPoint center = mapFromScene(item->SceneCenter());
    const QRect area = viewport()->rect().adjusted(8, 8, -8, -8);
    int x = center.x() + int(reach);
    if (x + preview_->width() > area.right())
        x = center.x() - int(reach) - preview_->width();
    x = std::clamp(x, area.left(), std::max(area.left(), area.right() - preview_->width()));
    const int y = std::clamp(center.y() - 24, area.top(), std::max(area.top(), area.bottom() - preview_->height()));
    preview_->move(x, y);
    preview_->show();
    preview_->raise();
}

void GraphView::HidePreview()
{
    hoverTimer_.stop();
    hoverId_ = 0;
    preview_->Clear();
}

void GraphView::contextMenuEvent(QContextMenuEvent* event)
{
    HidePreview();
    if (scene_->MultiLinkToolEnabled())
    {
        event->accept();
        return;
    }
    const QPointF scenePos = mapToScene(event->pos());
    QGraphicsItem* clickedItem = nullptr;
    for (QGraphicsItem* item : scene_->items(scenePos))
    {
        if (item->type() == NeuronItem::Type || item->type() == LinkItem::Type)
        {
            clickedItem = item;
            break;
        }
    }
    // A right click selects the item under the mouse.
    if (!clickedItem)
        scene_->clearSelection();
    else if (!clickedItem->isSelected())
    {
        scene_->clearSelection();
        clickedItem->setSelected(true);
    }
    emit ContextMenuRequested(scenePos, event->globalPos());
}

void GraphView::StartRename(quint32 id)
{
    NeuronItem* item = scene_->ItemFor(id);
    const Neuron* neuron = scene_->GetGraph().FindNeuron(id);
    if (!item || !neuron)
        return;
    delete rename_;
    CenterOnNeuron(id);

    auto* field = new QLineEdit(neuron->title, viewport());
    rename_ = field;
    field->setAlignment(Qt::AlignCenter);
    field->setFont(Theme::Font(Theme::kBody, true));
    const int width = std::max(160, int(NeuronItem::kWidth * Zoom()));
    const QPoint label = mapFromScene(item->mapToScene(QPointF(NeuronItem::kWidth / 2.0, 66)));
    field->setFixedWidth(width);
    field->move(label.x() - width / 2, label.y());
    field->selectAll();
    field->show();
    field->setFocus();

    // Enter or a click saves the name, while Escape cancels the change.
    auto finish = [this, field, id](bool keep)
    {
        if (rename_ != field)
            return;
        rename_ = nullptr;
        const Neuron* currentNeuron = scene_->GetGraph().FindNeuron(id);
        if (keep && currentNeuron && field->text().trimmed() != currentNeuron->title)
        {
            Neuron after = *currentNeuron;
            after.title = field->text().trimmed();
            scene_->EditNeuron(after);
        }
        field->deleteLater();
        setFocus();
    };
    connect(field, &QLineEdit::editingFinished, this, [finish] { finish(true); });
    auto* cancel = new QAction(field);
    cancel->setShortcut(Qt::Key_Escape);
    cancel->setShortcutContext(Qt::WidgetShortcut);
    field->addAction(cancel);
    connect(cancel, &QAction::triggered, this, [finish] { finish(false); });
    // Cancel the field when its neuron leaves the screen.
    connect(scene_, &GraphScene::FolderShown, field, [finish] { finish(false); });
}

void GraphView::leaveEvent(QEvent* event)
{
    HidePreview();
    QGraphicsView::leaveEvent(event);
}

void GraphView::FitAll()
{
    const QRectF content = scene_->ContentRect();
    if (content.isEmpty())
    {
        resetTransform();
        centerOn(0, 0);
        return;
    }
    fitInView(content.adjusted(-48, -48, 48, 48), Qt::KeepAspectRatio);
    const qreal zoom = std::clamp(Zoom(), kMinZoom, 1.0);
    setTransform(QTransform::fromScale(zoom, zoom));
    centerOn(content.center());
}

void GraphView::GoUp()
{
    const quint32 previousFolder = scene_->CurrentFolder();
    const Neuron* folder = scene_->GetGraph().FindNeuron(previousFolder);
    if (!folder)
        return;
    scene_->ShowFolder(folder->parent);
    // Select the folder after leaving it.
    FitAll();
    scene_->SelectOnly(previousFolder);
}

void GraphView::CenterOnNeuron(quint32 id)
{
    if (NeuronItem* item = scene_->ItemFor(id))
        centerOn(item);
}

QPointF GraphView::VisibleCenter() const
{
    return mapToScene(viewport()->rect().center());
}

void GraphView::ZoomBy(qreal factor)
{
    // AnchorUnderMouse keeps the same canvas point under the mouse.
    const qreal newZoom = std::clamp(Zoom() * factor, kMinZoom, kMaxZoom);
    if (qFuzzyCompare(newZoom, Zoom()))
        return;
    scale(newZoom / Zoom(), newZoom / Zoom());
    resetCachedContent();
}

void GraphView::wheelEvent(QWheelEvent* event)
{
    // A touchpad scrolls with two fingers and zooms with Ctrl or a pinch.
    const bool touchpad = !event->pixelDelta().isNull();
    if (touchpad && !(event->modifiers() & Qt::ControlModifier))
    {
        const QPoint delta = event->pixelDelta();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    HidePreview();
    // Save and close the rename field before zooming.
    if (rename_)
        setFocus();
    const qreal steps = event->angleDelta().y() / 120.0;
    ZoomBy(std::pow(1.15, steps));
    event->accept();
}

void GraphView::mousePressEvent(QMouseEvent* event)
{
    HidePreview();
    if (event->button() == Qt::MiddleButton || (spaceHeld_ && event->button() == Qt::LeftButton))
    {
        panning_ = true;
        lastPan_ = event->pos();
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void GraphView::mouseMoveEvent(QMouseEvent* event)
{
    if (panning_)
    {
        const QPoint delta = event->pos() - lastPan_;
        lastPan_ = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
    TrackHover(event->pos(), event->buttons());
}

void GraphView::mouseReleaseEvent(QMouseEvent* event)
{
    if (panning_)
    {
        panning_ = false;
        viewport()->setCursor(spaceHeld_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void GraphView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && (scene_->LinkToolEnabled() || scene_->MultiLinkToolEnabled()))
    {
        if (scene_->LinkToolEnabled())
            scene_->SetLinkToolEnabled(false);
        else
            scene_->SetMultiLinkToolEnabled(false);
        return;
    }
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat())
    {
        spaceHeld_ = true;
        viewport()->setCursor(Qt::OpenHandCursor);
        return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
        scene_->DeleteSelection();
        return;
    }
    if (event->key() == Qt::Key_F2)
    {
        if (const std::optional<quint32> id = scene_->SelectedNeuron())
            StartRename(*id);
        return;
    }
    if (event->key() == Qt::Key_Escape && scene_->CurrentFolder() != 0)
    {
        GoUp();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void GraphView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat())
    {
        spaceHeld_ = false;
        if (!panning_)
            viewport()->setCursor(Qt::ArrowCursor);
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void GraphView::dragEnterEvent(QDragEnterEvent* event)
{
    if (!LocalFiles(event->mimeData()).isEmpty())
        event->acceptProposedAction();
}

void GraphView::dragMoveEvent(QDragMoveEvent* event)
{
    if (!LocalFiles(event->mimeData()).isEmpty())
        event->acceptProposedAction();
}

void GraphView::dropEvent(QDropEvent* event)
{
    const QStringList paths = LocalFiles(event->mimeData());
    if (paths.isEmpty())
        return;
    event->acceptProposedAction();
    emit FilesDropped(paths, mapToScene(event->position().toPoint()));
}

} // namespace Ogt
