/**
 * @file Icons.h
 * @brief Provides the Papicons used by the interface.
 */

#ifndef OGT_ICONS_H
#define OGT_ICONS_H

#include "core/Graph.h"

#include <QColor>
#include <QIcon>

namespace Ogt
{

enum class IconId
{
    Add,
    Connect,
    ConnectMany,
    Search,
    Theme,
    More,
    Open,
    Relocate,
    Back,
    Undo,
    Redo,
};

QIcon MakeIcon(IconId id, const QColor& color);
QIcon MakeKindIcon(NeuronKind kind, const QColor& color);
QIcon MakeNeuronIcon(NeuronIcon icon, const QColor& color);
/// Three linked dots on a paper tile, for the window and the taskbar.
QIcon MakeAppIcon();

} // namespace Ogt

#endif // OGT_ICONS_H
