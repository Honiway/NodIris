/**
 * @file Graph.h
 * @brief Stores neurons, folders and links without UI code.
 */

#ifndef OGT_GRAPH_H
#define OGT_GRAPH_H

#include "core/NeuronKind.h"

#include <QByteArray>
#include <QDate>
#include <QPoint>
#include <QString>

#include <vector>

namespace Ogt
{

/// Selects a custom icon or the automatic icon for a neuron.
enum class NeuronIcon : quint8
{
    Automatic = 0,
    Star = 1,
    Idea = 2,
    Person = 3,
    Flag = 4,
    Home = 5,
    Heart = 6,
};

struct Neuron
{
    quint32 id = 0;
    /// The folder this neuron lives in, 0 for the top level.
    quint32 parent = 0;
    NeuronKind kind = NeuronKind::Note;
    QString title;
    /// The body of a note or a task, or the address of a web link.
    QString text;
    /// For file kinds, relative to the project file so a project folder can move.
    QString path;
    TaskStatus status = TaskStatus::Todo;
    QDate due;
    QPoint position;
    /// A small JPEG used by image neurons.
    QByteArray thumbnail;
    NeuronIcon icon = NeuronIcon::Automatic;
    /// A resized and compressed image used instead of the built-in icon.
    QByteArray iconImage;

    bool operator==(const Neuron&) const = default;
};

/// Written to disk: append, never reorder.
enum class LinkKind : quint8
{
    Relates = 0,
    /// Marks a step that leads to the next step from left to right.
    LeadsTo = 1,
    PartOf = 2,
};

QString LinkKindName(LinkKind kind);

struct Link
{
    quint32 from = 0;
    quint32 to = 0;
    LinkKind kind = LinkKind::Relates;
    QString label;
    /// Groups branches that share one final arrow.
    quint32 joinGroup = 0;

    bool operator==(const Link&) const = default;
};

class Graph final
{
public:
    /// Gives the neuron a fresh id when it has none, and returns the id.
    quint32 AddNeuron(Neuron neuron);
    /// Removes the neuron and every link that touches it.
    bool RemoveNeuron(quint32 id);
    Neuron* FindNeuron(quint32 id);
    const Neuron* FindNeuron(quint32 id) const;
    const std::vector<Neuron>& Neurons() const { return neurons_; }

    /// Neurons directly inside @p folder (0 for the top level), in id order.
    std::vector<quint32> Children(quint32 folder) const;
    /// Returns everything inside @p folder with folders before their content.
    std::vector<quint32> Descendants(quint32 folder) const;
    /// True when @p id sits in @p folder, directly or deeper down.
    bool IsInside(quint32 id, quint32 folder) const;

    /// Adds a valid link when the same pair is not already linked.
    bool AddLink(const Link& link);
    bool RemoveLink(quint32 from, quint32 to);
    Link* FindLink(quint32 from, quint32 to);
    const Link* FindLink(quint32 from, quint32 to) const;
    const std::vector<Link>& Links() const { return links_; }
    std::vector<Link> LinksOf(quint32 id) const;

    /// Ids of the neurons whose title, text or file name contain @p query, ignoring case.
    std::vector<quint32> Search(const QString& query) const;

    quint32 NextId() const { return nextId_; }
    void SetNextId(quint32 id) { nextId_ = id; }
    bool IsEmpty() const { return neurons_.empty(); }
    void Clear();

private:
    // Kept sorted by id so lookups are a binary search and saves come out in a stable order.
    std::vector<Neuron> neurons_;
    std::vector<Link> links_;
    quint32 nextId_ = 1;
};

} // namespace Ogt

#endif // OGT_GRAPH_H
