/**
 * @file DocumentPage.h
 * @brief Edits a note, task or text file on a full page.
 */

#ifndef OGT_DOCUMENT_PAGE_H
#define OGT_DOCUMENT_PAGE_H

#include "core/Graph.h"

#include <QTimer>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QToolButton;

namespace Ogt
{

class GraphScene;

class DocumentPage final : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentPage(GraphScene* scene, QWidget* parent = nullptr);

    /// True for notes, tasks, and text files that exist and are small enough to edit here.
    static bool CanOpen(const Neuron& neuron, const QString& projectFile);

    /// Loads a neuron and returns false when this page cannot edit it.
    bool Open(quint32 id);
    /// Saves pending changes when a document is open.
    void Commit();
    /// Saves and lets go of the neuron, so nothing is written once the graph is replaced.
    void Close();
    quint32 Id() const { return id_; }

signals:
    /// Reports that the user saved the document and returned to the graph.
    void BackRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void GoBack();
    void RefreshIcons();
    void ApplyFont();
    void ShowState(const QString& text);

    GraphScene* scene_;
    QPushButton* back_;
    QLineEdit* title_;
    QLabel* state_;
    QToolButton* undo_;
    QToolButton* redo_;
    QPlainTextEdit* body_;
    QTimer saveTimer_;

    quint32 id_ = 0;
    /// Absolute path when a text file is open, empty for a note or a task.
    QString filePath_;
    bool filling_ = false;
    /// The text changed since it was loaded or last written to the file.
    bool dirty_ = false;
};

} // namespace Ogt

#endif // OGT_DOCUMENT_PAGE_H
