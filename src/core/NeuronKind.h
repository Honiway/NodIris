/**
 * @file NeuronKind.h
 * @brief What a neuron holds, and how a dropped file picks one.
 */

#ifndef OGT_NEURON_KIND_H
#define OGT_NEURON_KIND_H

#include <QString>
#include <QtGlobal>

namespace Ogt
{

/// The values are written to disk: append new kinds, never reorder them.
enum class NeuronKind : quint8
{
    Note = 0,
    Task = 1,
    Link = 2,
    Image = 3,
    Document = 4,
    Sheet = 5,
    Pdf = 6,
    File = 7,
    /// Holds a graph whose children use this folder as their parent.
    Folder = 8,
};

inline constexpr int kNeuronKindCount = 9;

/// Also written to disk.
enum class TaskStatus : quint8
{
    Todo = 0,
    Doing = 1,
    Done = 2,
};

/// Returns true when a neuron stores a path to a file.
bool IsFileKind(NeuronKind kind);
/// Plain text the app can show and edit itself: .txt, .md, .csv and the like.
bool IsTextFile(const QString& path);

/// Returns the neuron kind for a file extension or File when it is unknown.
NeuronKind KindForPath(const QString& path);

/// Translated, for menus and the inspector.
QString KindName(NeuronKind kind);
QString StatusName(TaskStatus status);

} // namespace Ogt

#endif // OGT_NEURON_KIND_H
