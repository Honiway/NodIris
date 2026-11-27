/**
 * @file snapshot.cpp
 * @brief Renders the window to PNG without a screen, for the README and for design reviews.
 *
 * Usage: `nodiris_snapshot <output folder>`. Writes start.png, light.png,
 * dark.png, preview.png (the cursor resting on a note), document.png and
 * document-dark.png (a note opened full page) and folder.png (inside a
 * folder) from a sample research project. Runs with QT_QPA_PLATFORM=offscreen,
 * so it also works on a build machine.
 */

#include "core/ProjectFile.h"
#include "core/Thumbnail.h"
#include "ui/GraphScene.h"
#include "ui/GraphView.h"
#include "ui/DocumentPage.h"
#include "ui/MainWindow.h"
#include "ui/NeuronItem.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QMouseEvent>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>

#include <iostream>

namespace
{

// Create a simple board image for the example project.
void WriteSampleImage(const QString& path)
{
    QImage image(960, 640, QImage::Format_RGB32);
    image.fill(QColor("#c9a982"));
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor notes[] = {QColor("#f6f1e3"), QColor("#f3e3a1"), QColor("#dfe9f3"), QColor("#f6f1e3")};
    for (int i = 0; i < 4; ++i)
    {
        painter.save();
        painter.translate(140 + i * 200, 180 + (i % 2) * 140);
        painter.rotate(-4 + i * 3);
        painter.fillRect(QRectF(-80, -80, 170, 170), QColor(0, 0, 0, 40));
        painter.fillRect(QRectF(-86, -86, 170, 170), notes[i]);
        painter.setPen(QPen(QColor("#5b5b60"), 5, Qt::SolidLine, Qt::RoundCap));
        for (int line = 0; line < 4; ++line)
            painter.drawLine(QPointF(-60, -50 + line * 30), QPointF(40 - (line % 2) * 30, -50 + line * 30));
        painter.restore();
    }
    painter.end();
    image.save(path, "JPG", 90);
}

void Touch(const QString& path)
{
    QFile file(path);
    file.open(QIODevice::WriteOnly);
    file.write("sample");
}

Ogt::Graph SampleProject(const QString& folder)
{
    Ogt::Graph graph;
    auto addNote = [&](const QString& title, QPoint position, const QString& text = {})
    {
        Ogt::Neuron neuron;
        neuron.title = title;
        neuron.text = text;
        neuron.position = position;
        return graph.AddNeuron(neuron);
    };
    const quint32 question = addNote("Question", {0, 0}, "One sentence. If it takes two, it is two questions.");
    const quint32 sources = addNote("Sources", {-272, 120}, "Drop papers, PDFs and links next to this one.");
    const quint32 reading = addNote("Reading notes", {0, 160});
    const quint32 opinion = addNote("What I think so far", {272, 120});
    graph.AddLink({sources, reading, Ogt::LinkKind::Relates, {}, 1});
    graph.AddLink({reading, opinion, Ogt::LinkKind::LeadsTo, {}});
    graph.AddLink({opinion, question, Ogt::LinkKind::Relates, {}});

    WriteSampleImage(folder + "/whiteboard.jpg");
    Touch(folder + "/interviews.docx");
    Touch(folder + "/survey-results.xlsx");
    Touch(folder + "/Nielsen 1994.pdf");

    auto addFile = [&](Ogt::NeuronKind kind, const QString& title, const QString& file, QPoint position)
    {
        Ogt::Neuron neuron;
        neuron.kind = kind;
        neuron.title = title;
        neuron.path = file;
        neuron.position = position;
        if (kind == Ogt::NeuronKind::Image)
            neuron.thumbnail = Ogt::MakeThumbnail(folder + "/" + file);
        return graph.AddNeuron(neuron);
    };
    const quint32 board = addFile(Ogt::NeuronKind::Image, "Workshop board", "whiteboard.jpg", {-544, 280});
    const quint32 interviews = addFile(Ogt::NeuronKind::Document, "Interviews", "interviews.docx", {-544, 0});
    const quint32 survey = addFile(Ogt::NeuronKind::Sheet, "Survey results", "survey-results.xlsx", {-272, 300});
    const quint32 paper = addFile(Ogt::NeuronKind::Pdf, "Usability heuristics", "Nielsen 1994.pdf", {-272, -40});
    const quint32 missing = addFile(Ogt::NeuronKind::File, "Old recording", "recording.m4a", {272, 300});

    Ogt::Neuron task;
    task.kind = Ogt::NeuronKind::Task;
    task.title = "Write up findings";
    task.status = Ogt::TaskStatus::Doing;
    task.due = QDate(2026, 12, 11);
    task.position = {544, 120};
    const quint32 write = graph.AddNeuron(task);

    graph.AddLink({interviews, sources, Ogt::LinkKind::Relates, {}});
    graph.AddLink({paper, sources, Ogt::LinkKind::Relates, {}});
    graph.AddLink({board, reading, Ogt::LinkKind::Relates, "day 1", 1});
    graph.AddLink({survey, reading, Ogt::LinkKind::Relates, {}, 1});
    graph.AddLink({opinion, write, Ogt::LinkKind::LeadsTo, {}});
    graph.AddLink({missing, opinion, Ogt::LinkKind::PartOf, {}});

    Ogt::Neuron archive;
    archive.kind = Ogt::NeuronKind::Folder;
    archive.title = "Earlier rounds";
    archive.position = {544, 300};
    const quint32 rounds = graph.AddNeuron(archive);
    graph.AddLink({rounds, write, Ogt::LinkKind::Relates, {}});
    const char* titles[] = {"Round 1, March", "Round 2, June", "Pilot notes"};
    quint32 previous = 0;
    for (int i = 0; i < 3; ++i)
    {
        Ogt::Neuron inner;
        inner.kind = i < 2 ? Ogt::NeuronKind::Task : Ogt::NeuronKind::Note;
        inner.title = titles[i];
        inner.status = Ogt::TaskStatus::Done;
        inner.parent = rounds;
        inner.position = {i * 240, 0};
        const quint32 id = graph.AddNeuron(inner);
        if (previous)
            graph.AddLink({previous, id, Ogt::LinkKind::LeadsTo, {}});
        previous = id;
    }
    graph.FindNeuron(opinion)->text = "People skip the onboarding because it asks for an account first.\n\n"
                                      "Two of five found the export on their own. Nobody found the shortcuts.";
    return graph;
}

/// Opens a context menu at @p position and saves it in the window image.
void ShootContextMenu(QWidget& window, Ogt::GraphView* view, QPoint position, const QString& path)
{
    QTimer::singleShot(300, [&window, path]
    {
        QWidget* menu = QApplication::activePopupWidget();
        QImage shot = window.grab().toImage();
        if (menu)
        {
            QPainter painter(&shot);
            painter.drawPixmap(window.mapFromGlobal(menu->pos()), menu->grab());
            painter.end();
            menu->close();
        }
        shot.save(path);
        std::cout << "wrote " << path.toStdString() << '\n';
    });
    QContextMenuEvent event(QContextMenuEvent::Mouse, position, view->viewport()->mapToGlobal(position));
    QApplication::sendEvent(view->viewport(), &event);
}

void Shoot(QWidget& window, const QString& path)
{
    QApplication::processEvents();
    window.grab().save(path);
    std::cout << "wrote " << path.toStdString() << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QApplication::setOrganizationName("NodIris-snapshot");
    QApplication::setApplicationName("NodIris-snapshot");
    const QString outputFolder = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QDir::currentPath();
    QDir().mkpath(outputFolder);

    QTemporaryDir tempFolder;
    const QString projectPath = tempFolder.filePath("Field research.ogt");
    QString error;
    if (!Ogt::SaveProject(projectPath, SampleProject(tempFolder.path()), error))
    {
        std::cerr << error.toStdString() << '\n';
        return 1;
    }

    Ogt::Theme::Get().Apply(app, Ogt::ThemeMode::Light);
    Ogt::MainWindow window;
    window.resize(1440, 900);
    window.show();
    Shoot(window, outputFolder + "/start.png");

    window.OpenProject(projectPath);
    QApplication::processEvents();
    window.View()->FitAll();
    window.Scene()->SelectOnly(4); // "What I think so far"
    Shoot(window, outputFolder + "/light.png");

    Ogt::Theme::Get().Apply(app, Ogt::ThemeMode::Dark);
    Shoot(window, outputFolder + "/dark.png");

    // Rest the cursor on a note and wait for its preview.
    Ogt::Theme::Get().Apply(app, Ogt::ThemeMode::Light);
    window.Scene()->clearSelection();
    Ogt::GraphView* view = window.View();
    const QPoint neuronPosition = view->mapFromScene(window.Scene()->ItemFor(4)->SceneCenter());
    QMouseEvent mouseMove(QEvent::MouseMove, neuronPosition, view->viewport()->mapToGlobal(neuronPosition),
                          Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &mouseMove);
    QEventLoop previewWait;
    QTimer::singleShot(700, &previewWait, &QEventLoop::quit);
    previewWait.exec();
    Shoot(window, outputFolder + "/preview.png");

    window.Scene()->clearSelection();
    ShootContextMenu(window, view, neuronPosition, outputFolder + "/menu-neuron.png");
    ShootContextMenu(window, view, QPoint(200, 150), outputFolder + "/menu-canvas.png");

    // Same as a double-click on "What I think so far".
    emit window.Scene()->NeuronActivated(4);
    Shoot(window, outputFolder + "/document.png");
    Ogt::Theme::Get().Apply(app, Ogt::ThemeMode::Dark);
    Shoot(window, outputFolder + "/document-dark.png");
    Ogt::Theme::Get().Apply(app, Ogt::ThemeMode::Light);
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(window.Document(), &escape);

    window.Scene()->ShowFolder(window.Scene()->GetGraph().Neurons().back().parent);
    view->FitAll();
    Shoot(window, outputFolder + "/folder.png");
    return 0;
}
