/**
 * @file ProjectFile.h
 * @brief Reads and writes compressed `.ogt` project files.
 */

#ifndef OGT_PROJECT_FILE_H
#define OGT_PROJECT_FILE_H

#include "core/Graph.h"

#include <QByteArray>
#include <QString>

namespace Ogt
{

inline constexpr char kProjectSuffix[] = "ogt";
inline constexpr quint8 kFormatVersion = 1;

QByteArray EncodeGraph(const Graph& graph);
/// Leaves @p graph untouched and fills @p error when the bytes are not a project.
bool DecodeGraph(const QByteArray& bytes, Graph& graph, QString& error);

/// Writes through a temporary file, so a crash mid-save never leaves half a project behind.
bool SaveProject(const QString& filePath, const Graph& graph, QString& error);
bool LoadProject(const QString& filePath, Graph& graph, QString& error);

/// Neuron paths are stored relative to the folder that holds the project file.
QString RelativeToProject(const QString& projectFile, const QString& absolutePath);
QString ResolveFromProject(const QString& projectFile, const QString& storedPath);

} // namespace Ogt

#endif // OGT_PROJECT_FILE_H
