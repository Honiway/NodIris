/**
 * @file scene_tests.cpp
 * @brief The canvas and its undo history agree with the graph at every step.
 *
 * Adds, links, edits, moves and deletes through GraphScene the way
 * the window does, and checks after each step and after undoing it that the
 * graph, the items on the canvas and the file on disk all say the same thing.
 *
 * Run with `ctest -R scene_tests`. Exits non-zero on any failure.
 */

#include "core/ProjectFile.h"
#include "ui/DocumentPage.h"
#include "ui/GraphScene.h"
#include "ui/GraphView.h"
#include "ui/LinkItem.h"
#include "ui/NeuronItem.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QFile>
#include <QTemporaryDir>
#include <QUndoStack>

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

int CountItems(const Ogt::GraphScene& scene, int type)
{
    int count = 0;
    for (const QGraphicsItem* item : scene.items())
        count += item->type() == type ? 1 : 0;
    return count;
}

bool InStep(const Ogt::GraphScene& scene)
{
    return CountItems(scene, Ogt::NeuronItem::Type) == int(scene.GetGraph().Neurons().size())
        && CountItems(scene, Ogt::LinkItem::Type) == int(scene.GetGraph().Links().size());
}

/// Items match the neurons and links of the folder on screen.
bool InStepAt(const Ogt::GraphScene& scene)
{
    const Ogt::Graph& graph = scene.GetGraph();
    const std::vector<quint32> shown = graph.Children(scene.CurrentFolder());
    int links = 0;
    for (const Ogt::Link& link : graph.Links())
        links += graph.FindNeuron(link.from)->parent == scene.CurrentFolder() ? 1 : 0;
    return CountItems(scene, Ogt::NeuronItem::Type) == int(shown.size())
        && CountItems(scene, Ogt::LinkItem::Type) == links;
}

Ogt::Neuron Make(Ogt::NeuronKind kind, const QString& title, QPoint position)
{
    Ogt::Neuron neuron;
    neuron.kind = kind;
    neuron.title = title;
    neuron.position = position;
    return neuron;
}

void Press(Ogt::GraphScene& scene, QPointF position, Qt::MouseButton button)
{
    QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMousePress);
    event.setScenePos(position);
    event.setButton(button);
    event.setButtons(button);
    QApplication::sendEvent(&scene, &event);
}
} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QApplication::setOrganizationName("NodIris-tests");
    Ogt::Theme::Get().Apply(app, Ogt::ThemeMode::Light);

    QTemporaryDir folder;
    const QString project = folder.filePath("plan.ogt");
    Ogt::GraphScene scene;
    scene.Load({}, project);
    QUndoStack* undo = scene.UndoStack();

    const quint32 plan = scene.AddNeuron(Make(Ogt::NeuronKind::Task, "Plan", {0, 0}));
    const quint32 build = scene.AddNeuron(Make(Ogt::NeuronKind::Task, "Build", {301, 7}));
    const quint32 note = scene.AddNeuron(Make(Ogt::NeuronKind::Note, "Why", {0, 200}));
    Check(scene.GetGraph().Neurons().size() == 3 && InStep(scene), "three neurons, three cards");
    Check(scene.GetGraph().FindNeuron(build)->position == QPoint(300, 8), "new neurons land on the 4 px grid");

    scene.Connect(plan, build);
    scene.Connect(note, plan);
    scene.Connect(build, plan);
    Check(scene.GetGraph().Links().size() == 2 && InStep(scene), "two links, and the reversed duplicate is ignored");
    Check(scene.GetGraph().FindLink(plan, build)->kind == Ogt::LinkKind::LeadsTo,
          "two tasks joined lead from one to the next");
    Check(scene.GetGraph().FindLink(note, plan)->kind == Ogt::LinkKind::Relates,
          "a note joined to a task is just related");

    const auto handles = scene.ItemFor(plan)->HandleRects();
    Check(handles.size() == 4 && handles[0].center() != handles[1].center()
              && scene.ItemFor(plan)->HandleCenterAt(handles[2].center()).has_value(),
          "a neuron has four working connection handles");

    Ogt::GraphScene pasted;
    pasted.Load({}, project);
    pasted.Paste({*scene.GetGraph().FindNeuron(plan), *scene.GetGraph().FindNeuron(build)},
                 {*scene.GetGraph().FindLink(plan, build)}, {500, 500});
    Check(pasted.GetGraph().Neurons().size() == 2 && pasted.GetGraph().Links().size() == 1,
          "pasting neurons keeps the links between them");
    pasted.UndoStack()->undo();
    Check(pasted.GetGraph().IsEmpty(), "a paste is undone in one step");

    pasted.SetLinkToolEnabled(true);
    Check(pasted.LinkToolEnabled(), "the dedicated link tool can be enabled");
    pasted.SetLinkToolEnabled(false);
    Check(!pasted.LinkToolEnabled(), "the dedicated link tool can be cancelled");
    pasted.SetMultiLinkToolEnabled(true);
    Check(pasted.MultiLinkToolEnabled() && !pasted.LinkToolEnabled(),
          "the multiple link tool is enabled without the simple link tool");
    pasted.SetMultiLinkToolEnabled(false);

    Ogt::GraphScene joined;
    joined.Load({}, project);
    const quint32 sourceA = joined.AddNeuron(Make(Ogt::NeuronKind::Task, "A", {0, 0}));
    const quint32 sourceB = joined.AddNeuron(Make(Ogt::NeuronKind::Note, "B", {0, 100}));
    const quint32 target = joined.AddNeuron(Make(Ogt::NeuronKind::Task, "Target", {300, 50}));
    joined.ConnectMany({sourceA, sourceB, target}, target);
    Check(joined.GetGraph().Links().size() == 2,
          "multiple sources are linked to one target and the target itself is ignored");
    Check(joined.GetGraph().Links()[0].joinGroup != 0
              && joined.GetGraph().Links()[0].joinGroup == joined.GetGraph().Links()[1].joinGroup,
          "multiple links share one visual join group");
    Check(joined.GetGraph().FindLink(sourceA, target)->kind == Ogt::LinkKind::LeadsTo
              && joined.GetGraph().FindLink(sourceB, target)->kind == Ogt::LinkKind::Relates,
          "each multiple link keeps the usual task relationship rule");
    joined.UndoStack()->undo();
    Check(joined.GetGraph().Links().empty(), "all multiple links are undone in one step");
    joined.SetMultiLinkToolEnabled(true);
    Press(joined, joined.ItemFor(sourceA)->SceneCenter(), Qt::LeftButton);
    Press(joined, joined.ItemFor(sourceB)->SceneCenter(), Qt::LeftButton);
    Check(joined.selectedItems().size() == 2, "left clicks collect multiple source neurons");
    Press(joined, joined.ItemFor(target)->SceneCenter(), Qt::RightButton);
    Check(joined.GetGraph().Links().size() == 2 && joined.selectedItems().empty(),
          "a right click links the collected sources to its target and clears the batch");
    joined.SetLinkToolEnabled(true);
    Check(joined.LinkToolEnabled() && !joined.MultiLinkToolEnabled(),
          "enabling the simple link tool closes the multiple link tool");

    Ogt::LinkItem* firstLink = nullptr;
    for (QGraphicsItem* item : scene.items())
    {
        if (item->type() == Ogt::LinkItem::Type)
        {
            firstLink = static_cast<Ogt::LinkItem*>(item);
            break;
        }
    }
    const QRectF curvedBounds = firstLink ? firstLink->boundingRect() : QRectF();
    Ogt::AppearanceSettings appearance;
    appearance.link = QColor("#123456");
    appearance.linkStyle = Ogt::LinkStyle::Straight;
    appearance.linkWidth = 3.0;
    Ogt::Theme::Get().ApplyAppearance(app, appearance);
    Check(Ogt::Theme::Get().Tokens().link == QColor("#123456")
              && Ogt::Theme::Get().Appearance().linkWidth == 3.0,
          "custom connection colour and width are applied");
    const QRectF straightBounds = firstLink ? firstLink->boundingRect() : QRectF();
    Check(firstLink && straightBounds.width() * straightBounds.height() < curvedBounds.width() * curvedBounds.height(),
          "connections can switch from curved to straight");
    Ogt::Theme::Get().ResetAppearance(app);

    Ogt::Neuron renamed = *scene.GetGraph().FindNeuron(note);
    renamed.title = "Wh";
    scene.EditNeuron(renamed);
    renamed.title = "Why this matters";
    scene.EditNeuron(renamed);
    const int stepsAfterTyping = undo->count();
    undo->undo();
    Check(scene.GetGraph().FindNeuron(note)->title == "Why", "typing a title is one undo step, not one per key");
    undo->redo();
    Check(undo->count() == stepsAfterTyping, "redo brings the typing back as the same single step");

    scene.SelectOnly(plan);
    scene.DeleteSelection();
    Check(!scene.GetGraph().FindNeuron(plan) && scene.GetGraph().Links().empty() && InStep(scene),
          "deleting a neuron removes its card and both of its links");
    undo->undo();
    Check(scene.GetGraph().FindNeuron(plan) && scene.GetGraph().Links().size() == 2 && InStep(scene),
          "undo brings the neuron back with both links");

    QString error;
    Check(Ogt::SaveProject(project, scene.GetGraph(), error), "the project saves");
    Ogt::GraphScene reopened;
    Ogt::Graph loaded;
    Ogt::LoadProject(project, loaded, error);
    reopened.Load(std::move(loaded), project);
    Check(reopened.GetGraph().Neurons() == scene.GetGraph().Neurons() && InStep(reopened),
          "a reopened project shows the same cards");
    Check(reopened.UndoStack()->count() == 0, "a reopened project starts with an empty undo history");

    scene.SetFilter("matters");
    Check(scene.Matches().size() == 1 && scene.Matches().front() == note, "search finds the renamed note");
    scene.SetFilter({});
    Check(scene.Matches().empty(), "clearing the search shows everything again");

    QFile missing(folder.filePath("gone.docx"));
    missing.open(QIODevice::WriteOnly);
    missing.close();
    scene.AddFiles({missing.fileName(), folder.filePath("does-not-exist.txt")}, {600, 0});
    const Ogt::Neuron* added = &scene.GetGraph().Neurons().back();
    Check(added->kind == Ogt::NeuronKind::Document && added->path == "gone.docx",
          "a dropped file keeps a path relative to the project");
    Check(scene.GetGraph().Neurons().size() == 4, "a path that is not a file is skipped");

    // Folders: open one, add inside, drop a neuron into it, delete it with its content.
    Ogt::Neuron box = Make(Ogt::NeuronKind::Folder, "Research", {0, 400});
    const quint32 research = scene.AddNeuron(box);
    const int topLevel = CountItems(scene, Ogt::NeuronItem::Type);
    scene.ShowFolder(research);
    Check(CountItems(scene, Ogt::NeuronItem::Type) == 0, "an open folder starts empty");
    const quint32 inside = scene.AddNeuron(Make(Ogt::NeuronKind::Note, "Paper", {0, 0}));
    Check(scene.GetGraph().FindNeuron(inside)->parent == research, "what is added in a folder belongs to it");
    Check(scene.FolderPath() == std::vector<quint32>{research}, "the path leads to the open folder");
    scene.ShowFolder(0);
    Check(CountItems(scene, Ogt::NeuronItem::Type) == topLevel, "leaving the folder shows the top level again");

    scene.MoveIntoFolder({build}, research);
    Check(scene.GetGraph().FindNeuron(build)->parent == research, "a neuron dropped on a folder goes in");
    Check(!scene.GetGraph().FindLink(plan, build), "its link to a neuron left outside is cut");
    Check(!scene.ItemFor(build) && InStepAt(scene), "it leaves the canvas of the level it came from");
    undo->undo();
    Check(scene.GetGraph().FindNeuron(build)->parent == 0 && scene.GetGraph().FindLink(plan, build),
          "undo brings it back out with its link");

    scene.MoveIntoFolder({research}, research);
    Check(scene.GetGraph().FindNeuron(research)->parent == 0, "a folder cannot go inside itself");

    const size_t before = scene.GetGraph().Neurons().size();
    scene.SelectOnly(research);
    scene.DeleteSelection();
    Check(scene.GetGraph().Neurons().size() == before - 2, "deleting a folder deletes what it holds");
    undo->undo();
    Check(scene.GetGraph().FindNeuron(inside) && scene.GetGraph().FindNeuron(inside)->parent == research,
          "undo puts the folder back with its content inside");

    const quint32 scratch = scene.AddNeuron(Make(Ogt::NeuronKind::Folder, "Scratch", {400, 400}));
    scene.ShowFolder(scratch);
    undo->undo();
    Check(!scene.GetGraph().FindNeuron(scratch) && scene.CurrentFolder() == 0 && InStepAt(scene),
          "undoing the folder on screen steps back out to where it was");

    // Double-click opens notes and text files full page; Back saves what was written.
    Ogt::DocumentPage page(&scene);
    const quint32 draft = scene.AddNeuron(Make(Ogt::NeuronKind::Note, "Draft", {800, 0}));
    Check(page.Open(draft), "a note opens as a document");
    page.findChild<QPlainTextEdit*>()->setPlainText("First line\nSecond line");
    page.Close();
    Check(scene.GetGraph().FindNeuron(draft)->text == "First line\nSecond line", "leaving the page keeps the text");
    undo->undo();
    Check(scene.GetGraph().FindNeuron(draft)->text.isEmpty(), "the canvas undo takes the writing back");

    QFile textFile(folder.filePath("todo.txt"));
    textFile.open(QIODevice::WriteOnly);
    textFile.write("milk\n");
    textFile.close();
    scene.AddFiles({textFile.fileName()}, {800, 200});
    const quint32 todo = scene.GetGraph().Neurons().back().id;
    Check(page.Open(todo), "a text file opens as a document");
    Check(page.findChild<QPlainTextEdit*>()->toPlainText() == "milk\n", "the page shows what the file holds");
    page.findChild<QPlainTextEdit*>()->setPlainText("milk\neggs\n");
    page.Close();
    textFile.open(QIODevice::ReadOnly);
    Check(textFile.readAll() == "milk\neggs\n", "the file on disk gets the new text");
    textFile.close();
    const quint32 sheetId = scene.AddNeuron(Make(Ogt::NeuronKind::Sheet, "Budget", {800, 400}));
    Check(!page.Open(sheetId), "a spreadsheet is left to its own program");

    // Rename (F2 or right click) edits the title in a field over the node.
    Ogt::GraphView view(&scene);
    view.StartRename(draft);
    auto* field = view.viewport()->findChild<QLineEdit*>();
    Check(field && field->text() == "Draft", "the rename field starts with the current title");
    field->setText("Final draft");
    emit field->editingFinished();
    Check(scene.GetGraph().FindNeuron(draft)->title == "Final draft", "Enter in the rename field keeps the new title");
    undo->undo();
    Check(scene.GetGraph().FindNeuron(draft)->title == "Draft", "a rename is one step to undo");

    Ogt::GraphScene big;
    Ogt::Graph many;
    for (int i = 0; i < 1000; ++i)
    {
        Ogt::Neuron neuron = Make(i % 3 == 0 ? Ogt::NeuronKind::Task : Ogt::NeuronKind::Note,
                                  QString("Card %1").arg(i), {(i % 40) * 240, (i / 40) * 120});
        neuron.text = i % 2 ? "Two short lines of text,\nlike most notes." : QString();
        many.AddNeuron(neuron);
        if (i > 0)
            many.AddLink({quint32(i), quint32(i + 1), Ogt::LinkKind::Relates, {}});
    }
    big.Load(std::move(many), project);
    Check(InStep(big), "a thousand cards and their links are all on the canvas");

    if (gFailures == 0)
        std::cout << "scene_tests: all passed\n";
    return gFailures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
