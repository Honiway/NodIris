/**
 * @file DocumentPage.cpp
 * @brief One column of text at a readable width, a back button, undo and redo.
 */

#include "ui/DocumentPage.h"

#include "core/ProjectFile.h"
#include "ui/GraphScene.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QToolButton>
#include <QVBoxLayout>

namespace Ogt
{

namespace
{

constexpr int kSaveDelayMs = 800;
// Large files are too slow for the text editor.
constexpr qint64 kMaxFileBytes = 4 * 1024 * 1024;
constexpr int kColumnWidth = 720;

QToolButton* BarButton(const QString& tip, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setToolTip(tip);
    button->setAutoRaise(true);
    button->setIconSize(QSize(16, 16));
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

} // namespace

DocumentPage::DocumentPage(GraphScene* scene, QWidget* parent)
    : QWidget(parent)
    , scene_(scene)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* bar = new QFrame(this);
    bar->setProperty("role", "bar");
    bar->setFixedHeight(48);
    auto* barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(8, 0, 12, 0);
    barLayout->setSpacing(4);
    back_ = new QPushButton(tr("Back to the graph"), bar);
    back_->setProperty("flat", true);
    back_->setCursor(Qt::PointingHandCursor);
    back_->setToolTip(tr("Back to the graph  Esc"));
    barLayout->addWidget(back_);
    barLayout->addStretch(1);
    state_ = new QLabel(bar);
    state_->setProperty("role", "muted");
    barLayout->addWidget(state_);
    barLayout->addSpacing(8);
    undo_ = BarButton(tr("Undo"), bar);
    redo_ = BarButton(tr("Redo"), bar);
    barLayout->addWidget(undo_);
    barLayout->addWidget(redo_);
    outer->addWidget(bar);

    // Keep the text in a centered column.
    auto* sheet = new QWidget(this);
    auto* sheetLayout = new QHBoxLayout(sheet);
    sheetLayout->setContentsMargins(24, 32, 24, 0);
    auto* column = new QWidget(sheet);
    column->setMaximumWidth(kColumnWidth);
    auto* columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(16);
    title_ = new QLineEdit(column);
    title_->setProperty("role", "heading");
    title_->setPlaceholderText(tr("Untitled"));
    columnLayout->addWidget(title_);
    body_ = new QPlainTextEdit(column);
    body_->setProperty("role", "document");
    body_->setFrameShape(QFrame::NoFrame);
    // Use the same inner space as the title field.
    body_->document()->setDocumentMargin(2);
    columnLayout->addWidget(body_, 1);
    sheetLayout->addStretch(1);
    sheetLayout->addWidget(column, 100);
    sheetLayout->addStretch(1);
    outer->addWidget(sheet, 1);

    saveTimer_.setSingleShot(true);
    saveTimer_.setInterval(kSaveDelayMs);
    connect(&saveTimer_, &QTimer::timeout, this, &DocumentPage::Commit);
    auto changed = [this]
    {
        if (filling_)
            return;
        dirty_ = true;
        ShowState(tr("Editing"));
        saveTimer_.start();
    };
    connect(body_, &QPlainTextEdit::textChanged, this, changed);
    connect(title_, &QLineEdit::textEdited, this, changed);
    connect(title_, &QLineEdit::returnPressed, body_, qOverload<>(&QWidget::setFocus));
    connect(back_, &QPushButton::clicked, this, &DocumentPage::GoBack);
    connect(undo_, &QToolButton::clicked, body_, &QPlainTextEdit::undo);
    connect(redo_, &QToolButton::clicked, body_, &QPlainTextEdit::redo);
    connect(body_, &QPlainTextEdit::undoAvailable, undo_, &QWidget::setEnabled);
    connect(body_, &QPlainTextEdit::redoAvailable, redo_, &QWidget::setEnabled);
    connect(&Theme::Get(), &Theme::Changed, this, &DocumentPage::RefreshIcons);
    RefreshIcons();
}

bool DocumentPage::CanOpen(const Neuron& neuron, const QString& projectFile)
{
    if (neuron.kind == NeuronKind::Note || neuron.kind == NeuronKind::Task)
        return true;
    if (!IsFileKind(neuron.kind) || !IsTextFile(neuron.path))
        return false;
    const QFileInfo info(ResolveFromProject(projectFile, neuron.path));
    return info.isFile() && info.size() <= kMaxFileBytes;
}

bool DocumentPage::Open(quint32 id)
{
    const Neuron* neuron = scene_->GetGraph().FindNeuron(id);
    if (!neuron || !CanOpen(*neuron, scene_->ProjectFile()))
        return false;

    QString text = neuron->text;
    QString path;
    if (IsFileKind(neuron->kind))
    {
        path = ResolveFromProject(scene_->ProjectFile(), neuron->path);
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        text = QString::fromUtf8(file.readAll());
    }

    Commit();
    filling_ = true;
    id_ = id;
    filePath_ = path;
    title_->setText(neuron->title);
    ApplyFont();
    body_->setPlainText(text);
    filling_ = false;
    dirty_ = false;

    ShowState(path.isEmpty() ? KindName(neuron->kind) : QFileInfo(path).fileName());
    undo_->setEnabled(false);
    redo_->setEnabled(false);
    body_->setFocus();
    body_->moveCursor(QTextCursor::End);
    return true;
}

void DocumentPage::Commit()
{
    saveTimer_.stop();
    const Neuron* savedNeuron = scene_->GetGraph().FindNeuron(id_);
    if (!savedNeuron)
        return;
    Neuron after = *savedNeuron;
    after.title = title_->text().trimmed();

    if (filePath_.isEmpty())
    {
        after.text = body_->toPlainText();
    }
    else if (dirty_)
    {
        QSaveFile file(filePath_);
        if (!file.open(QIODevice::WriteOnly) || file.write(body_->toPlainText().toUtf8()) < 0 || !file.commit())
        {
            ShowState(tr("Could not save %1").arg(QFileInfo(filePath_).fileName()));
            return;
        }
    }
    dirty_ = false;
    // Several saves of the same neuron use one undo step.
    scene_->EditNeuron(after);
    ShowState(tr("Saved"));
}

void DocumentPage::Close()
{
    Commit();
    id_ = 0;
    filePath_.clear();
    filling_ = true;
    body_->clear();
    title_->clear();
    filling_ = false;
}

void DocumentPage::GoBack()
{
    Commit();
    emit BackRequested();
}

void DocumentPage::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        GoBack();
        return;
    }
    if (event->matches(QKeySequence::Save))
    {
        Commit();
        return;
    }
    QWidget::keyPressEvent(event);
}

void DocumentPage::ApplyFont()
{
    // Use a fixed-width font for tables and config files.
    const QString suffix = QFileInfo(filePath_).suffix().toLower();
    const bool tabular = !filePath_.isEmpty() && suffix != "md" && suffix != "txt";
    QFont font = tabular ? QFontDatabase::systemFont(QFontDatabase::FixedFont) : Theme::Font(15);
    font.setPixelSize(tabular ? 13 : 15);
    body_->setFont(font);
}

void DocumentPage::RefreshIcons()
{
    // Apply the selected fonts again after the style sheet changes.
    ApplyFont();
    const QColor color = Theme::Get().Tokens().textSecondary;
    back_->setIcon(MakeIcon(IconId::Back, color));
    undo_->setIcon(MakeIcon(IconId::Undo, color));
    redo_->setIcon(MakeIcon(IconId::Redo, color));
}

void DocumentPage::ShowState(const QString& text)
{
    state_->setText(text);
}

} // namespace Ogt
