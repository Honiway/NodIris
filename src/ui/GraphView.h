/**
 * @file GraphView.h
 * @brief The canvas: zoom with the wheel, pan with the middle button or space, drop files.
 */

#ifndef OGT_GRAPH_VIEW_H
#define OGT_GRAPH_VIEW_H

#include <QGraphicsView>
#include <QPointer>
#include <QTimer>

class QLineEdit;

namespace Ogt
{

class GraphScene;
class PreviewPopup;

class GraphView final : public QGraphicsView
{
    Q_OBJECT

public:
    explicit GraphView(GraphScene* scene, QWidget* parent = nullptr);

    /// Frames every neuron, without zooming in past 100 %.
    void FitAll();
    void ZoomBy(qreal factor);
    /// Leaves the folder on screen for the one that holds it.
    void GoUp();
    void CenterOnNeuron(quint32 id);
    /// Edits the neuron title in a field over its label.
    void StartRename(quint32 id);
    QPointF VisibleCenter() const;
    qreal Zoom() const { return transform().m11(); }

signals:
    void FilesDropped(const QStringList& paths, QPointF scenePos);
    /// Sends the position of a right click after updating the selection.
    void ContextMenuRequested(QPointF scenePos, QPoint screenPos);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void TrackHover(QPoint viewPos, Qt::MouseButtons buttons);
    void ShowPreview();
    void HidePreview();

    PreviewPopup* preview_;
    QTimer hoverTimer_;
    quint32 hoverId_ = 0;
    QPointer<QLineEdit> rename_;
    GraphScene* scene_;
    bool spaceHeld_ = false;
    bool panning_ = false;
    QPoint lastPan_;
};

} // namespace Ogt

#endif // OGT_GRAPH_VIEW_H
