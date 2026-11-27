/**
 * @file MainWindow.h
 * @brief The one window: the start page, or a project with its canvas and inspector.
 */

#ifndef OGT_MAIN_WINDOW_H
#define OGT_MAIN_WINDOW_H

#include "core/Graph.h"

#include <QMainWindow>
#include <QTimer>

class QAction;
class QLabel;
class QLineEdit;
class QMenu;
class QStackedWidget;
class QToolBar;
class QToolButton;

namespace Ogt
{

class DocumentPage;
class GraphScene;
class GraphView;
class Inspector;
class StartPage;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    bool OpenProject(const QString& path);
    bool CreateProject(const QString& path);
    GraphScene* Scene() const { return scene_; }
    GraphView* View() const { return view_; }
    DocumentPage* Document() const { return document_; }

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void BuildActions();
    void BuildToolbar();
    void RefreshIcons();

    void AskNewProject();
    void AskOpenProject();
    void CloseProject();
    bool Save(bool showFeedback = false);
    void ScheduleSave();
    void RememberRecent(const QString& path);
    QStringList RecentProjects() const;

    void AddNeuronAt(NeuronKind kind, QPointF scenePos);
    void AddFilesAt(QPointF scenePos);
    void ShowAddMenu(QPointF scenePos, QPoint screenPos);
    void ShowContextMenu(QPointF scenePos, QPoint screenPos);
    /// The kinds to create and "Files...", shared by the add menu and the right-click menu.
    void FillAddMenu(QMenu& menu, QPointF scenePos);
    void ShowProperties();
    void Duplicate(quint32 id);
    void Copy();
    void Cut();
    void Paste();
    void SelectAll();
    bool InvokeTextEdit(const char* method);
    void Activate(quint32 id);
    void ShowDocument(quint32 id);
    void LeaveDocument();
    void ShowSearch();
    void HideSearch();
    void JumpToNextMatch();
    void ExportGraph();
    void ShowAbout();
    void RefreshBreadcrumb();

    GraphScene* scene_;
    GraphView* view_;
    Inspector* inspector_;
    StartPage* start_;
    DocumentPage* document_;
    QStackedWidget* pages_;
    QToolBar* toolbar_;
    /// Project name and the folders down to the one on screen, each one a link back up.
    QLabel* breadcrumb_;
    QString projectName_;
    QLineEdit* search_;
    QAction* searchField_ = nullptr;
    QAction* searchIcon_ = nullptr;

    QAction* details_;
    QAction* add_;
    QAction* connect_;
    QAction* connectMany_;
    QAction* find_;
    QAction* theme_;
    QAction* more_;
    QMenu* moreMenu_;

    QTimer saveTimer_;
    QString lastError_;
    size_t nextMatch_ = 0;
    std::vector<Neuron> copiedNeurons_;
    std::vector<Link> copiedLinks_;
};

} // namespace Ogt

#endif // OGT_MAIN_WINDOW_H
