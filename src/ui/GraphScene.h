/**
 * @file GraphScene.h
 * @brief Keeps the open graph, its visible items and its undo history in sync.
 */

#ifndef OGT_GRAPH_SCENE_H
#define OGT_GRAPH_SCENE_H

#include "core/Graph.h"

#include <QGraphicsScene>
#include <QHash>
#include <QUndoStack>

#include <optional>

class QGraphicsLineItem;

namespace Ogt
{

class LinkItem;
class NeuronItem;

class GraphScene final : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit GraphScene(QObject* parent = nullptr);
    ~GraphScene() override;

    /// Replaces everything, shows the top level and starts a fresh undo history.
    void Load(Graph graph, const QString& projectFile);
    const Graph& GetGraph() const { return graph_; }
    const QString& ProjectFile() const { return projectFile_; }
    QUndoStack* UndoStack() { return &undo_; }

    /// Puts the content of @p folder on the canvas, 0 for the top level.
    void ShowFolder(quint32 folder);
    quint32 CurrentFolder() const { return folder_; }
    /// The folders from the top level down to the one on screen, outermost first.
    std::vector<quint32> FolderPath() const;

    // Raw edits, called by the undo commands.
    void InsertNeuron(const Neuron& neuron);
    void EraseNeuron(quint32 id);
    void ReplaceNeuron(const Neuron& neuron);
    void PlaceNeurons(const QHash<quint32, QPoint>& positions);
    void InsertLink(const Link& link);
    void EraseLink(quint32 from, quint32 to);
    void ReplaceLink(const Link& link);

    // What a person does. Each one is a single step in the undo history.
    /// Adds @p neuron to the folder on screen.
    quint32 AddNeuron(Neuron neuron);
    /// Turns files into neurons around @p scenePosition and creates image previews.
    void AddFiles(const QStringList& paths, QPointF scenePosition);
    void Connect(quint32 from, quint32 to);
    /// Links every valid source to the same target as one undoable operation.
    void ConnectMany(const std::vector<quint32>& sources, quint32 target);
    /// Adds copies in the current folder as one undoable operation.
    void Paste(const std::vector<Neuron>& neurons, const std::vector<Link>& links, QPointF center);
    void EditNeuron(const Neuron& after);
    void EditLink(const Link& after);
    /// Deletes the selection and the content of selected folders.
    void DeleteSelection();
    /// Moves neurons into @p folder and removes links to neurons left outside.
    void MoveIntoFolder(const std::vector<quint32>& ids, quint32 folder);
    /// Fades neurons that do not match @p query.
    void SetFilter(const QString& query);
    /// Returns matches from both visible and closed folders.
    std::vector<quint32> Matches() const { return matches_; }

    /// The neuron or link selected on its own, if exactly one thing is selected.
    std::optional<quint32> SelectedNeuron() const;
    std::optional<Link> SelectedLink() const;
    void SelectOnly(quint32 id);
    NeuronItem* ItemFor(quint32 id) const { return items_.value(id); }
    NeuronItem* NeuronAt(QPointF scenePos) const;
    QRectF ContentRect() const;
    void SetLinkToolEnabled(bool enabled);
    bool LinkToolEnabled() const { return linkToolEnabled_; }
    void SetMultiLinkToolEnabled(bool enabled);
    bool MultiLinkToolEnabled() const { return multiLinkToolEnabled_; }

signals:
    /// The graph changed: time to save.
    void Changed();
    void FolderShown(quint32 folder);
    void NeuronActivated(quint32 id);
    void EmptyDoubleClicked(QPointF scenePos, QPoint screenPos);
    void Feedback(const QString& message);
    void LinkToolEnabledChanged(bool enabled);
    void MultiLinkToolEnabledChanged(bool enabled);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
    void CreateItem(const Neuron& neuron);
    void CreateLinkItem(const Link& link);
    void RefreshFolderItem(quint32 folder);
    int ChildCount(quint32 folder) const;
    LinkItem* LinkItemFor(quint32 from, quint32 to) const;
    void UpdateLinkGroups();
    /// A folder under @p scenePos that is not part of what is being dragged.
    NeuronItem* FolderUnderDrag(QPointF scenePos) const;
    void ClearItems();
    void ApplyFilter();

    Graph graph_;
    QString projectFile_;
    QUndoStack undo_;
    quint32 folder_ = 0;
    QHash<quint32, NeuronItem*> items_;
    std::vector<LinkItem*> linkItems_;

    // Positions when the mouse went down, to turn a drag into one Move step.
    QHash<quint32, QPoint> pressPositions_;
    NeuronItem* dropTarget_ = nullptr;
    NeuronItem* linkSource_ = nullptr;
    QGraphicsLineItem* draftLink_ = nullptr;
    QPointF linkStart_;
    bool linkToolEnabled_ = false;
    bool multiLinkToolEnabled_ = false;
    std::vector<quint32> multiLinkSources_;

    QString filter_;
    std::vector<quint32> matches_;
};

} // namespace Ogt

#endif // OGT_GRAPH_SCENE_H
