/**
 * @file Graph.cpp
 * @brief Neurons kept sorted by id, links kept in creation order.
 */

#include "core/Graph.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <algorithm>

namespace Ogt
{

QString LinkKindName(LinkKind kind)
{
    switch (kind)
    {
    case LinkKind::Relates: return QCoreApplication::translate("Ogt", "Related to");
    case LinkKind::LeadsTo: return QCoreApplication::translate("Ogt", "Leads to");
    case LinkKind::PartOf: return QCoreApplication::translate("Ogt", "Part of");
    }
    return {};
}

namespace
{

auto LowerBound(std::vector<Neuron>& neurons, quint32 id)
{
    return std::lower_bound(neurons.begin(), neurons.end(), id,
                            [](const Neuron& neuron, quint32 value) { return neuron.id < value; });
}

auto LowerBound(const std::vector<Neuron>& neurons, quint32 id)
{
    return std::lower_bound(neurons.begin(), neurons.end(), id,
                            [](const Neuron& neuron, quint32 value) { return neuron.id < value; });
}

} // namespace

quint32 Graph::AddNeuron(Neuron neuron)
{
    if (neuron.id == 0)
        neuron.id = nextId_;
    nextId_ = std::max(nextId_, neuron.id + 1);

    // Undo can restore an old id, so the new neuron may go in the middle of the list.
    auto neuronPosition = LowerBound(neurons_, neuron.id);
    if (neuronPosition != neurons_.end() && neuronPosition->id == neuron.id)
        *neuronPosition = std::move(neuron);
    else
        neuronPosition = neurons_.insert(neuronPosition, std::move(neuron));
    return neuronPosition->id;
}

bool Graph::RemoveNeuron(quint32 id)
{
    auto neuronPosition = LowerBound(neurons_, id);
    if (neuronPosition == neurons_.end() || neuronPosition->id != id)
        return false;
    neurons_.erase(neuronPosition);
    std::erase_if(links_, [id](const Link& link) { return link.from == id || link.to == id; });
    return true;
}

Neuron* Graph::FindNeuron(quint32 id)
{
    auto neuronPosition = LowerBound(neurons_, id);
    return neuronPosition != neurons_.end() && neuronPosition->id == id ? &*neuronPosition : nullptr;
}

const Neuron* Graph::FindNeuron(quint32 id) const
{
    auto neuronPosition = LowerBound(neurons_, id);
    return neuronPosition != neurons_.end() && neuronPosition->id == id ? &*neuronPosition : nullptr;
}

std::vector<quint32> Graph::Children(quint32 folder) const
{
    std::vector<quint32> result;
    for (const Neuron& neuron : neurons_)
    {
        if (neuron.parent == folder)
            result.push_back(neuron.id);
    }
    return result;
}

std::vector<quint32> Graph::Descendants(quint32 folder) const
{
    std::vector<quint32> result = Children(folder);
    // Add one folder level at a time.
    for (size_t i = 0; i < result.size(); ++i)
    {
        const Neuron* neuron = FindNeuron(result[i]);
        if (neuron && neuron->kind == NeuronKind::Folder)
        {
            const std::vector<quint32> childIds = Children(neuron->id);
            result.insert(result.end(), childIds.begin(), childIds.end());
        }
    }
    return result;
}

bool Graph::IsInside(quint32 id, quint32 folder) const
{
    // Stop after the number of neurons to avoid a loop in a damaged file.
    const Neuron* neuron = FindNeuron(id);
    for (size_t steps = 0; neuron && steps <= neurons_.size(); ++steps)
    {
        if (neuron->parent == folder)
            return true;
        if (neuron->parent == 0)
            return false;
        neuron = FindNeuron(neuron->parent);
    }
    return false;
}

bool Graph::AddLink(const Link& link)
{
    const Neuron* sourceNeuron = FindNeuron(link.from);
    const Neuron* targetNeuron = FindNeuron(link.to);
    if (link.from == link.to || !sourceNeuron || !targetNeuron || sourceNeuron->parent != targetNeuron->parent)
        return false;
    if (FindLink(link.from, link.to) || FindLink(link.to, link.from))
        return false;
    links_.push_back(link);
    return true;
}

bool Graph::RemoveLink(quint32 from, quint32 to)
{
    return std::erase_if(links_, [from, to](const Link& link) { return link.from == from && link.to == to; }) > 0;
}

Link* Graph::FindLink(quint32 from, quint32 to)
{
    auto linkPosition = std::find_if(links_.begin(), links_.end(),
                                     [from, to](const Link& link) { return link.from == from && link.to == to; });
    return linkPosition != links_.end() ? &*linkPosition : nullptr;
}

const Link* Graph::FindLink(quint32 from, quint32 to) const
{
    return const_cast<Graph*>(this)->FindLink(from, to);
}

std::vector<Link> Graph::LinksOf(quint32 id) const
{
    std::vector<Link> result;
    for (const Link& link : links_)
    {
        if (link.from == id || link.to == id)
            result.push_back(link);
    }
    return result;
}

std::vector<quint32> Graph::Search(const QString& query) const
{
    std::vector<quint32> result;
    const QString needle = query.trimmed();
    if (needle.isEmpty())
        return result;

    for (const Neuron& neuron : neurons_)
    {
        if (neuron.title.contains(needle, Qt::CaseInsensitive) || neuron.text.contains(needle, Qt::CaseInsensitive)
            || QFileInfo(neuron.path).fileName().contains(needle, Qt::CaseInsensitive))
        {
            result.push_back(neuron.id);
        }
    }
    return result;
}

void Graph::Clear()
{
    neurons_.clear();
    links_.clear();
    nextId_ = 1;
}

} // namespace Ogt
