/**
 * @file Inspector.cpp
 * @brief One page for neurons, one for links, one hint for when nothing is selected.
 */

#include "ui/Inspector.h"

#include "core/ProjectFile.h"
#include "core/Thumbnail.h"
#include "ui/GraphScene.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Ogt
{

namespace
{

constexpr int kWidth = 288;
constexpr int kMinimumWidth = 220;
constexpr int kMaximumWidth = 320;
constexpr int kCustomIconChoice = -1;
// QDateEdit cannot be empty, so its minimum date stands for "no date".
const QDate kNoDate(2000, 1, 1);

QLabel* SectionLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("role", "section");
    return label;
}

QPushButton* FlatButton(const QString& text, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setProperty("flat", true);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

} // namespace

Inspector::Inspector(GraphScene* scene, QWidget* parent)
    : QFrame(parent)
    , scene_(scene)
{
    setProperty("role", "panel");
    setMinimumWidth(kMinimumWidth);
    setMaximumWidth(kMaximumWidth);
    resize(kWidth, height());
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    pages_ = new QStackedWidget(this);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(pages_);

    // Nothing selected: say what can be done, once, in plain words.
    auto* empty = new QWidget(pages_);
    auto* emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(20, 20, 20, 20);
    auto* hint = new QLabel(tr("Double-click the canvas to add a neuron, or drop files on it.\n\n"
                               "Drag the dot on the edge of a neuron onto another one to link them.\n\n"
                               "Select something to see its details here."),
                            empty);
    hint->setWordWrap(true);
    hint->setProperty("role", "secondary");
    emptyLayout->addWidget(hint);
    emptyLayout->addStretch();
    pages_->addWidget(empty);

    auto* neuronPage = new QWidget(pages_);
    auto* layout = new QVBoxLayout(neuronPage);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(8);

    kind_ = new QLabel(neuronPage);
    kind_->setProperty("role", "section");
    layout->addWidget(kind_);
    title_ = new QLineEdit(neuronPage);
    title_->setPlaceholderText(tr("Title"));
    title_->setFont(Theme::Font(Theme::kTitle, true));
    layout->addWidget(title_);

    layout->addSpacing(8);
    layout->addWidget(SectionLabel(tr("Icon"), neuronPage));
    icon_ = new QComboBox(neuronPage);
    icon_->addItem(tr("Automatic (node type)"), int(NeuronIcon::Automatic));
    icon_->addItem(tr("Star"), int(NeuronIcon::Star));
    icon_->addItem(tr("Idea"), int(NeuronIcon::Idea));
    icon_->addItem(tr("Person"), int(NeuronIcon::Person));
    icon_->addItem(tr("Flag"), int(NeuronIcon::Flag));
    icon_->addItem(tr("Home"), int(NeuronIcon::Home));
    icon_->addItem(tr("Heart"), int(NeuronIcon::Heart));
    layout->addWidget(icon_);
    auto* iconButtons = new QHBoxLayout();
    iconButtons->setContentsMargins(0, 0, 0, 0);
    iconButtons->setSpacing(4);
    chooseIcon_ = FlatButton(tr("Choose an image"), neuronPage);
    resetIcon_ = FlatButton(tr("Reset"), neuronPage);
    iconButtons->addWidget(chooseIcon_);
    iconButtons->addWidget(resetIcon_);
    iconButtons->addStretch();
    layout->addLayout(iconButtons);

    fileRow_ = new QWidget(neuronPage);
    auto* fileLayout = new QVBoxLayout(fileRow_);
    fileLayout->setContentsMargins(0, 8, 0, 0);
    fileLayout->setSpacing(4);
    preview_ = new QLabel(fileRow_);
    preview_->setAlignment(Qt::AlignLeft);
    fileLayout->addWidget(preview_);
    filePath_ = new QLabel(fileRow_);
    filePath_->setProperty("role", "muted");
    filePath_->setWordWrap(true);
    filePath_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fileLayout->addWidget(filePath_);
    auto* fileButtons = new QHBoxLayout();
    fileButtons->setContentsMargins(0, 0, 0, 0);
    fileButtons->setSpacing(4);
    open_ = FlatButton(tr("Open"), fileRow_);
    relocate_ = FlatButton(tr("Find file"), fileRow_);
    fileButtons->addWidget(open_);
    fileButtons->addWidget(relocate_);
    fileButtons->addStretch();
    fileLayout->addLayout(fileButtons);
    layout->addWidget(fileRow_);

    address_ = new QLineEdit(neuronPage);
    address_->setPlaceholderText(tr("https://"));
    layout->addWidget(address_);

    taskRow_ = new QWidget(neuronPage);
    auto* taskLayout = new QHBoxLayout(taskRow_);
    taskLayout->setContentsMargins(0, 8, 0, 0);
    taskLayout->setSpacing(8);
    status_ = new QComboBox(taskRow_);
    for (TaskStatus status : {TaskStatus::Todo, TaskStatus::Doing, TaskStatus::Done})
        status_->addItem(StatusName(status), int(status));
    due_ = new QDateEdit(taskRow_);
    due_->setCalendarPopup(true);
    due_->setMinimumDate(kNoDate);
    due_->setSpecialValueText(tr("No date"));
    taskLayout->addWidget(status_, 1);
    taskLayout->addWidget(due_, 1);
    layout->addWidget(taskRow_);

    textLabel_ = SectionLabel(tr("Notes"), neuronPage);
    layout->addSpacing(8);
    layout->addWidget(textLabel_);
    text_ = new QPlainTextEdit(neuronPage);
    text_->setPlaceholderText(tr("Write anything"));
    layout->addWidget(text_, 1);

    connections_ = new QLabel(neuronPage);
    connections_->setProperty("role", "muted");
    connections_->setWordWrap(true);
    layout->addWidget(connections_);
    layout->addStretch(0);
    pages_->addWidget(neuronPage);

    auto* linkPage = new QWidget(pages_);
    auto* linkLayout = new QVBoxLayout(linkPage);
    linkLayout->setContentsMargins(20, 20, 20, 20);
    linkLayout->setSpacing(8);
    linkLayout->addWidget(SectionLabel(tr("Link"), linkPage));
    linkEnds_ = new QLabel(linkPage);
    linkEnds_->setWordWrap(true);
    linkEnds_->setFont(Theme::Font(Theme::kTitle, true));
    linkLayout->addWidget(linkEnds_);
    linkKind_ = new QComboBox(linkPage);
    for (LinkKind kind : {LinkKind::Relates, LinkKind::LeadsTo, LinkKind::PartOf})
        linkKind_->addItem(LinkKindName(kind), int(kind));
    linkLayout->addSpacing(8);
    linkLayout->addWidget(linkKind_);
    linkLabel_ = new QLineEdit(linkPage);
    linkLabel_->setPlaceholderText(tr("Label"));
    linkLayout->addWidget(linkLabel_);
    linkLayout->addStretch();
    pages_->addWidget(linkPage);

    connect(title_, &QLineEdit::textEdited, this, &Inspector::Commit);
    connect(icon_, &QComboBox::currentIndexChanged, this, &Inspector::Commit);
    connect(chooseIcon_, &QPushButton::clicked, this, &Inspector::ChooseIconImage);
    connect(resetIcon_, &QPushButton::clicked, this, &Inspector::ResetIcon);
    connect(address_, &QLineEdit::textEdited, this, &Inspector::Commit);
    connect(text_, &QPlainTextEdit::textChanged, this, &Inspector::Commit);
    connect(status_, &QComboBox::currentIndexChanged, this, &Inspector::Commit);
    connect(due_, &QDateEdit::dateChanged, this, &Inspector::Commit);
    connect(open_, &QPushButton::clicked, this, [this] { emit OpenRequested(neuronId_); });
    connect(relocate_, &QPushButton::clicked, this, &Inspector::Relocate);
    connect(linkKind_, &QComboBox::currentIndexChanged, this, &Inspector::CommitLink);
    connect(linkLabel_, &QLineEdit::textEdited, this, &Inspector::CommitLink);

    connect(scene_, &QGraphicsScene::selectionChanged, this, &Inspector::Refresh);
    connect(scene_, &GraphScene::Changed, this, &Inspector::Refresh);
    connect(&Theme::Get(), &Theme::Changed, this, &Inspector::RefreshIcons);
    RefreshIcons();
    Refresh();
}

void Inspector::FocusTitle()
{
    title_->setFocus();
    title_->selectAll();
}

void Inspector::RefreshIcons()
{
    const QColor color = Theme::Get().Tokens().textSecondary;
    open_->setIcon(MakeIcon(IconId::Open, color));
    relocate_->setIcon(MakeIcon(IconId::Relocate, color));
    for (int i = 0; i < icon_->count(); ++i)
    {
        const int iconValue = icon_->itemData(i).toInt();
        if (iconValue >= int(NeuronIcon::Star))
            icon_->setItemIcon(i, MakeNeuronIcon(NeuronIcon(iconValue), color));
    }
}

void Inspector::Refresh()
{
    if (filling_)
        return;
    if (const auto id = scene_->SelectedNeuron())
    {
        if (const Neuron* neuron = scene_->GetGraph().FindNeuron(*id))
            return ShowNeuron(*neuron);
    }
    if (const auto link = scene_->SelectedLink())
        return ShowLink(*link);
    neuronId_ = 0;
    pages_->setCurrentIndex(0);
}

void Inspector::ShowNeuron(const Neuron& neuron)
{
    filling_ = true;
    neuronId_ = neuron.id;
    pages_->setCurrentIndex(1);

    kind_->setText(KindName(neuron.kind));
    if (title_->text() != neuron.title)
        title_->setText(neuron.title);

    const int customIndex = icon_->findData(kCustomIconChoice);
    if (!neuron.iconImage.isEmpty())
    {
        if (customIndex < 0)
            icon_->addItem(tr("Custom image"), kCustomIconChoice);
        if (shownIconImage_ != neuron.iconImage || customIndex < 0)
        {
            shownIconImage_ = neuron.iconImage;
            QPixmap image;
            image.loadFromData(shownIconImage_);
            icon_->setItemIcon(icon_->findData(kCustomIconChoice), QIcon(image));
        }
        icon_->setCurrentIndex(icon_->findData(kCustomIconChoice));
    }
    else
    {
        shownIconImage_.clear();
        if (customIndex >= 0)
            icon_->removeItem(customIndex);
        icon_->setCurrentIndex(icon_->findData(int(neuron.icon)));
    }
    resetIcon_->setEnabled(neuron.icon != NeuronIcon::Automatic || !neuron.iconImage.isEmpty());

    const bool isFile = IsFileKind(neuron.kind);
    const bool isFolder = neuron.kind == NeuronKind::Folder;
    fileRow_->setVisible(isFile || isFolder);
    relocate_->setVisible(isFile);
    if (isFolder)
    {
        // The same row as a file: what it holds, and a button to go in.
        const int count = int(scene_->GetGraph().Children(neuron.id).size());
        filePath_->setText(count == 0 ? tr("Empty folder") : tr("Contains %n neurons", nullptr, count));
        open_->setEnabled(true);
        preview_->setVisible(false);
    }
    else if (isFile)
    {
        const QString absolute = ResolveFromProject(scene_->ProjectFile(), neuron.path);
        const bool exists = !absolute.isEmpty() && QFileInfo::exists(absolute);
        filePath_->setText(exists ? QDir::toNativeSeparators(neuron.path)
                                  : tr("File not found: %1").arg(QDir::toNativeSeparators(neuron.path)));
        open_->setEnabled(exists);
        QPixmap thumbnail;
        if (!neuron.thumbnail.isEmpty())
            thumbnail.loadFromData(neuron.thumbnail);
        preview_->setVisible(!thumbnail.isNull());
        preview_->setPixmap(thumbnail);
    }

    address_->setVisible(neuron.kind == NeuronKind::Link);
    if (address_->text() != neuron.text && neuron.kind == NeuronKind::Link)
        address_->setText(neuron.text);

    taskRow_->setVisible(neuron.kind == NeuronKind::Task);
    status_->setCurrentIndex(status_->findData(int(neuron.status)));
    due_->setDate(neuron.due.isValid() ? neuron.due : kNoDate);

    const bool hasText = neuron.kind == NeuronKind::Note || neuron.kind == NeuronKind::Task;
    textLabel_->setVisible(hasText);
    text_->setVisible(hasText);
    // Replacing the text moves the cursor to the start, so only do it when it really differs.
    if (hasText && text_->toPlainText() != neuron.text)
        text_->setPlainText(neuron.text);

    QStringList linkedNeuronNames;
    for (const Link& link : scene_->GetGraph().LinksOf(neuron.id))
    {
        const quint32 linkedNeuronId = link.from == neuron.id ? link.to : link.from;
        if (const Neuron* linkedNeuron = scene_->GetGraph().FindNeuron(linkedNeuronId))
            linkedNeuronNames.append(linkedNeuron->title.isEmpty() ? KindName(linkedNeuron->kind)
                                                                   : linkedNeuron->title);
    }
    connections_->setVisible(!linkedNeuronNames.isEmpty());
    connections_->setText(tr("Linked to %1").arg(linkedNeuronNames.join(", ")));
    filling_ = false;
}

void Inspector::ShowLink(const Link& link)
{
    filling_ = true;
    neuronId_ = 0;
    pages_->setCurrentIndex(2);
    const Graph& graph = scene_->GetGraph();
    const Neuron* sourceNeuron = graph.FindNeuron(link.from);
    const Neuron* targetNeuron = graph.FindNeuron(link.to);
    linkEnds_->setText(QString("%1  →  %2").arg(sourceNeuron ? sourceNeuron->title : QString(),
                                                targetNeuron ? targetNeuron->title : QString()));
    linkKind_->setCurrentIndex(linkKind_->findData(int(link.kind)));
    if (linkLabel_->text() != link.label)
        linkLabel_->setText(link.label);
    filling_ = false;
}

void Inspector::Commit()
{
    if (filling_ || neuronId_ == 0)
        return;
    const Neuron* currentNeuron = scene_->GetGraph().FindNeuron(neuronId_);
    if (!currentNeuron)
        return;

    Neuron after = *currentNeuron;
    after.title = title_->text();
    if (icon_->currentData().toInt() != kCustomIconChoice)
    {
        after.icon = NeuronIcon(icon_->currentData().toInt());
        after.iconImage.clear();
    }
    if (after.kind == NeuronKind::Link)
        after.text = address_->text().trimmed();
    if (after.kind == NeuronKind::Note || after.kind == NeuronKind::Task)
        after.text = text_->toPlainText();
    if (after.kind == NeuronKind::Task)
    {
        after.status = TaskStatus(status_->currentData().toInt());
        after.due = due_->date() == kNoDate ? QDate() : due_->date();
    }

    filling_ = true;
    scene_->EditNeuron(after);
    filling_ = false;
}

void Inspector::CommitLink()
{
    if (filling_)
        return;
    const auto selected = scene_->SelectedLink();
    if (!selected)
        return;
    Link after = *selected;
    after.kind = LinkKind(linkKind_->currentData().toInt());
    after.label = linkLabel_->text();
    filling_ = true;
    scene_->EditLink(after);
    filling_ = false;
}

void Inspector::Relocate()
{
    const Neuron* currentNeuron = scene_->GetGraph().FindNeuron(neuronId_);
    if (!currentNeuron)
        return;
    const QString start = QFileInfo(scene_->ProjectFile()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(this, tr("Find file"), start);
    if (path.isEmpty())
        return;

    Neuron after = *currentNeuron;
    after.path = RelativeToProject(scene_->ProjectFile(), path);
    after.kind = KindForPath(path);
    after.thumbnail = after.kind == NeuronKind::Image ? MakeThumbnail(path) : QByteArray();
    scene_->EditNeuron(after);
}

void Inspector::ChooseIconImage()
{
    const Neuron* currentNeuron = scene_->GetGraph().FindNeuron(neuronId_);
    if (!currentNeuron)
        return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose an icon image"), {},
                                                      tr("Images (*.png *.jpg *.jpeg *.webp *.bmp *.gif);;All files (*)"));
    if (path.isEmpty())
        return;
    const QByteArray image = MakeThumbnail(path, kIconImageSide, true);
    if (image.isEmpty())
    {
        QMessageBox::warning(this, tr("Invalid image"), tr("This image could not be opened."));
        return;
    }
    Neuron after = *currentNeuron;
    after.icon = NeuronIcon::Automatic;
    after.iconImage = image;
    scene_->EditNeuron(after);
}

void Inspector::ResetIcon()
{
    const Neuron* currentNeuron = scene_->GetGraph().FindNeuron(neuronId_);
    if (!currentNeuron)
        return;
    Neuron after = *currentNeuron;
    after.icon = NeuronIcon::Automatic;
    after.iconImage.clear();
    scene_->EditNeuron(after);
}

} // namespace Ogt
