/**
 * @file Thumbnail.cpp
 * @brief Reads images at a small size and saves them as JPEG or PNG data.
 */

#include "core/Thumbnail.h"

#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QPainter>

namespace Ogt
{

QByteArray MakeThumbnail(const QString& imagePath, int maxSide, bool preserveAlpha)
{
    QImageReader reader(imagePath);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid() && (size.width() > maxSide || size.height() > maxSide))
        reader.setScaledSize(size.scaled(maxSide, maxSide, Qt::KeepAspectRatio));

    QImage image = reader.read();
    if (image.isNull())
        return {};
    // Formats that cannot scale while decoding (GIF, some PNGs) come back full size.
    if (image.width() > maxSide || image.height() > maxSide)
        image = image.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // File previews use JPEG; custom icons keep useful transparent edges in PNG.
    const bool saveAlpha = preserveAlpha && image.hasAlphaChannel();
    if (image.hasAlphaChannel() && !saveAlpha)
    {
        QImage imageWithoutAlpha(image.size(), QImage::Format_RGB32);
        imageWithoutAlpha.fill(Qt::white);
        QPainter painter(&imageWithoutAlpha);
        painter.drawImage(0, 0, image);
        painter.end();
        image = imageWithoutAlpha;
    }

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, saveAlpha ? "PNG" : "JPG", saveAlpha ? -1 : 75))
    {
        bytes.clear();
        buffer.seek(0);
        image.save(&buffer, "PNG");
    }
    return bytes;
}

} // namespace Ogt
