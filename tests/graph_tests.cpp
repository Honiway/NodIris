/**
 * @file graph_tests.cpp
 * @brief Neurons and links: ids, refusals, removal, search.
 *
 * Checks that ids are handed out once, that an undo can put a neuron back
 * under its old id, that links to nowhere or twice between the same pair are
 * refused, that removing a neuron takes its links with it, and that search
 * looks at titles, text and file names.
 *
 * Run with `ctest -R graph_tests`. Exits non-zero on any failure.
 */

#include "core/Graph.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
int gFailures = 0;

void Check(bool condition, const std::string& what)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << what << '\n';
        ++gFailures;
    }
}

Ogt::Neuron Named(const QString& title)
{
    Ogt::Neuron neuron;
    neuron.title = title;
    return neuron;
}
} // namespace

int main()
{
    Ogt::Graph graph;
    const quint32 alphaId = graph.AddNeuron(Named("Alpha"));
    const quint32 betaId = graph.AddNeuron(Named("Beta"));
    const quint32 gammaId = graph.AddNeuron(Named("Gamma"));
    Check(alphaId == 1 && betaId == 2 && gammaId == 3, "ids start at 1 and count up");
    Check(graph.NextId() == 4, "the next id follows the last one");

    Check(graph.AddLink({alphaId, betaId, Ogt::LinkKind::LeadsTo, {}}), "a link between two neurons is added");
    Check(!graph.AddLink({alphaId, betaId, Ogt::LinkKind::Relates, {}}), "a second link between the same pair is refused");
    Check(!graph.AddLink({betaId, alphaId, Ogt::LinkKind::Relates, {}}), "the same pair the other way round is refused too");
    Check(!graph.AddLink({alphaId, alphaId, Ogt::LinkKind::Relates, {}}), "a neuron cannot link to itself");
    Check(!graph.AddLink({alphaId, 99, Ogt::LinkKind::Relates, {}}), "a link to an unknown neuron is refused");
    Check(graph.AddLink({betaId, gammaId, Ogt::LinkKind::PartOf, "draft"}), "links can carry a label");
    Check(graph.LinksOf(betaId).size() == 2, "a neuron in the middle sees both its links");

    const Ogt::Neuron removed = *graph.FindNeuron(betaId);
    Check(graph.RemoveNeuron(betaId), "a neuron is removed");
    Check(graph.FindNeuron(betaId) == nullptr, "a removed neuron is gone");
    Check(graph.Links().empty(), "its links went with it");
    Check(!graph.RemoveNeuron(betaId), "removing it twice does nothing");

    graph.AddNeuron(removed);
    Check(graph.FindNeuron(betaId) && graph.FindNeuron(betaId)->title == "Beta", "undo puts a neuron back under its old id");
    Check(graph.Neurons()[1].id == betaId, "neurons stay sorted by id after a reinsert");
    Check(graph.AddNeuron(Named("Delta")) == 4, "a reinsert does not reuse ids that are taken");

    Ogt::Neuron file;
    file.kind = Ogt::NeuronKind::Sheet;
    file.title = "Budget";
    file.path = "sheets/q3-costs.xlsx";
    const quint32 sheet = graph.AddNeuron(file);
    Ogt::Neuron* note = graph.FindNeuron(gammaId);
    note->text = "Ask about the Q3 numbers";
    Check(graph.Search("alpha").size() == 1, "search ignores case");
    Check(graph.Search("q3").size() == 2, "search reads note text and file names");
    Check(graph.Search("costs").front() == sheet, "a file name alone is enough to find a neuron");
    Check(graph.Search("   ").empty(), "a blank query finds nothing");

    Ogt::Graph folders;
    Ogt::Neuron box = Named("Box");
    box.kind = Ogt::NeuronKind::Folder;
    const quint32 outer = folders.AddNeuron(box);
    box.title = "Inner box";
    box.parent = outer;
    const quint32 inner = folders.AddNeuron(box);
    Ogt::Neuron deep = Named("Deep note");
    deep.parent = inner;
    const quint32 note2 = folders.AddNeuron(deep);
    const quint32 top = folders.AddNeuron(Named("Top note"));
    Check(folders.Children(0) == std::vector<quint32>{outer, top}, "the top level holds only what has no folder");
    Check(folders.Descendants(outer) == std::vector<quint32>{inner, note2}, "a folder holds everything below it");
    Check(folders.IsInside(note2, outer) && !folders.IsInside(top, outer), "depth is followed through folders");
    Check(!folders.AddLink({top, note2, Ogt::LinkKind::Relates, {}}), "a link cannot cross a folder's edge");
    Check(folders.AddLink({top, outer, Ogt::LinkKind::Relates, {}}), "a folder links like any neuron of its level");

    graph.Clear();
    Check(graph.IsEmpty() && graph.NextId() == 1, "clear starts over");

    if (gFailures == 0)
        std::cout << "graph_tests: all passed\n";
    return gFailures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
