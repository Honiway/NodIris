/**
 * @file LinkItem.h
 * @brief A link drawn between two nodes: solid, dashed for "part of", an arrow for "leads to".
 */

#ifndef OGT_LINK_ITEM_H
#define OGT_LINK_ITEM_H

#include "core/Graph.h"

#include <QGraphicsItem>
#include <QLineF>

#include <vector>

class QPainterPath;

namespace Ogt
{

class NeuronItem;

class LinkItem final : public QGraphicsItem
{
public:
    enum { Type = UserType + 2 };

    LinkItem(NeuronItem* from, NeuronItem* to, const Link& link);
    ~LinkItem() override;

    const Link& GetLink() const { return link_; }
    void SetLink(const Link& link);
    void SetJoinGroup(const std::vector<LinkItem*>& links, bool drawsFinalPart);
    void ClearJoinGroup();
    /// Follows the two nodes after one of them moved.
    void Adjust();

    /// The straight segment between two circles, from rim to rim. Also used for the link being dragged out.
    static QLineF Between(QPointF from, qreal fromRadius, QPointF to, qreal toRadius);

    int type() const override { return Type; }
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    QPainterPath Path() const;
    QPointF JoinPoint() const;
    QLineF FinalLine() const;
    NeuronItem* from_;
    NeuronItem* to_;
    Link link_;
    QLineF line_;
    std::vector<LinkItem*> joinedLinks_;
    bool drawsFinalPart_ = false;
};

} // namespace Ogt

#endif // OGT_LINK_ITEM_H
