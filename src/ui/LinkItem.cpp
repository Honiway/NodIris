/**
 * @file LinkItem.cpp
 * @brief Draws a straight or curved link between two neurons.
 */

#include "ui/LinkItem.h"

#include "ui/NeuronItem.h"
#include "ui/Theme.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

#include <algorithm>

namespace Ogt
{

namespace
{

// Space left between the rim and the line, so the arrow head does not touch the node.
constexpr qreal kRimGap = 4;

} // namespace

LinkItem::LinkItem(NeuronItem* from, NeuronItem* to, const Link& link)
    : from_(from)
    , to_(to)
    , link_(link)
{
    setFlags(ItemIsSelectable);
    // Under the nodes, so a line never crosses a title.
    setZValue(0);
    from_->Attach(this);
    to_->Attach(this);
    Adjust();
}

LinkItem::~LinkItem()
{
    from_->Detach(this);
    to_->Detach(this);
}

void LinkItem::SetLink(const Link& link)
{
    prepareGeometryChange();
    link_ = link;
    update();
}

void LinkItem::SetJoinGroup(const std::vector<LinkItem*>& links, bool drawsFinalPart)
{
    prepareGeometryChange();
    joinedLinks_ = links;
    drawsFinalPart_ = drawsFinalPart;
    update();
}

void LinkItem::ClearJoinGroup()
{
    prepareGeometryChange();
    joinedLinks_.clear();
    drawsFinalPart_ = false;
    update();
}

QLineF LinkItem::Between(QPointF from, qreal fromRadius, QPointF to, qreal toRadius)
{
    const QLineF full(from, to);
    const qreal length = full.length();
    if (length <= fromRadius + toRadius + 2 * kRimGap)
        return {from, from};
    const QPointF unit = (to - from) / length;
    return {from + unit * (fromRadius + kRimGap), to - unit * (toRadius + kRimGap)};
}

void LinkItem::Adjust()
{
    const std::vector<LinkItem*> links = joinedLinks_.size() >= 2
                                             ? joinedLinks_ : std::vector<LinkItem*>{this};
    for (LinkItem* link : links)
    {
        link->prepareGeometryChange();
        link->line_ = Between(link->from_->SceneCenter(), NeuronItem::kRadius,
                              link->to_->SceneCenter(), NeuronItem::kRadius);
        link->update();
    }
}

QRectF LinkItem::boundingRect() const
{
    const QPainterPath path = Path();
    QRectF bounds = path.controlPointRect().adjusted(-10, -10, 10, 10);
    if (!link_.label.isEmpty())
        bounds |= QRectF(path.pointAtPercent(0.5) - QPointF(110, 12), QSizeF(220, 24));
    return bounds;
}

QPainterPath LinkItem::shape() const
{
    // Wider than the drawn line, so a 1 px line can still be clicked.
    QPainterPathStroker stroker;
    stroker.setWidth(10);
    return stroker.createStroke(Path());
}

QPainterPath LinkItem::Path() const
{
    if (joinedLinks_.size() >= 2)
    {
        const QPointF joinPoint = JoinPoint();
        const QLineF branch = Between(from_->SceneCenter(), NeuronItem::kRadius, joinPoint, 0);
        QPainterPath joinedPath(branch.p1());
        joinedPath.lineTo(branch.p2());
        if (drawsFinalPart_)
        {
            const QLineF finalLine = FinalLine();
            joinedPath.moveTo(finalLine.p1());
            joinedPath.lineTo(finalLine.p2());
        }
        return joinedPath;
    }

    QPainterPath path(line_.p1());
    if (Theme::Get().Appearance().linkStyle == LinkStyle::Straight || line_.length() < 80)
    {
        path.lineTo(line_.p2());
        return path;
    }

    // A small curve makes nearby links easier to see.
    const QPointF delta = line_.p2() - line_.p1();
    const QPointF normal(-delta.y() / line_.length(), delta.x() / line_.length());
    const qreal bow = std::clamp(line_.length() * 0.10, 12.0, 42.0);
    path.cubicTo(line_.p1() + delta * 0.32 + normal * bow,
                 line_.p2() - delta * 0.32 + normal * bow, line_.p2());
    return path;
}

QPointF LinkItem::JoinPoint() const
{
    QPointF sourceCenter;
    for (const LinkItem* link : joinedLinks_)
        sourceCenter += link->from_->SceneCenter();
    sourceCenter /= qreal(joinedLinks_.size());

    const QPointF targetCenter = to_->SceneCenter();
    QPointF direction = targetCenter - sourceCenter;
    qreal distance = QLineF(sourceCenter, targetCenter).length();
    if (distance < 1)
    {
        direction = QPointF(1, 0);
        distance = 1;
    }
    const QPointF unit = direction / distance;
    const qreal finalPartLength = std::clamp(distance * 0.28, 48.0, 96.0);
    return targetCenter - unit * (NeuronItem::kRadius + kRimGap + finalPartLength);
}

QLineF LinkItem::FinalLine() const
{
    return Between(JoinPoint(), 0, to_->SceneCenter(), NeuronItem::kRadius);
}

void LinkItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    if (line_.length() < 1)
        return;
    const ThemeTokens& theme = Theme::Get().Tokens();
    const QColor color = isSelected() ? theme.accent : theme.link;
    const QPainterPath path = Path();
    painter->setRenderHint(QPainter::Antialiasing);

    const qreal width = Theme::Get().Appearance().linkWidth;
    QPen pen(color, isSelected() ? width + 0.75 : width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    if (link_.kind == LinkKind::PartOf)
        pen.setDashPattern({3, 3});
    painter->setPen(pen);
    painter->drawPath(path);

    const bool joinedArrow = joinedLinks_.size() >= 2 && drawsFinalPart_;
    if (joinedArrow)
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawEllipse(JoinPoint(), 3, 3);
    }
    if (link_.kind == LinkKind::LeadsTo || joinedArrow)
    {
        const QLineF arrowLine = joinedArrow ? FinalLine() : line_;
        if (arrowLine.length() < 1)
            return;
        const QPointF direction = (arrowLine.p2() - arrowLine.p1()) / arrowLine.length();
        const QPointF side(-direction.y(), direction.x());
        const QPointF tip = arrowLine.p2();
        const QPointF back = tip - direction * 8;
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawPolygon(QPolygonF({tip, back + side * 4, back - side * 4}));
    }

    if (!link_.label.isEmpty())
    {
        const QFont font = Theme::Font(Theme::kSmall);
        const QFontMetrics metrics(font);
        const QString text = metrics.elidedText(link_.label, Qt::ElideRight, 200);
        const qreal width = metrics.horizontalAdvance(text) + 12;
        const QRectF box(path.pointAtPercent(0.5) - QPointF(width / 2, 10), QSizeF(width, 20));
        painter->setPen(QPen(theme.hairline, 1));
        painter->setBrush(theme.canvas);
        painter->drawRoundedRect(box, 6, 6);
        painter->setFont(font);
        painter->setPen(isSelected() ? theme.accent : theme.textSecondary);
        painter->drawText(box, Qt::AlignCenter, text);
    }
}

} // namespace Ogt
