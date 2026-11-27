/**
 * @file NeuronItem.h
 * @brief Draws one round neuron and its text on the canvas.
 */

#ifndef OGT_NEURON_ITEM_H
#define OGT_NEURON_ITEM_H

#include "core/Graph.h"

#include <QGraphicsItem>
#include <QPixmap>

#include <array>
#include <optional>
#include <vector>

namespace Ogt
{

class LinkItem;

class NeuronItem final : public QGraphicsItem
{
public:
    enum { Type = UserType + 1 };

    /// Width of the label under the circle, and so of the whole item.
    static constexpr int kWidth = 160;
    static constexpr int kRadius = 28;

    NeuronItem(const Neuron& neuron, const QString& projectFile, int childCount = 0);

    /// Refreshes the node and checks whether its file still exists.
    void SetNeuron(const Neuron& neuron, const QString& projectFile, int childCount = 0);
    quint32 Id() const { return id_; }
    NeuronKind Kind() const { return kind_; }

    /// Centre of the circle, in item and in scene coordinates.
    static QPointF CircleCenter() { return {kWidth / 2.0, kTop + kRadius}; }
    QPointF SceneCenter() const { return mapToScene(CircleCenter()); }
    /// The four dots around the circle that can start a new link when dragged.
    std::array<QRectF, 4> HandleRects() const;
    /// Centre of the handle under @p itemPos, when there is one.
    std::optional<QPointF> HandleCenterAt(QPointF itemPos) const;
    bool CircleContains(QPointF itemPos) const;

    void SetDimmed(bool dimmed);
    /// Rings the node while something is dragged over it, to show it will take it in.
    void SetDropTarget(bool isTarget);

    void Attach(LinkItem* link) { links_.push_back(link); }
    void Detach(LinkItem* link);

    /// Returns the size of a neuron before its canvas item exists.
    static QSize SizeFor(const Neuron& neuron);

    int type() const override { return Type; }
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    // Room above the circle for the selection ring.
    static constexpr int kTop = 6;

    quint32 id_ = 0;
    NeuronKind kind_ = NeuronKind::Note;
    NeuronIcon icon_ = NeuronIcon::Automatic;
    TaskStatus status_ = TaskStatus::Todo;
    QStringList titleLines_;
    QString meta_;
    bool missing_ = false;
    QByteArray imageData_;
    QPixmap thumbnail_;
    QSizeF size_;
    bool hovered_ = false;
    bool dimmed_ = false;
    bool dropTarget_ = false;
    std::vector<LinkItem*> links_;
};

} // namespace Ogt

#endif // OGT_NEURON_ITEM_H
