/**
 * @file Icons.cpp
 * @brief Loads and recolours the Papicons used by the interface.
 */

#include "ui/Icons.h"

#include <QPainter>
#include <QPixmap>

void InitPapicons()
{
    Q_INIT_RESOURCE(papicons);
}

namespace Ogt
{

namespace
{

QIcon Papicon(const QString& name, const QColor& color)
{
    static const bool ready = []
    {
        InitPapicons();
        return true;
    }();
    Q_UNUSED(ready);

    const QIcon source(QString(":/papicons/%1.svg").arg(name));
    QIcon icon;
    for (int scale : {1, 2})
    {
        QPixmap pixmap(16 * scale, 16 * scale);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        source.paint(&painter, pixmap.rect());
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), color);
        painter.end();
        pixmap.setDevicePixelRatio(scale);
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace

QIcon MakeIcon(IconId id, const QColor& color)
{
    switch (id)
    {
    case IconId::Add: return Papicon("plus", color);
    case IconId::Connect: return Papicon("link", color);
    case IconId::ConnectMany: return Papicon("tasks", color);
    case IconId::Search: return Papicon("search", color);
    case IconId::Theme: return Papicon("color-theme", color);
    case IconId::More: return Papicon("menu", color);
    case IconId::Open: return Papicon("external-link", color);
    case IconId::Relocate: return Papicon("folder", color);
    case IconId::Back: return Papicon("chevron-left", color);
    case IconId::Undo: return Papicon("arrow-left", color);
    case IconId::Redo: return Papicon("arrow-right", color);
    }
    return {};
}

QIcon MakeKindIcon(NeuronKind kind, const QColor& color)
{
    switch (kind)
    {
    case NeuronKind::Note: return Papicon("list", color);
    case NeuronKind::Task: return Papicon("tasks", color);
    case NeuronKind::Link: return Papicon("globe", color);
    case NeuronKind::Image: return Papicon("gallery", color);
    case NeuronKind::Document: return Papicon("paper", color);
    case NeuronKind::Sheet: return Papicon("grid", color);
    case NeuronKind::Pdf: return Papicon("newspaper", color);
    case NeuronKind::File: return Papicon("archive", color);
    case NeuronKind::Folder: return Papicon("folder", color);
    }
    return {};
}

QIcon MakeNeuronIcon(NeuronIcon icon, const QColor& color)
{
    switch (icon)
    {
    case NeuronIcon::Star: return Papicon("star", color);
    case NeuronIcon::Idea: return Papicon("sparkles", color);
    case NeuronIcon::Person: return Papicon("user", color);
    case NeuronIcon::Flag: return Papicon("bookmark", color);
    case NeuronIcon::Home: return Papicon("home", color);
    case NeuronIcon::Heart: return Papicon("heart", color);
    case NeuronIcon::Automatic: return {};
    }
    return {};
}

QIcon MakeAppIcon()
{
    return QIcon(":/packaging/icons/nodiris.png");
}

} // namespace Ogt
