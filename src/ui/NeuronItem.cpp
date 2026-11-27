/**
 * @file NeuronItem.cpp
 * @brief Round nodes, with a cheap path for when the view is zoomed far out.
 */

#include "ui/NeuronItem.h"

#include "core/Layout.h"
#include "core/ProjectFile.h"
#include "ui/Icons.h"
#include "ui/LinkItem.h"
#include "ui/Theme.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHash>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>
#include <QTextLayout>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Ogt
{

namespace
{

constexpr int kLabelGap = 8;
constexpr int kTitleLineHeight = 18;
constexpr int kMetaHeight = 16;
constexpr int kHandleRadius = 5;
constexpr int kIconSize = 22;

/// The title in at most two lines that fit under the node, the second one cut short if needed.
QStringList TitleLines(const QString& title)
{
    const QFont font = Theme::Font(Theme::kBody, true);
    QTextLayout layout(title, font);
    layout.beginLayout();
    QTextLine first = layout.createLine();
    if (!first.isValid())
    {
        layout.endLayout();
        return {};
    }
    first.setLineWidth(NeuronItem::kWidth);
    layout.endLayout();

    QStringList lines = {title.left(first.textLength()).trimmed()};
    const QString rest = title.mid(first.textLength()).trimmed();
    if (!rest.isEmpty())
        lines.append(QFontMetrics(font).elidedText(rest, Qt::ElideRight, NeuronItem::kWidth));
    return lines;
}

/// Draws each kind and color once, then keeps the pixmap in a cache.
QPixmap KindPixmap(NeuronKind kind, const QColor& color)
{
    static QHash<quint64, QPixmap> cache;
    const quint64 key = (quint64(kind) << 32) | color.rgba();
    auto cachedPixmap = cache.find(key);
    if (cachedPixmap == cache.end())
        cachedPixmap = cache.insert(key, MakeKindIcon(kind, color).pixmap(QSize(kIconSize, kIconSize), 2.0));
    return cachedPixmap.value();
}

QPixmap IconPixmap(NeuronIcon icon, NeuronKind kind, const QColor& color)
{
    if (icon == NeuronIcon::Automatic)
        return KindPixmap(kind, color);
    static QHash<quint64, QPixmap> cache;
    const quint64 key = (quint64(icon) << 32) | color.rgba();
    auto cachedPixmap = cache.find(key);
    if (cachedPixmap == cache.end())
        cachedPixmap = cache.insert(key, MakeNeuronIcon(icon, color).pixmap(QSize(kIconSize, kIconSize), 2.0));
    return cachedPixmap.value();
}

QString MetaFor(const Neuron& neuron, const QString& projectFile, int childCount, bool& missing)
{
    missing = false;
    switch (neuron.kind)
    {
    case NeuronKind::Note:
        return {};
    case NeuronKind::Task:
    {
        QString details = StatusName(neuron.status);
        if (neuron.due.isValid())
            details += QString::fromUtf8("  ·  ") + QLocale().toString(neuron.due, QLocale::ShortFormat);
        return details;
    }
    case NeuronKind::Link:
    {
        if (neuron.text.isEmpty())
            return QCoreApplication::translate("Ogt", "No address yet");
        const QUrl url = QUrl::fromUserInput(neuron.text);
        return url.host().isEmpty() ? neuron.text : url.host();
    }
    case NeuronKind::Folder:
        if (childCount == 0)
            return QCoreApplication::translate("Ogt", "Empty folder");
        return QCoreApplication::translate("Ogt", "%n neurons", nullptr, childCount);
    default:
    {
        const QString absolute = ResolveFromProject(projectFile, neuron.path);
        missing = absolute.isEmpty() || !QFileInfo::exists(absolute);
        return missing ? QCoreApplication::translate("Ogt", "File not found") : QFileInfo(neuron.path).fileName();
    }
    }
}

} // namespace

NeuronItem::NeuronItem(const Neuron& neuron, const QString& projectFile, int childCount)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setZValue(1);
    SetNeuron(neuron, projectFile, childCount);
}

QSize NeuronItem::SizeFor(const Neuron& neuron)
{
    const QString title = neuron.title.isEmpty() ? KindName(neuron.kind) : neuron.title;
    int height = kTop + 2 * kRadius + kLabelGap + int(TitleLines(title).size()) * kTitleLineHeight;
    // Notes have no detail line; every other kind does.
    if (neuron.kind != NeuronKind::Note)
        height += kMetaHeight;
    return {kWidth, SnapToGrid(height + kGridStep)};
}

void NeuronItem::SetNeuron(const Neuron& neuron, const QString& projectFile, int childCount)
{
    prepareGeometryChange();
    id_ = neuron.id;
    kind_ = neuron.kind;
    icon_ = neuron.icon;
    status_ = neuron.status;
    titleLines_ = TitleLines(neuron.title.isEmpty() ? KindName(neuron.kind) : neuron.title);
    meta_ = MetaFor(neuron, projectFile, childCount, missing_);

    QByteArray imageData;
    if (!neuron.iconImage.isEmpty())
        imageData = neuron.iconImage;
    else if (neuron.icon == NeuronIcon::Automatic && !neuron.thumbnail.isEmpty())
        imageData = neuron.thumbnail;
    if (imageData_ != imageData)
    {
        imageData_ = imageData;
        thumbnail_ = QPixmap();
        if (!imageData_.isEmpty())
            thumbnail_.loadFromData(imageData_);
    }

    size_ = SizeFor(neuron);
    setPos(neuron.position);
    for (LinkItem* link : links_)
        link->Adjust();
    update();
}

std::array<QRectF, 4> NeuronItem::HandleRects() const
{
    const QPointF center = CircleCenter();
    const QSizeF size(2 * kHandleRadius, 2 * kHandleRadius);
    auto makeHandle = [size](QPointF centerPoint)
    { return QRectF(centerPoint - QPointF(kHandleRadius, kHandleRadius), size); };
    return {makeHandle(center + QPointF(kRadius, 0)), makeHandle(center + QPointF(0, kRadius)),
            makeHandle(center - QPointF(kRadius, 0)), makeHandle(center - QPointF(0, kRadius))};
}

std::optional<QPointF> NeuronItem::HandleCenterAt(QPointF itemPos) const
{
    for (const QRectF& handle : HandleRects())
    {
        if (handle.adjusted(-3, -3, 3, 3).contains(itemPos))
            return handle.center();
    }
    return std::nullopt;
}

bool NeuronItem::CircleContains(QPointF itemPos) const
{
    return QLineF(itemPos, CircleCenter()).length() <= kRadius + 2;
}

void NeuronItem::SetDimmed(bool dimmed)
{
    if (dimmed_ == dimmed)
        return;
    dimmed_ = dimmed;
    update();
}

void NeuronItem::SetDropTarget(bool isTarget)
{
    if (dropTarget_ == isTarget)
        return;
    dropTarget_ = isTarget;
    update();
}

void NeuronItem::Detach(LinkItem* link)
{
    std::erase(links_, link);
}

QRectF NeuronItem::boundingRect() const
{
    return QRectF(QPointF(0, 0), size_).adjusted(-2, -2, 2, 2);
}

QPainterPath NeuronItem::shape() const
{
    // The circle and the title can be grabbed; the empty corners around the circle cannot.
    QPainterPath path;
    path.addEllipse(CircleCenter(), kRadius + 2, kRadius + 2);
    for (const QRectF& handle : HandleRects())
        path.addEllipse(handle.adjusted(-3, -3, 3, 3));
    const qreal labelTop = kTop + 2 * kRadius + 4;
    path.addRect(QRectF(0, labelTop, kWidth, size_.height() - labelTop));
    return path;
}

void NeuronItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    const ThemeTokens& theme = Theme::Get().Tokens();
    const QColor kindColor = Theme::Get().KindColor(kind_);
    const qreal detail = option->levelOfDetailFromTransform(painter->worldTransform());
    const QPointF center = CircleCenter();

    if (dimmed_)
        painter->setOpacity(0.28);
    painter->setRenderHint(QPainter::Antialiasing);

    // Draw only the colored circle when the view is far away.
    if (detail < 0.4)
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(kindColor);
        painter->drawEllipse(center, kRadius, kRadius);
        return;
    }

    if (isSelected() || dropTarget_)
    {
        painter->setPen(QPen(theme.accent, 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(center, kRadius + 4, kRadius + 4);
    }

    QColor border = hovered_ ? theme.textMuted : theme.hairline;
    if (missing_)
        border = theme.danger;
    painter->setPen(QPen(border, 1));
    painter->setBrush(theme.surface);
    painter->drawEllipse(center, kRadius - 0.5, kRadius - 0.5);

    if (!thumbnail_.isNull())
    {
        // Crop the image to fill the circle without changing its shape.
        QPainterPath clip;
        clip.addEllipse(center, kRadius - 1.5, kRadius - 1.5);
        const qreal side = std::min(thumbnail_.width(), thumbnail_.height());
        const QRectF source((thumbnail_.width() - side) / 2, (thumbnail_.height() - side) / 2, side, side);
        const QRectF imageArea(center - QPointF(kRadius, kRadius), QSizeF(2 * kRadius, 2 * kRadius));
        painter->save();
        painter->setClipPath(clip);
        painter->drawPixmap(imageArea, thumbnail_, source);
        painter->restore();
    }
    else
    {
        if (kind_ == NeuronKind::Task && status_ == TaskStatus::Done)
        {
            QColor wash = kindColor;
            wash.setAlpha(36);
            painter->setPen(Qt::NoPen);
            painter->setBrush(wash);
            painter->drawEllipse(center, kRadius - 1.5, kRadius - 1.5);
        }
        // Draw an inner ring for folders.
        if (kind_ == NeuronKind::Folder)
        {
            painter->setPen(QPen(theme.hairline, 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(center, kRadius - 5, kRadius - 5);
        }
        const QRectF iconRect(center - QPointF(kIconSize / 2.0, kIconSize / 2.0), QSizeF(kIconSize, kIconSize));
        painter->drawPixmap(iconRect, IconPixmap(icon_, kind_, kindColor), QRectF());
    }

    // Draw a small status dot for a task in progress.
    if (kind_ == NeuronKind::Task && status_ == TaskStatus::Doing)
    {
        const qreal angle = -std::numbers::pi / 4;
        const QPointF rim = center + QPointF(std::cos(angle), std::sin(angle)) * kRadius;
        painter->setPen(QPen(theme.surface, 2));
        painter->setBrush(kindColor);
        painter->drawEllipse(rim, 5, 5);
    }

    qreal y = kTop + 2 * kRadius + kLabelGap;
    const QFont titleFont = Theme::Font(Theme::kBody, true);
    const QFont metaFont = Theme::Font(Theme::kSmall);

    // Draw the canvas behind the label so links do not cross the text.
    qreal labelWidth = 0;
    for (const QString& line : titleLines_)
        labelWidth = std::max<qreal>(labelWidth, QFontMetrics(titleFont).horizontalAdvance(line));
    if (!meta_.isEmpty())
        labelWidth = std::max<qreal>(labelWidth, std::min(kWidth, QFontMetrics(metaFont).horizontalAdvance(meta_)));
    const qreal labelHeight = titleLines_.size() * kTitleLineHeight + (meta_.isEmpty() ? 0 : kMetaHeight);
    painter->setPen(Qt::NoPen);
    painter->setBrush(theme.canvas);
    painter->drawRoundedRect(QRectF((kWidth - labelWidth) / 2 - 4, y, labelWidth + 8, labelHeight), 4, 4);

    painter->setFont(titleFont);
    const bool done = kind_ == NeuronKind::Task && status_ == TaskStatus::Done;
    painter->setPen(done ? theme.textMuted : theme.text);
    for (const QString& line : titleLines_)
    {
        painter->drawText(QRectF(0, y, kWidth, kTitleLineHeight), Qt::AlignHCenter | Qt::AlignVCenter, line);
        y += kTitleLineHeight;
    }

    if (!meta_.isEmpty())
    {
        painter->setFont(metaFont);
        QColor metaColor = theme.textMuted;
        if (missing_)
            metaColor = theme.danger;
        else if (kind_ == NeuronKind::Task && status_ != TaskStatus::Todo)
            metaColor = Theme::Get().StatusColor(status_);
        painter->setPen(metaColor);
        painter->drawText(QRectF(0, y, kWidth, kMetaHeight), Qt::AlignHCenter | Qt::AlignVCenter,
                          QFontMetrics(metaFont).elidedText(meta_, Qt::ElideMiddle, kWidth));
    }

    if (hovered_ || isSelected())
    {
        painter->setPen(QPen(theme.accent, 1.5));
        painter->setBrush(theme.surface);
        for (const QRectF& handle : HandleRects())
            painter->drawEllipse(handle.adjusted(0.75, 0.75, -0.75, -0.75));
    }
}

QVariant NeuronItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionChange)
        return QPointF(SnapToGrid(value.toPointF().toPoint()));
    if (change == ItemPositionHasChanged)
    {
        for (LinkItem* link : links_)
            link->Adjust();
    }
    return QGraphicsItem::itemChange(change, value);
}

void NeuronItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    hovered_ = true;
    update();
    QGraphicsItem::hoverEnterEvent(event);
}

void NeuronItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    hovered_ = false;
    update();
    QGraphicsItem::hoverLeaveEvent(event);
}

} // namespace Ogt
