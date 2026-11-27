/**
 * @file PreviewPopup.cpp
 * @brief Reads only what it shows: a scaled picture, the first few kilobytes of a text file.
 */

#include "ui/PreviewPopup.h"

#include "core/ProjectFile.h"
#include "ui/Theme.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImageReader>
#include <QLabel>
#include <QLocale>
#include <QPixmap>
#include <QVBoxLayout>

namespace Ogt
{

namespace
{

constexpr int kWidth = 288;
constexpr int kPadding = 12;
constexpr int kImageWidth = kWidth - 2 * kPadding;
constexpr int kImageHeight = 240;
constexpr int kMaxLines = 12;
constexpr int kExcerptBytes = 4096;
constexpr int kFolderListed = 8;

/// The first lines of @p text, with a "…" when there was more.
QString FirstLines(const QString& text, int count)
{
    QStringList lines = text.split('\n');
    const bool hasMoreLines = lines.size() > count;
    lines = lines.mid(0, count);
    QString previewText = lines.join('\n').trimmed();
    if (hasMoreLines)
        previewText += QString::fromUtf8("\n…");
    return previewText;
}

QString Excerpt(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return FirstLines(QString::fromUtf8(file.read(kExcerptBytes)), kMaxLines);
}

QPixmap LoadPicture(const QString& path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid() && (size.width() > kImageWidth || size.height() > kImageHeight))
        reader.setScaledSize(size.scaled(kImageWidth, kImageHeight, Qt::KeepAspectRatio));
    return QPixmap::fromImage(reader.read());
}

QString FileFacts(const QFileInfo& info)
{
    const QLocale locale;
    return QString("%1  ·  %2")
        .arg(locale.formattedDataSize(info.size()), locale.toString(info.lastModified().date(), QLocale::ShortFormat));
}

} // namespace

PreviewPopup::PreviewPopup(QWidget* parent)
    : QFrame(parent)
{
    setProperty("role", "preview");
    setFixedWidth(kWidth);
    // The cursor stays on the node underneath, so hovering keeps working while this is shown.
    setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    layout->setSpacing(4);

    title_ = new QLabel(this);
    title_->setFont(Theme::Font(Theme::kBody, true));
    title_->setWordWrap(true);
    layout->addWidget(title_);
    meta_ = new QLabel(this);
    meta_->setProperty("role", "muted");
    meta_->setWordWrap(true);
    layout->addWidget(meta_);
    image_ = new QLabel(this);
    layout->addSpacing(4);
    layout->addWidget(image_);
    body_ = new QLabel(this);
    body_->setProperty("role", "secondary");
    body_->setWordWrap(true);
    body_->setTextFormat(Qt::PlainText);
    layout->addWidget(body_);
    hide();
}

void PreviewPopup::SetNeuron(const Neuron& neuron, const Graph& graph, const QString& projectFile)
{
    Clear();
    title_->setText(neuron.title.isEmpty() ? KindName(neuron.kind) : neuron.title);
    QString detailsText = KindName(neuron.kind);
    QString bodyText;
    QPixmap picture;
    body_->setFont(Theme::Font(Theme::kBody));

    switch (neuron.kind)
    {
    case NeuronKind::Note:
        bodyText = FirstLines(neuron.text, kMaxLines);
        break;
    case NeuronKind::Task:
        detailsText = StatusName(neuron.status);
        if (neuron.due.isValid())
            detailsText += QString::fromUtf8("  ·  ") + QLocale().toString(neuron.due, QLocale::LongFormat);
        bodyText = FirstLines(neuron.text, kMaxLines);
        break;
    case NeuronKind::Link:
        bodyText = neuron.text.isEmpty() ? tr("No address yet") : neuron.text;
        break;
    case NeuronKind::Folder:
    {
        const std::vector<quint32> children = graph.Children(neuron.id);
        detailsText = children.empty() ? tr("Empty folder") : tr("%n neurons", nullptr, int(children.size()));
        QStringList childNames;
        for (size_t i = 0; i < children.size() && i < size_t(kFolderListed); ++i)
        {
            const Neuron* child = graph.FindNeuron(children[i]);
            childNames.append(QString::fromUtf8("·  ")
                              + (child->title.isEmpty() ? KindName(child->kind) : child->title));
        }
        if (children.size() > size_t(kFolderListed))
            childNames.append(tr("and %n more", nullptr, int(children.size()) - kFolderListed));
        bodyText = childNames.join('\n');
        break;
    }
    default:
    {
        const QString path = ResolveFromProject(projectFile, neuron.path);
        const QFileInfo fileInfo(path);
        if (path.isEmpty() || !fileInfo.exists())
        {
            detailsText = tr("File not found: %1").arg(neuron.path);
            break;
        }
        detailsText = fileInfo.fileName() + QString::fromUtf8("  ·  ") + FileFacts(fileInfo);
        if (neuron.kind == NeuronKind::Image)
            picture = LoadPicture(path);
        else if (IsTextFile(path))
        {
            bodyText = Excerpt(path);
            if (fileInfo.suffix().compare("csv", Qt::CaseInsensitive) == 0
                || fileInfo.suffix().compare("tsv", Qt::CaseInsensitive) == 0)
            {
                QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
                fixedFont.setPixelSize(Theme::kSmall);
                body_->setFont(fixedFont);
            }
        }
        break;
    }
    }

    // An image that could not be read from disk still has its thumbnail.
    if (neuron.kind == NeuronKind::Image && picture.isNull() && !neuron.thumbnail.isEmpty())
        picture.loadFromData(neuron.thumbnail);

    meta_->setText(detailsText);
    body_->setText(bodyText);
    body_->setVisible(!bodyText.isEmpty());
    image_->setPixmap(picture);
    image_->setVisible(!picture.isNull());
    adjustSize();
}

void PreviewPopup::Clear()
{
    hide();
    image_->clear();
    body_->clear();
}

} // namespace Ogt
