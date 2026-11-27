/**
 * @file Layout.h
 * @brief Keeps neuron positions aligned to the canvas grid.
 */

#ifndef OGT_LAYOUT_H
#define OGT_LAYOUT_H

#include <QPoint>

namespace Ogt
{

/// The canvas snaps every neuron to this grid.
inline constexpr int kGridStep = 4;

int SnapToGrid(int value);
QPoint SnapToGrid(QPoint point);

} // namespace Ogt

#endif // OGT_LAYOUT_H
