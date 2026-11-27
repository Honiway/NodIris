/**
 * @file MainWindow.cpp
 * @brief Seven buttons in the toolbar, everything else in one menu or under a shortcut.
 */

#include "ui/MainWindow.h"

#include "core/ProjectFile.h"
#include "ui/AppearanceDialog.h"
#include "ui/DocumentPage.h"
#include "ui/GraphScene.h"
#include "ui/GraphView.h"
#include "ui/Icons.h"
#include "ui/Inspector.h"
#include "ui/NeuronItem.h"
#include "ui/StartPage.h"
#include "ui/Theme.h"
#include "ui/WindowTitleBar.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSet>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>

namespace Ogt
{

namespace
{

constexpr int kSaveDelayMs = 1500;
constexpr int kRecentCount = 8;

QString ProjectFilter()
{
    return QCoreApplication::translate("Ogt", "NodIris projects (*.ogt)");
}

QString ShortcutHint(const QString& name, const QKeySequence& keys)
{
    return keys.isEmpty() ? name : QString("%1  %2").arg(name, keys.toString(QKeySequence::NativeText));
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("NodIris");
    setAcceptDrops(false);
#ifndef Q_OS_MACOS
    setWindowFlag(Qt::FramelessWindowHint);
    setMenuWidget(new WindowTitleBar(this));
#endif

    scene_ = new GraphScene(this);
    view_ = new GraphView(scene_);
    inspector_ = new Inspector(scene_);
    start_ = new StartPage();
    document_ = new DocumentPage(scene_);

    auto* editor = new QSplitter(Qt::Horizontal);
    editor->setChildrenCollapsible(false);
    editor->setHandleWidth(1);
    editor->addWidget(view_);
    editor->addWidget(inspector_);
    editor->setStretchFactor(0, 1);
    editor->setStretchFactor(1, 0);
    editor->setSizes({992, 288});

    pages_ = new QStackedWidget(this);
    pages_->addWidget(start_);
    pages_->addWidget(editor);
    pages_->addWidget(document_);
    setCentralWidget(pages_);

    BuildActions();
    BuildToolbar();
    RefreshIcons();
    toolbar_->setVisible(false);
    statusBar()->setSizeGripEnabled(true);

    saveTimer_.setSingleShot(true);
    saveTimer_.setInterval(kSaveDelayMs);
    connect(&saveTimer_, &QTimer::timeout, this, [this] { Save(); });
    connect(scene_, &GraphScene::Changed, this, &MainWindow::ScheduleSave);
    connect(scene_, &GraphScene::NeuronActivated, this, &MainWindow::Activate);
    connect(scene_, &GraphScene::FolderShown, this, &MainWindow::RefreshBreadcrumb);
    // A folder renamed in the inspector shows its new name in the path at once.
    connect(scene_, &GraphScene::Changed, this, &MainWindow::RefreshBreadcrumb);
    connect(&Theme::Get(), &Theme::Changed, this, &MainWindow::RefreshBreadcrumb);
    connect(scene_, &GraphScene::EmptyDoubleClicked, this, &MainWindow::ShowAddMenu);
    connect(scene_, &GraphScene::Feedback, statusBar(), [this](const QString& message)
    {
        statusBar()->showMessage(message, 3000);
    });
    connect(view_, &GraphView::ContextMenuRequested, this, &MainWindow::ShowContextMenu);
    connect(view_, &GraphView::FilesDropped, scene_, &GraphScene::AddFiles);
    connect(inspector_, &Inspector::OpenRequested, this, &MainWindow::Activate);
    connect(document_, &DocumentPage::BackRequested, this, &MainWindow::LeaveDocument);
    connect(&Theme::Get(), &Theme::Changed, this, &MainWindow::RefreshIcons);

    connect(start_, &StartPage::CreateRequested, this, &MainWindow::AskNewProject);
    connect(start_, &StartPage::OpenRequested, this, &MainWindow::AskOpenProject);
    connect(start_, &StartPage::RecentChosen, this, [this](const QString& path) { OpenProject(path); });
    start_->SetRecent(RecentProjects());

    QSettings settings;
    if (!restoreGeometry(settings.value("window/geometry").toByteArray()))
        resize(1280, 800);
}

void MainWindow::BuildActions()
{
    auto* newProject = new QAction(tr("New project"), this);
    newProject->setShortcut(QKeySequence::New);
    connect(newProject, &QAction::triggered, this, &MainWindow::AskNewProject);

    auto* openProject = new QAction(tr("Open project"), this);
    openProject->setShortcut(QKeySequence::Open);
    connect(openProject, &QAction::triggered, this, &MainWindow::AskOpenProject);

    auto* save = new QAction(tr("Save"), this);
    save->setShortcut(QKeySequence::Save);
    connect(save, &QAction::triggered, this, [this] { Save(true); });

    auto* close = new QAction(tr("Close project"), this);
    close->setShortcut(QKeySequence::Close);
    connect(close, &QAction::triggered, this, &MainWindow::CloseProject);

    QAction* undo = scene_->UndoStack()->createUndoAction(this, tr("Undo"));
    undo->setShortcut(QKeySequence::Undo);
    QAction* redo = scene_->UndoStack()->createRedoAction(this, tr("Redo"));
    redo->setShortcut(QKeySequence::Redo);

    auto* cut = new QAction(tr("Cut"), this);
    cut->setShortcut(QKeySequence::Cut);
    connect(cut, &QAction::triggered, this, &MainWindow::Cut);
    auto* copy = new QAction(tr("Copy"), this);
    copy->setShortcut(QKeySequence::Copy);
    connect(copy, &QAction::triggered, this, &MainWindow::Copy);
    auto* paste = new QAction(tr("Paste"), this);
    paste->setShortcut(QKeySequence::Paste);
    connect(paste, &QAction::triggered, this, &MainWindow::Paste);
    auto* selectAll = new QAction(tr("Select all"), this);
    selectAll->setShortcut(QKeySequence::SelectAll);
    connect(selectAll, &QAction::triggered, this, &MainWindow::SelectAll);

    auto* up = new QAction(tr("Leave folder"), this);
    up->setShortcut(QKeySequence("Alt+Up"));
    connect(up, &QAction::triggered, view_, &GraphView::GoUp);

    auto* fit = new QAction(tr("Show everything"), this);
    fit->setShortcut(QKeySequence("Ctrl+0"));
    connect(fit, &QAction::triggered, view_, &GraphView::FitAll);

    auto* zoomIn = new QAction(tr("Zoom in"), this);
    zoomIn->setShortcuts({QKeySequence::ZoomIn, QKeySequence("Ctrl+=")});
    connect(zoomIn, &QAction::triggered, this, [this] { view_->ZoomBy(1.25); });
    auto* zoomOut = new QAction(tr("Zoom out"), this);
    zoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOut, &QAction::triggered, this, [this] { view_->ZoomBy(0.8); });

    auto* details = new QAction(tr("Details panel"), this);
    details->setCheckable(true);
    details->setChecked(true);
    details->setShortcut(QKeySequence("Ctrl+I"));
    connect(details, &QAction::toggled, inspector_, &QWidget::setVisible);
    details_ = details;

    auto* duplicate = new QAction(tr("Duplicate"), this);
    duplicate->setShortcut(QKeySequence("Ctrl+D"));
    connect(duplicate, &QAction::triggered, this, [this]
    {
        if (const std::optional<quint32> id = scene_->SelectedNeuron(); id && pages_->currentIndex() == 1)
            Duplicate(*id);
    });

    auto* exportGraph = new QAction(tr("Export graph"), this);
    exportGraph->setShortcut(QKeySequence("Ctrl+Shift+E"));
    connect(exportGraph, &QAction::triggered, this, &MainWindow::ExportGraph);

    auto* appearance = new QAction(tr("Customize appearance"), this);
    connect(appearance, &QAction::triggered, this, [this]
    {
        AppearanceDialog dialog(this);
        dialog.exec();
    });

    auto* followSystem = new QAction(tr("Follow system theme"), this);
    followSystem->setCheckable(true);
    followSystem->setChecked(Theme::Get().Mode() == ThemeMode::System);
    connect(followSystem, &QAction::triggered, this, [](bool on)
    {
        // Unticking keeps whatever is showing now instead of flipping the screen.
        const ThemeMode current = Theme::Get().Tokens().dark ? ThemeMode::Dark : ThemeMode::Light;
        Theme::Get().Apply(*qApp, on ? ThemeMode::System : current);
    });
    connect(&Theme::Get(), &Theme::Changed, followSystem, [followSystem]
    {
        followSystem->setChecked(Theme::Get().Mode() == ThemeMode::System);
    });

    auto* about = new QAction(tr("About NodIris"), this);
    connect(about, &QAction::triggered, this, &MainWindow::ShowAbout);

    add_ = new QAction(tr("Add"), this);
    connect(add_, &QAction::triggered, this, [this]
    {
        const QPointF center = view_->VisibleCenter();
        QWidget* button = toolbar_->widgetForAction(add_);
        ShowAddMenu(center, button->mapToGlobal(QPoint(0, button->height() + 4)));
    });

    connect_ = new QAction(tr("Link"), this);
    connect_->setCheckable(true);
    connect(connect_, &QAction::toggled, scene_, &GraphScene::SetLinkToolEnabled);
    connect(scene_, &GraphScene::LinkToolEnabledChanged, connect_, &QAction::setChecked);

    connectMany_ = new QAction(tr("Many to one"), this);
    connectMany_->setCheckable(true);
    connect(connectMany_, &QAction::toggled, scene_, &GraphScene::SetMultiLinkToolEnabled);
    connect(scene_, &GraphScene::MultiLinkToolEnabledChanged, connectMany_, &QAction::setChecked);

    find_ = new QAction(tr("Search"), this);
    find_->setShortcut(QKeySequence::Find);
    connect(find_, &QAction::triggered, this, &MainWindow::ShowSearch);

    theme_ = new QAction(tr("Light or dark"), this);
    theme_->setShortcut(QKeySequence("Ctrl+Shift+L"));
    connect(theme_, &QAction::triggered, this, [] { Theme::Get().Toggle(*qApp); });

    moreMenu_ = new QMenu(this);
    moreMenu_->addAction(newProject);
    moreMenu_->addAction(openProject);
    moreMenu_->addAction(close);
    moreMenu_->addSeparator();
    moreMenu_->addAction(undo);
    moreMenu_->addAction(redo);
    moreMenu_->addSeparator();
    moreMenu_->addAction(cut);
    moreMenu_->addAction(copy);
    moreMenu_->addAction(paste);
    moreMenu_->addAction(selectAll);
    moreMenu_->addSeparator();
    moreMenu_->addAction(up);
    moreMenu_->addAction(fit);
    moreMenu_->addAction(details);
    moreMenu_->addAction(exportGraph);
    moreMenu_->addSeparator();
    moreMenu_->addAction(appearance);
    moreMenu_->addAction(followSystem);
    moreMenu_->addAction(about);

    more_ = new QAction(tr("Menu"), this);
    more_->setMenu(moreMenu_);

    // Shortcuts only fire for actions that belong to a visible widget.
    addActions({newProject, openProject, save, close, undo, redo, cut, copy, paste, selectAll, up, fit, zoomIn,
                zoomOut, details, duplicate, exportGraph});
    addActions({find_, theme_});
}

void MainWindow::BuildToolbar()
{
    toolbar_ = new QToolBar(this);
    toolbar_->setMovable(false);
    toolbar_->setFloatable(false);
    toolbar_->setIconSize(QSize(16, 16));
    toolbar_->setContextMenuPolicy(Qt::PreventContextMenu);
    addToolBar(Qt::TopToolBarArea, toolbar_);

    breadcrumb_ = new QLabel(toolbar_);
    breadcrumb_->setFont(Theme::Font(Theme::kBody, true));
    breadcrumb_->setContentsMargins(8, 0, 0, 0);
    breadcrumb_->setTextFormat(Qt::RichText);
    breadcrumb_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    connect(breadcrumb_, &QLabel::linkActivated, this, [this](const QString& link)
    {
        scene_->ShowFolder(link.toUInt());
        view_->FitAll();
    });
    toolbar_->addWidget(breadcrumb_);

    auto* spacer = new QWidget(toolbar_);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar_->addWidget(spacer);

    search_ = new QLineEdit(toolbar_);
    search_->setPlaceholderText(tr("Search"));
    search_->setMinimumWidth(120);
    search_->setMaximumWidth(240);
    search_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    search_->setClearButtonEnabled(true);
    searchIcon_ = search_->addAction(QIcon(), QLineEdit::LeadingPosition);
    searchField_ = toolbar_->addWidget(search_);
    searchField_->setVisible(false);
    connect(search_, &QLineEdit::textChanged, this, [this](const QString& text)
    {
        nextMatch_ = 0;
        scene_->SetFilter(text);
    });
    connect(search_, &QLineEdit::returnPressed, this, &MainWindow::JumpToNextMatch);

    for (QAction* action : {add_, connect_, connectMany_, find_, theme_, more_})
    {
        action->setToolTip(ShortcutHint(action->text(), action->shortcut()));
        toolbar_->addAction(action);
    }
    if (auto* button = qobject_cast<QToolButton*>(toolbar_->widgetForAction(more_)))
        button->setPopupMode(QToolButton::InstantPopup);

    // Escape in the search field closes it.
    auto* escape = new QAction(search_);
    escape->setShortcut(Qt::Key_Escape);
    escape->setShortcutContext(Qt::WidgetShortcut);
    connect(escape, &QAction::triggered, this, &MainWindow::HideSearch);
    search_->addAction(escape);
}

void MainWindow::RefreshIcons()
{
    const QColor color = Theme::Get().Tokens().textSecondary;
    add_->setIcon(MakeIcon(IconId::Add, color));
    connect_->setIcon(MakeIcon(IconId::Connect, color));
    connectMany_->setIcon(MakeIcon(IconId::ConnectMany, color));
    find_->setIcon(MakeIcon(IconId::Search, color));
    theme_->setIcon(MakeIcon(IconId::Theme, color));
    more_->setIcon(MakeIcon(IconId::More, color));
    searchIcon_->setIcon(MakeIcon(IconId::Search, Theme::Get().Tokens().textMuted));
}

void MainWindow::AskNewProject()
{
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString suggested = QDir(folder).filePath(tr("Untitled") + ".ogt");
    QString path = QFileDialog::getSaveFileName(this, tr("New project"), suggested, ProjectFilter());
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().compare(kProjectSuffix, Qt::CaseInsensitive) != 0)
        path += QString(".") + kProjectSuffix;
    CreateProject(path);
}

void MainWindow::AskOpenProject()
{
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getOpenFileName(this, tr("Open project"), folder, ProjectFilter());
    if (!path.isEmpty())
        OpenProject(path);
}

bool MainWindow::CreateProject(const QString& path)
{
    QString error;
    if (!SaveProject(path, Graph(), error))
    {
        QMessageBox::warning(this, tr("New project"), tr("Could not create %1.\n%2").arg(path, error));
        return false;
    }
    return OpenProject(path);
}

bool MainWindow::OpenProject(const QString& path)
{
    if (!scene_->ProjectFile().isEmpty())
        Save();
    document_->Close();
    scene_->SetLinkToolEnabled(false);
    scene_->SetMultiLinkToolEnabled(false);

    Graph graph;
    QString error;
    if (!LoadProject(path, graph, error))
    {
        QMessageBox::warning(this, tr("Open project"), tr("Could not open %1.\n%2").arg(path, error));
        return false;
    }

    scene_->Load(std::move(graph), QFileInfo(path).absoluteFilePath());
    const QString name = QFileInfo(path).completeBaseName();
    projectName_ = name;
    RefreshBreadcrumb();
    setWindowTitle(QString("%1  ·  NodIris").arg(name));
    setWindowFilePath(path);
    toolbar_->setVisible(true);
    pages_->setCurrentIndex(1);
    RememberRecent(QFileInfo(path).absoluteFilePath());
    lastError_.clear();
    // Fit once the view has its real size, or it frames into a zero-sized viewport.
    QTimer::singleShot(0, view_, &GraphView::FitAll);
    view_->setFocus();
    return true;
}

void MainWindow::CloseProject()
{
    if (scene_->ProjectFile().isEmpty())
        return;
    Save();
    document_->Close();
    HideSearch();
    scene_->Load({}, {});
    toolbar_->setVisible(false);
    setWindowTitle("NodIris");
    start_->SetRecent(RecentProjects());
    pages_->setCurrentIndex(0);
}

bool MainWindow::Save(bool showFeedback)
{
    // Ctrl+S or closing the window while writing: the page's pending text goes in first.
    if (pages_->currentWidget() == document_)
        document_->Commit();
    saveTimer_.stop();
    if (scene_->ProjectFile().isEmpty())
        return false;
    QString error;
    if (SaveProject(scene_->ProjectFile(), scene_->GetGraph(), error))
    {
        lastError_.clear();
        if (showFeedback)
            statusBar()->showMessage(tr("Project saved."), 3000);
        return true;
    }
    // Autosave runs every few seconds: say it once, not on every keystroke.
    if (error != lastError_)
    {
        lastError_ = error;
        QMessageBox::warning(this, tr("Save"), tr("The project could not be saved.\n%1").arg(error));
    }
    return false;
}

void MainWindow::ScheduleSave()
{
    if (!scene_->ProjectFile().isEmpty())
        saveTimer_.start();
}

QStringList MainWindow::RecentProjects() const
{
    QStringList paths = QSettings().value("recent").toStringList();
    paths.removeIf([](const QString& path) { return !QFileInfo::exists(path); });
    return paths;
}

void MainWindow::RememberRecent(const QString& path)
{
    QStringList paths = RecentProjects();
    paths.removeAll(path);
    paths.prepend(path);
    while (paths.size() > kRecentCount)
        paths.removeLast();
    QSettings().setValue("recent", paths);
}

void MainWindow::FillAddMenu(QMenu& menu, QPointF scenePos)
{
    const QColor color = Theme::Get().Tokens().textSecondary;
    for (NeuronKind kind : {NeuronKind::Note, NeuronKind::Task, NeuronKind::Link, NeuronKind::Folder})
    {
        QAction* action = menu.addAction(MakeKindIcon(kind, color), KindName(kind));
        connect(action, &QAction::triggered, this, [this, kind, scenePos] { AddNeuronAt(kind, scenePos); });
    }
    menu.addSeparator();
    QAction* files = menu.addAction(MakeKindIcon(NeuronKind::File, color), tr("Files"));
    connect(files, &QAction::triggered, this, [this, scenePos] { AddFilesAt(scenePos); });
}

void MainWindow::ShowAddMenu(QPointF scenePos, QPoint screenPos)
{
    QMenu menu(this);
    FillAddMenu(menu, scenePos);
    menu.exec(screenPos);
}

void MainWindow::ShowContextMenu(QPointF scenePos, QPoint screenPos)
{
    QMenu menu(this);
    const QColor color = Theme::Get().Tokens().textSecondary;
    std::vector<quint32> selected;
    for (QGraphicsItem* item : scene_->selectedItems())
    {
        if (item->type() == NeuronItem::Type)
            selected.push_back(static_cast<NeuronItem*>(item)->Id());
    }
    const std::optional<Link> link = scene_->SelectedLink();

    if (link)
    {
        // Link kinds are few enough to pick right here, checked like radio buttons.
        QMenu* kinds = menu.addMenu(tr("Kind of link"));
        for (LinkKind kind : {LinkKind::Relates, LinkKind::LeadsTo, LinkKind::PartOf})
        {
            QAction* action = kinds->addAction(LinkKindName(kind));
            action->setCheckable(true);
            action->setChecked(link->kind == kind);
            connect(action, &QAction::triggered, this, [this, link, kind]
            {
                Link after = *link;
                after.kind = kind;
                scene_->EditLink(after);
            });
        }
        menu.addAction(tr("Properties"), QKeySequence("Ctrl+I"), this, &MainWindow::ShowProperties);
        menu.addSeparator();
        menu.addAction(tr("Delete link"), QKeySequence::Delete, scene_, &GraphScene::DeleteSelection);
    }
    else if (selected.empty())
    {
        QMenu* add = menu.addMenu(MakeIcon(IconId::Add, color), tr("New"));
        FillAddMenu(*add, scenePos);
        menu.addSeparator();
        menu.addAction(tr("Show everything"), QKeySequence("Ctrl+0"), view_, &GraphView::FitAll);
        if (scene_->CurrentFolder() != 0)
            menu.addAction(tr("Leave folder"), QKeySequence("Alt+Up"), view_, &GraphView::GoUp);
    }
    else
    {
        if (selected.size() == 1)
        {
            const quint32 id = selected.front();
            const Neuron* neuron = scene_->GetGraph().FindNeuron(id);
            QAction* open = menu.addAction(tr("Open"), this, [this, id] { Activate(id); });
            // Bold, like the default action in a file manager: it is what a double-click does.
            QFont bold = open->font();
            bold.setBold(true);
            open->setFont(bold);
            if (IsFileKind(neuron->kind))
            {
                const QString path = ResolveFromProject(scene_->ProjectFile(), neuron->path);
                QAction* reveal = menu.addAction(tr("Show in folder"), this, [path]
                {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
                });
                reveal->setEnabled(QFileInfo::exists(path));
            }
            menu.addSeparator();
            menu.addAction(tr("Rename"), QKeySequence(Qt::Key_F2), this, [this, id] { view_->StartRename(id); });
            menu.addAction(tr("Duplicate"), QKeySequence("Ctrl+D"), this, [this, id] { Duplicate(id); });
        }

        // Folders on this level the selection can go into, the selection itself left out.
        QMenu* into = menu.addMenu(tr("Move into"));
        for (quint32 id : scene_->GetGraph().Children(scene_->CurrentFolder()))
        {
            const Neuron* folder = scene_->GetGraph().FindNeuron(id);
            if (folder->kind != NeuronKind::Folder || std::find(selected.begin(), selected.end(), id) != selected.end())
                continue;
            const QString name = folder->title.isEmpty() ? KindName(folder->kind) : folder->title;
            into->addAction(MakeKindIcon(NeuronKind::Folder, color), name, this,
                            [this, selected, id] { scene_->MoveIntoFolder(selected, id); });
        }
        into->setEnabled(!into->isEmpty());

        if (selected.size() == 1)
            menu.addAction(tr("Properties"), QKeySequence("Ctrl+I"), this, &MainWindow::ShowProperties);
        menu.addSeparator();
        const QString text =
            selected.size() == 1 ? tr("Delete") : tr("Delete %n neurons", nullptr, int(selected.size()));
        menu.addAction(text, QKeySequence::Delete, scene_, &GraphScene::DeleteSelection);
    }
    menu.exec(screenPos);
}

void MainWindow::ShowProperties()
{
    // Through the action, so its check mark in the menu stays right.
    details_->setChecked(true);
    if (scene_->SelectedNeuron())
        inspector_->FocusTitle();
}

void MainWindow::Duplicate(quint32 id)
{
    const Neuron* neuron = scene_->GetGraph().FindNeuron(id);
    // A folder's copy would be empty, which is not what anyone means by a copy of a folder.
    if (!neuron || neuron->kind == NeuronKind::Folder)
        return;
    Neuron copy = *neuron;
    copy.position += QPoint(24, 24);
    scene_->SelectOnly(scene_->AddNeuron(copy));
}

bool MainWindow::InvokeTextEdit(const char* method)
{
    QWidget* focus = QApplication::focusWidget();
    if (!qobject_cast<QLineEdit*>(focus) && !qobject_cast<QTextEdit*>(focus)
        && !qobject_cast<QPlainTextEdit*>(focus))
        return false;
    return QMetaObject::invokeMethod(focus, method, Qt::DirectConnection);
}

void MainWindow::Copy()
{
    if (InvokeTextEdit("copy"))
        return;
    if (pages_->currentIndex() != 1)
        return;

    copiedNeurons_.clear();
    copiedLinks_.clear();
    QSet<quint32> ids;
    for (QGraphicsItem* item : scene_->selectedItems())
    {
        if (item->type() != NeuronItem::Type)
            continue;
        const quint32 id = static_cast<NeuronItem*>(item)->Id();
        ids.insert(id);
        copiedNeurons_.push_back(*scene_->GetGraph().FindNeuron(id));
    }
    for (const Link& link : scene_->GetGraph().Links())
    {
        if (ids.contains(link.from) && ids.contains(link.to))
            copiedLinks_.push_back(link);
    }
    if (copiedNeurons_.empty())
    {
        statusBar()->showMessage(tr("Select one or more neurons first."), 3000);
        return;
    }
    statusBar()->showMessage(tr("%n neuron(s) copied.", nullptr, int(copiedNeurons_.size())), 3000);
}

void MainWindow::Cut()
{
    if (InvokeTextEdit("cut"))
        return;
    if (pages_->currentIndex() != 1)
        return;
    Copy();
    if (copiedNeurons_.empty())
        return;
    const int count = int(copiedNeurons_.size());
    scene_->DeleteSelection();
    statusBar()->showMessage(tr("%n neuron(s) cut.", nullptr, count), 3000);
}

void MainWindow::Paste()
{
    if (InvokeTextEdit("paste"))
        return;
    if (pages_->currentIndex() != 1)
        return;
    if (copiedNeurons_.empty())
    {
        statusBar()->showMessage(tr("Nothing to paste."), 3000);
        return;
    }
    scene_->Paste(copiedNeurons_, copiedLinks_, view_->VisibleCenter());
    statusBar()->showMessage(tr("%n neuron(s) pasted.", nullptr, int(copiedNeurons_.size())), 3000);
}

void MainWindow::SelectAll()
{
    if (InvokeTextEdit("selectAll"))
        return;
    if (pages_->currentIndex() != 1)
        return;
    int count = 0;
    scene_->clearSelection();
    for (QGraphicsItem* item : scene_->items())
    {
        if (item->type() == NeuronItem::Type)
        {
            item->setSelected(true);
            ++count;
        }
    }
    statusBar()->showMessage(tr("%n neuron(s) selected.", nullptr, count), 3000);
}

void MainWindow::AddNeuronAt(NeuronKind kind, QPointF scenePos)
{
    Neuron neuron;
    neuron.kind = kind;
    neuron.title = KindName(kind);
    // Centre the new card on the point that was clicked.
    neuron.position = (scenePos - QPointF(NeuronItem::kWidth / 2.0, 24)).toPoint();
    const quint32 id = scene_->AddNeuron(neuron);
    scene_->SelectOnly(id);
    inspector_->setVisible(true);
    inspector_->FocusTitle();
}

void MainWindow::AddFilesAt(QPointF scenePos)
{
    const QString folder = QFileInfo(scene_->ProjectFile()).absolutePath();
    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Add files"), folder);
    scene_->AddFiles(paths, scenePos - QPointF(NeuronItem::kWidth / 2.0, 24));
}

void MainWindow::Activate(quint32 id)
{
    const Neuron* neuron = scene_->GetGraph().FindNeuron(id);
    if (!neuron)
        return;
    // Notes, tasks and text files are written in the app; everything else goes to its own program.
    if (DocumentPage::CanOpen(*neuron, scene_->ProjectFile()))
    {
        ShowDocument(id);
        return;
    }
    if (IsFileKind(neuron->kind))
    {
        const QString path = ResolveFromProject(scene_->ProjectFile(), neuron->path);
        if (QFileInfo::exists(path))
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        return;
    }
    if (neuron->kind == NeuronKind::Link && !neuron->text.isEmpty())
    {
        QDesktopServices::openUrl(QUrl::fromUserInput(neuron->text));
        return;
    }
    if (neuron->kind == NeuronKind::Folder)
    {
        scene_->ShowFolder(id);
        view_->FitAll();
        return;
    }
    scene_->SelectOnly(id);
    inspector_->setVisible(true);
    inspector_->FocusTitle();
}

void MainWindow::ShowDocument(quint32 id)
{
    if (!document_->Open(id))
        return;
    HideSearch();
    toolbar_->setVisible(false);
    pages_->setCurrentWidget(document_);
}

void MainWindow::LeaveDocument()
{
    const quint32 id = document_->Id();
    document_->Close();
    toolbar_->setVisible(true);
    pages_->setCurrentIndex(1);
    // Back where we were, with the neuron we just wrote in still selected.
    scene_->SelectOnly(id);
    view_->setFocus();
}

void MainWindow::ShowSearch()
{
    if (scene_->ProjectFile().isEmpty() || pages_->currentIndex() != 1)
        return;
    searchField_->setVisible(true);
    search_->setFocus();
    search_->selectAll();
}

void MainWindow::HideSearch()
{
    search_->clear();
    searchField_->setVisible(false);
    view_->setFocus();
}

void MainWindow::JumpToNextMatch()
{
    const std::vector<quint32> matches = scene_->Matches();
    if (matches.empty())
        return;
    const quint32 id = matches[nextMatch_ % matches.size()];
    ++nextMatch_;
    // The match may sit in another folder: go there first.
    const quint32 folder = scene_->GetGraph().FindNeuron(id)->parent;
    if (folder != scene_->CurrentFolder())
        scene_->ShowFolder(folder);
    scene_->SelectOnly(id);
    view_->CenterOnNeuron(id);
}

void MainWindow::ExportGraph()
{
    const QRectF whole = scene_->ContentRect().adjusted(-32, -32, 32, 32);
    if (whole.isEmpty())
        return;

    enum Scope { WholeGraph, CurrentView, Selection };
    QDialog options(this);
    options.setWindowTitle(tr("Export graph"));
    auto* layout = new QVBoxLayout(&options);
    auto* form = new QFormLayout();
    auto* scope = new QComboBox(&options);
    scope->addItem(tr("Entire graph"), WholeGraph);
    scope->addItem(tr("Current view"), CurrentView);
    const bool hasSelection = !scene_->selectedItems().isEmpty();
    scope->addItem(tr("Selected area"), Selection);
    if (!hasSelection)
        scope->setItemData(2, 0, Qt::UserRole - 1);
    else
        scope->setCurrentIndex(2);
    auto* format = new QComboBox(&options);
    format->addItem(tr("PNG image"), QString("png"));
    format->addItem(tr("PDF document"), QString("pdf"));
    form->addRow(tr("Area"), scope);
    form->addRow(tr("Format"), format);
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &options);
    connect(buttons, &QDialogButtonBox::accepted, &options, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &options, &QDialog::reject);
    layout->addWidget(buttons);
    if (options.exec() != QDialog::Accepted)
        return;

    QRectF content = whole;
    if (scope->currentData().toInt() == CurrentView)
        content = view_->mapToScene(view_->viewport()->rect()).boundingRect().intersected(whole);
    else if (scope->currentData().toInt() == Selection)
    {
        content = {};
        for (QGraphicsItem* item : scene_->selectedItems())
            content |= item->sceneBoundingRect();
        content = content.adjusted(-24, -24, 24, 24);
    }
    if (content.isEmpty())
        return;

    const QString extension = format->currentData().toString();
    const QString suggested = QFileInfo(scene_->ProjectFile()).absoluteDir().filePath(projectName_ + "." + extension);
    const QString filter = extension == "pdf" ? tr("PDF document (*.pdf)") : tr("PNG image (*.png)");
    QString path = QFileDialog::getSaveFileName(this, tr("Export graph"), suggested, filter);
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().compare(extension, Qt::CaseInsensitive) != 0)
        path += "." + extension;

    const QList<QGraphicsItem*> selection = scene_->selectedItems();
    scene_->clearSelection();
    bool written = false;
    if (extension == "pdf")
    {
        QPdfWriter writer(path);
        writer.setTitle(projectName_);
        writer.setCreator("NodIris");
        writer.setPageSize(QPageSize(content.size(), QPageSize::Point, "Graph"));
        writer.setPageMargins({}, QPageLayout::Point);
        QPainter painter(&writer);
        painter.fillRect(QRectF(0, 0, writer.width(), writer.height()), Theme::Get().Tokens().canvas);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
        scene_->render(&painter, QRectF(0, 0, writer.width(), writer.height()), content);
        written = painter.end();
    }
    else
    {
        // Twice the size, so the picture stays sharp in a document or on a slide.
        const qreal scale = 2.0;
        QImage image((content.size() * scale).toSize(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Theme::Get().Tokens().canvas);
        QPainter painter(&image);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
        scene_->render(&painter, QRectF(QPointF(0, 0), image.size()), content);
        painter.end();
        written = image.save(path);
    }
    for (QGraphicsItem* item : selection)
        item->setSelected(true);
    if (!written)
        QMessageBox::warning(this, tr("Export graph"), tr("Could not write %1.").arg(path));
}

void MainWindow::RefreshBreadcrumb()
{
    const ThemeTokens& theme = Theme::Get().Tokens();
    const QString link = QString("<a href=\"%1\" style=\"color:%2; text-decoration:none\">%3</a>");
    const QString separator =
        QString("<span style=\"color:%1\">&nbsp;&nbsp;/&nbsp;&nbsp;</span>").arg(theme.textMuted.name());
    const std::vector<quint32> path = scene_->FolderPath();

    // Every step is a link except the last one, which is where we already are.
    QString html = path.empty() ? projectName_.toHtmlEscaped()
                                : link.arg("0", theme.textSecondary.name(), projectName_.toHtmlEscaped());
    for (size_t i = 0; i < path.size(); ++i)
    {
        const Neuron* folder = scene_->GetGraph().FindNeuron(path[i]);
        const QString name = (folder->title.isEmpty() ? KindName(folder->kind) : folder->title).toHtmlEscaped();
        html += separator;
        html += i + 1 == path.size() ? name : link.arg(QString::number(path[i]), theme.textSecondary.name(), name);
    }
    breadcrumb_->setText(html);
}

void MainWindow::ShowAbout()
{
    QMessageBox::about(this, tr("About NodIris"),
                       tr("<p><b>NodIris %1</b></p>"
                          "<p>An open-source and lightweight mind-mapping software for planning and organizing projects or ideas.</p>"
                          "<p>Created by <b>Honiway</b>.</p>"
                          "<p>Icons from <b>Papicons</b> by Association Papillon.</p>"
                          "<p>Free software under the MIT licence. Built with Qt %2.</p>")
                           .arg(QCoreApplication::applicationVersion(), QT_VERSION_STR));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    Save();
    QSettings().setValue("window/geometry", saveGeometry());
    event->accept();
}

} // namespace Ogt
