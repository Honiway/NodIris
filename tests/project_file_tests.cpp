/**
 * @file project_file_tests.cpp
 * @brief A project saved and opened again is the same project, and stays small.
 *
 * Checks a full round trip through disk, that a damaged or foreign file is
 * refused without touching the graph already open, that paths are stored
 * relative to the project, that a thumbnail is a few kilobytes, and that a
 * thousand notes fit in well under a hundred kilobytes.
 *
 * Run with `ctest -R project_file_tests`. Exits non-zero on any failure.
 */

#include "core/ProjectFile.h"
#include "core/Thumbnail.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>

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

Ogt::Graph SampleGraph()
{
    Ogt::Graph graph;

    Ogt::Neuron note;
    note.title = "Réunion du lundi";
    note.text = "Ordre du jour\n- budget\n- planning";
    note.position = {-48, 120};
    const quint32 noteId = graph.AddNeuron(note);

    Ogt::Neuron task;
    task.kind = Ogt::NeuronKind::Task;
    task.title = "Envoyer le devis";
    task.status = Ogt::TaskStatus::Doing;
    task.due = QDate(2026, 12, 3);
    task.icon = Ogt::NeuronIcon::Flag;
    task.iconImage = QByteArray("compressed custom icon");
    task.position = {272, 120};
    const quint32 taskId = graph.AddNeuron(task);

    Ogt::Neuron sheet;
    sheet.kind = Ogt::NeuronKind::Sheet;
    sheet.title = "Budget";
    sheet.path = "docs/budget.xlsx";
    sheet.thumbnail = QByteArray("\xff\xd8 not really a jpeg", 20);
    const quint32 sheetId = graph.AddNeuron(sheet);

    graph.AddLink({noteId, taskId, Ogt::LinkKind::LeadsTo, "puis", 7});
    graph.AddLink({sheetId, noteId, Ogt::LinkKind::PartOf, {}});
    return graph;
}
} // namespace

int main(int argc, char** argv)
{
    // Image plugins (JPEG) are found through the application object.
    QCoreApplication app(argc, argv);
    QTemporaryDir folder;
    Check(folder.isValid(), "a temporary folder is available");

    const Ogt::Graph original = SampleGraph();
    const QString path = folder.filePath("plan.ogt");
    QString error;
    Check(Ogt::SaveProject(path, original, error), "the project saves (" + error.toStdString() + ")");

    Ogt::Graph loaded;
    Check(Ogt::LoadProject(path, loaded, error), "the project opens again (" + error.toStdString() + ")");
    Check(loaded.Neurons() == original.Neurons(), "every neuron comes back as it was, accents included");
    Check(loaded.Links() == original.Links(), "every link comes back as it was");
    Check(loaded.NextId() == original.NextId(), "the next id is kept, so new neurons never reuse an old one");

    Ogt::Graph untouched = SampleGraph();
    QFile garbage(folder.filePath("notes.txt"));
    garbage.open(QIODevice::WriteOnly);
    garbage.write("just some text, not a project");
    garbage.close();
    Check(!Ogt::LoadProject(garbage.fileName(), untouched, error), "a text file is refused");
    Check(!error.isEmpty(), "the refusal says why");
    Check(untouched.Neurons() == original.Neurons(), "a refused file leaves the open graph alone");

    Ogt::Graph nested;
    Ogt::Neuron chapter;
    chapter.kind = Ogt::NeuronKind::Folder;
    chapter.title = "Chapitre 1";
    const quint32 folderId = nested.AddNeuron(chapter);
    Ogt::Neuron child;
    child.title = "Inside";
    child.parent = folderId;
    nested.AddNeuron(child);
    Ogt::Neuron stray;
    stray.title = "Stray";
    stray.parent = 77; // a folder that does not exist
    const quint32 strayId = nested.AddNeuron(stray);
    Ogt::Graph reread;
    Check(Ogt::DecodeGraph(Ogt::EncodeGraph(nested), reread, error), "a project with folders reads back");
    Check(reread.Descendants(folderId).size() == 1, "a neuron stays inside its folder after a round trip");
    Check(reread.FindNeuron(strayId)->parent == 0, "a neuron whose folder is gone comes back at the top level");

    QByteArray damaged = Ogt::EncodeGraph(original);
    damaged.truncate(damaged.size() / 2);
    Check(!Ogt::DecodeGraph(damaged, untouched, error), "a truncated project is refused");

    QByteArray hostile = Ogt::EncodeGraph(original);
    hostile[4] = char(0x7f); // claims an unpacked size near 2 GB
    Check(!Ogt::DecodeGraph(hostile, untouched, error), "an absurd unpacked size is refused before allocating");

    QByteArray future = Ogt::EncodeGraph(original);
    future[3] = char(Ogt::kFormatVersion + 1);
    Check(!Ogt::DecodeGraph(future, untouched, error), "a project from a newer version is refused");

    const QString project = QDir::cleanPath(folder.path() + "/work/plan.ogt");
    const QString image = QDir::cleanPath(folder.path() + "/pictures/board.png");
    const QString storedPath = Ogt::RelativeToProject(project, image);
    Check(storedPath == "../pictures/board.png", "paths are stored relative to the project folder");
    Check(Ogt::ResolveFromProject(project, storedPath) == image, "a stored path resolves back to the file");

    QImage picture(1200, 800, QImage::Format_ARGB32);
    picture.fill(Qt::transparent);
    {
        QPainter painter(&picture);
        painter.fillRect(100, 100, 600, 400, QColor("#3f6fd8"));
        painter.fillRect(500, 300, 600, 400, QColor(200, 120, 60, 160));
    }
    const QString picturePath = folder.filePath("board.png");
    picture.save(picturePath);
    const QByteArray thumbnail = Ogt::MakeThumbnail(picturePath);
    const QImage small = QImage::fromData(thumbnail);
    Check(!small.isNull(), "the thumbnail decodes");
    Check(small.width() == Ogt::kThumbnailSide && small.height() <= Ogt::kThumbnailSide,
          "the thumbnail keeps the aspect ratio inside the size limit");
    Check(thumbnail.size() < 8 * 1024,
          "a thumbnail is a few kilobytes (" + std::to_string(thumbnail.size()) + " bytes)");
    const QByteArray iconImage = Ogt::MakeThumbnail(picturePath, Ogt::kIconImageSide, true);
    const QImage smallIcon = QImage::fromData(iconImage);
    Check(!smallIcon.isNull() && smallIcon.width() == Ogt::kIconImageSide
              && smallIcon.height() <= Ogt::kIconImageSide,
          "a custom icon is decoded inside its smaller size limit");
    Check(smallIcon.hasAlphaChannel(), "a custom icon keeps transparent edges");
    Check(iconImage.size() < 32 * 1024, "a custom icon stays compact in storage");
    Check(Ogt::MakeThumbnail(garbage.fileName()).isEmpty(), "a file that is not an image has no thumbnail");

    Ogt::Graph big;
    for (int i = 0; i < 1000; ++i)
    {
        Ogt::Neuron neuron;
        neuron.title = QString("Note %1").arg(i);
        neuron.text = "A short line of text like the ones people actually write.";
        neuron.position = {(i % 40) * 240, (i / 40) * 96};
        big.AddNeuron(neuron);
        if (i > 0)
            big.AddLink({quint32(i), quint32(i + 1), Ogt::LinkKind::Relates, {}});
    }
    const qsizetype bigSize = Ogt::EncodeGraph(big).size();
    std::cout << "1000 notes and 999 links: " << bigSize << " bytes\n";
    Check(bigSize < 64 * 1024, "a thousand notes stay under 64 KB");

    if (gFailures == 0)
        std::cout << "project_file_tests: all passed\n";
    return gFailures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
