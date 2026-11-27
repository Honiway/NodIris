/**
 * @file ProjectFile.cpp
 * @brief CBOR with integer keys, compressed with qCompress.
 */

#include "core/ProjectFile.h"

#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace Ogt
{

namespace
{

constexpr char kMagic[3] = {'O', 'G', 'T'};
constexpr int kHeaderSize = 4;
// Refuse files larger than the app can create.
constexpr qsizetype kMaxUnpackedSize = 256 * 1024 * 1024;

// Keys of the root map, of a neuron and of a link. Never renumber them.
enum RootKey : int
{
    RootNextId = 0,
    RootNeurons = 1,
    RootLinks = 2,
};

enum NeuronKey : int
{
    NeuronId = 0,
    NeuronKindKey = 1,
    NeuronTitle = 2,
    NeuronText = 3,
    NeuronPath = 4,
    NeuronStatus = 5,
    NeuronDue = 6,
    NeuronX = 7,
    NeuronY = 8,
    NeuronThumbnail = 9,
    NeuronParent = 10,
    NeuronIconKey = 11,
    NeuronIconImage = 12,
};

enum LinkKey : int
{
    LinkFrom = 0,
    LinkTo = 1,
    LinkKindKey = 2,
    LinkLabel = 3,
    LinkJoinGroup = 4,
};

QCborMap EncodeNeuron(const Neuron& neuron)
{
    QCborMap map;
    map.insert(NeuronId, neuron.id);
    if (neuron.parent != 0)
        map.insert(NeuronParent, neuron.parent);
    if (neuron.kind != NeuronKind::Note)
        map.insert(NeuronKindKey, int(neuron.kind));
    if (!neuron.title.isEmpty())
        map.insert(NeuronTitle, neuron.title);
    if (!neuron.text.isEmpty())
        map.insert(NeuronText, neuron.text);
    if (!neuron.path.isEmpty())
        map.insert(NeuronPath, neuron.path);
    if (neuron.status != TaskStatus::Todo)
        map.insert(NeuronStatus, int(neuron.status));
    if (neuron.due.isValid())
        map.insert(NeuronDue, neuron.due.toJulianDay());
    if (neuron.position.x() != 0)
        map.insert(NeuronX, neuron.position.x());
    if (neuron.position.y() != 0)
        map.insert(NeuronY, neuron.position.y());
    if (!neuron.thumbnail.isEmpty())
        map.insert(NeuronThumbnail, neuron.thumbnail);
    if (neuron.icon != NeuronIcon::Automatic)
        map.insert(NeuronIconKey, int(neuron.icon));
    if (!neuron.iconImage.isEmpty())
        map.insert(NeuronIconImage, neuron.iconImage);
    return map;
}

Neuron DecodeNeuron(const QCborMap& map)
{
    Neuron neuron;
    neuron.id = quint32(map.value(NeuronId).toInteger());
    neuron.parent = quint32(map.value(NeuronParent).toInteger(0));
    const qint64 kind = map.value(NeuronKindKey).toInteger(0);
    // Read an unknown kind as a basic file.
    neuron.kind = kind >= 0 && kind < kNeuronKindCount ? NeuronKind(kind) : NeuronKind::File;
    neuron.title = map.value(NeuronTitle).toString();
    neuron.text = map.value(NeuronText).toString();
    neuron.path = map.value(NeuronPath).toString();
    const qint64 status = map.value(NeuronStatus).toInteger(0);
    neuron.status = status >= 0 && status <= 2 ? TaskStatus(status) : TaskStatus::Todo;
    if (map.contains(NeuronDue))
        neuron.due = QDate::fromJulianDay(map.value(NeuronDue).toInteger());
    neuron.position = QPoint(int(map.value(NeuronX).toInteger()), int(map.value(NeuronY).toInteger()));
    neuron.thumbnail = map.value(NeuronThumbnail).toByteArray();
    const qint64 icon = map.value(NeuronIconKey).toInteger(0);
    neuron.icon = icon >= int(NeuronIcon::Automatic) && icon <= int(NeuronIcon::Heart)
                      ? NeuronIcon(icon) : NeuronIcon::Automatic;
    neuron.iconImage = map.value(NeuronIconImage).toByteArray();
    return neuron;
}

QCborMap EncodeLink(const Link& link)
{
    QCborMap map;
    map.insert(LinkFrom, link.from);
    map.insert(LinkTo, link.to);
    if (link.kind != LinkKind::Relates)
        map.insert(LinkKindKey, int(link.kind));
    if (!link.label.isEmpty())
        map.insert(LinkLabel, link.label);
    if (link.joinGroup != 0)
        map.insert(LinkJoinGroup, link.joinGroup);
    return map;
}

Link DecodeLink(const QCborMap& map)
{
    Link link;
    link.from = quint32(map.value(LinkFrom).toInteger());
    link.to = quint32(map.value(LinkTo).toInteger());
    const qint64 kind = map.value(LinkKindKey).toInteger(0);
    link.kind = kind >= 0 && kind <= 2 ? LinkKind(kind) : LinkKind::Relates;
    link.label = map.value(LinkLabel).toString();
    link.joinGroup = quint32(map.value(LinkJoinGroup).toInteger(0));
    return link;
}

} // namespace

QByteArray EncodeGraph(const Graph& graph)
{
    QCborArray neurons;
    for (const Neuron& neuron : graph.Neurons())
        neurons.append(EncodeNeuron(neuron));

    QCborArray links;
    for (const Link& link : graph.Links())
        links.append(EncodeLink(link));

    QCborMap root;
    root.insert(RootNextId, graph.NextId());
    root.insert(RootNeurons, neurons);
    if (!links.isEmpty())
        root.insert(RootLinks, links);

    QByteArray bytes(kMagic, sizeof(kMagic));
    bytes.append(char(kFormatVersion));
    bytes.append(qCompress(QCborValue(root).toCbor(), 9));
    return bytes;
}

bool DecodeGraph(const QByteArray& bytes, Graph& graph, QString& error)
{
    if (bytes.size() < kHeaderSize + 4 || !bytes.startsWith(QByteArray(kMagic, sizeof(kMagic))))
    {
        error = QCoreApplication::translate("Ogt", "This is not a NodIris project.");
        return false;
    }
    const quint8 version = quint8(bytes[3]);
    if (version > kFormatVersion)
    {
        error = QCoreApplication::translate("Ogt", "This project was saved by a newer version of NodIris.");
        return false;
    }

    // Check the size stored by qCompress before using memory for the file.
    const auto* size = reinterpret_cast<const uchar*>(bytes.constData() + kHeaderSize);
    const qsizetype unpackedSize = (qsizetype(size[0]) << 24) | (size[1] << 16) | (size[2] << 8) | size[3];
    if (unpackedSize > kMaxUnpackedSize)
    {
        error = QCoreApplication::translate("Ogt", "The project file is damaged.");
        return false;
    }

    const QByteArray cbor = qUncompress(bytes.mid(kHeaderSize));
    QCborParserError parseError;
    const QCborValue root = QCborValue::fromCbor(cbor, &parseError);
    if (cbor.isEmpty() || parseError.error != QCborError::NoError || !root.isMap())
    {
        error = QCoreApplication::translate("Ogt", "The project file is damaged.");
        return false;
    }

    Graph result;
    const QCborMap map = root.toMap();
    for (const QCborValue& value : map.value(RootNeurons).toArray())
    {
        Neuron neuron = DecodeNeuron(value.toMap());
        if (neuron.id != 0)
            result.AddNeuron(std::move(neuron));
    }
    // Move neurons with an invalid parent back to the top level.
    std::vector<quint32> orphans;
    for (const Neuron& neuron : result.Neurons())
    {
        if (neuron.parent == 0)
            continue;
        const Neuron* folder = result.FindNeuron(neuron.parent);
        if (!folder || folder->kind != NeuronKind::Folder || !result.IsInside(neuron.id, 0))
            orphans.push_back(neuron.id);
    }
    for (quint32 id : orphans)
        result.FindNeuron(id)->parent = 0;

    // AddLink ignores links with missing neurons.
    for (const QCborValue& value : map.value(RootLinks).toArray())
        result.AddLink(DecodeLink(value.toMap()));

    result.SetNextId(std::max(result.NextId(), quint32(map.value(RootNextId).toInteger(1))));
    graph = std::move(result);
    return true;
}

bool SaveProject(const QString& filePath, const Graph& graph, QString& error)
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
    {
        error = file.errorString();
        return false;
    }
    file.write(EncodeGraph(graph));
    if (!file.commit())
    {
        error = file.errorString();
        return false;
    }
    return true;
}

bool LoadProject(const QString& filePath, Graph& graph, QString& error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        error = file.errorString();
        return false;
    }
    return DecodeGraph(file.readAll(), graph, error);
}

QString RelativeToProject(const QString& projectFile, const QString& absolutePath)
{
    const QDir folder = QFileInfo(projectFile).absoluteDir();
    return QDir::fromNativeSeparators(folder.relativeFilePath(absolutePath));
}

QString ResolveFromProject(const QString& projectFile, const QString& storedPath)
{
    if (storedPath.isEmpty())
        return {};
    if (QDir::isAbsolutePath(storedPath))
        return QDir::cleanPath(storedPath);
    const QDir folder = QFileInfo(projectFile).absoluteDir();
    return QDir::cleanPath(folder.absoluteFilePath(storedPath));
}

} // namespace Ogt
