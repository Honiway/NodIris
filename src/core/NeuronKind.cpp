/**
 * @file NeuronKind.cpp
 * @brief Finds neuron kinds from file extensions and returns their names.
 */

#include "core/NeuronKind.h"

#include <QCoreApplication>
#include <QFileInfo>

namespace Ogt
{

bool IsTextFile(const QString& path)
{
    static const QStringList suffixes = {"txt", "md", "csv", "tsv", "json", "log", "ini", "yaml", "yml"};
    return suffixes.contains(QFileInfo(path).suffix().toLower());
}

bool IsFileKind(NeuronKind kind)
{
    return kind == NeuronKind::Image || kind == NeuronKind::Document || kind == NeuronKind::Sheet
        || kind == NeuronKind::Pdf || kind == NeuronKind::File;
}

NeuronKind KindForPath(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();

    static const QStringList images = {"png", "jpg", "jpeg", "gif", "bmp", "webp", "svg", "tif", "tiff"};
    static const QStringList documents = {"doc", "docx", "odt", "rtf", "txt", "md", "pages"};
    static const QStringList sheets = {"xls", "xlsx", "xlsm", "ods", "csv", "tsv", "numbers"};

    if (images.contains(suffix))
        return NeuronKind::Image;
    if (documents.contains(suffix))
        return NeuronKind::Document;
    if (sheets.contains(suffix))
        return NeuronKind::Sheet;
    if (suffix == "pdf")
        return NeuronKind::Pdf;
    return NeuronKind::File;
}

QString KindName(NeuronKind kind)
{
    switch (kind)
    {
    case NeuronKind::Note: return QCoreApplication::translate("Ogt", "Note");
    case NeuronKind::Task: return QCoreApplication::translate("Ogt", "Task");
    case NeuronKind::Link: return QCoreApplication::translate("Ogt", "Web link");
    case NeuronKind::Image: return QCoreApplication::translate("Ogt", "Image");
    case NeuronKind::Document: return QCoreApplication::translate("Ogt", "Document");
    case NeuronKind::Sheet: return QCoreApplication::translate("Ogt", "Spreadsheet");
    case NeuronKind::Pdf: return QCoreApplication::translate("Ogt", "PDF");
    case NeuronKind::File: return QCoreApplication::translate("Ogt", "File");
    case NeuronKind::Folder: return QCoreApplication::translate("Ogt", "Folder");
    }
    return {};
}

QString StatusName(TaskStatus status)
{
    switch (status)
    {
    case TaskStatus::Todo: return QCoreApplication::translate("Ogt", "To do");
    case TaskStatus::Doing: return QCoreApplication::translate("Ogt", "In progress");
    case TaskStatus::Done: return QCoreApplication::translate("Ogt", "Done");
    }
    return {};
}

} // namespace Ogt
