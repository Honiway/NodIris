/**
 * @file Thumbnail.h
 * @brief The small preview an image neuron keeps inside the project.
 */

#ifndef OGT_THUMBNAIL_H
#define OGT_THUMBNAIL_H

#include <QByteArray>
#include <QString>

namespace Ogt
{

inline constexpr int kThumbnailSide = 160;
inline constexpr int kIconImageSide = 96;

/// Reads @p imagePath at a small size and returns compressed image data.
QByteArray MakeThumbnail(const QString& imagePath, int maxSide = kThumbnailSide, bool preserveAlpha = false);

} // namespace Ogt

#endif // OGT_THUMBNAIL_H
