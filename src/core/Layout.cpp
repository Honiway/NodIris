/**
 * @file Layout.cpp
 * @brief Aligns positions to the canvas grid.
 */

#include "core/Layout.h"

#include <cmath>

namespace Ogt
{

int SnapToGrid(int value)
{
    return int(std::lround(double(value) / kGridStep)) * kGridStep;
}

QPoint SnapToGrid(QPoint point)
{
    return {SnapToGrid(point.x()), SnapToGrid(point.y())};
}

} // namespace Ogt
