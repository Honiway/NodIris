/**
 * @file Inspector.h
 * @brief Edits the selected neuron or link in the right panel.
 */

#ifndef OGT_INSPECTOR_H
#define OGT_INSPECTOR_H

#include "core/Graph.h"

#include <QFrame>

class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QWidget;

namespace Ogt
{

class GraphScene;

class Inspector final : public QFrame
{
    Q_OBJECT

public:
    explicit Inspector(GraphScene* scene, QWidget* parent = nullptr);

    /// Puts the cursor in the title, with the text selected, for a neuron that was just added.
    void FocusTitle();

signals:
    void OpenRequested(quint32 id);

private:
    void Refresh();
    void ShowNeuron(const Neuron& neuron);
    void ShowLink(const Link& link);
    void Commit();
    void CommitLink();
    void Relocate();
    void ChooseIconImage();
    void ResetIcon();
    void RefreshIcons();

    GraphScene* scene_;
    // Set while the fields are being filled from the graph, so filling them is not taken for an edit.
    bool filling_ = false;
    quint32 neuronId_ = 0;
    QByteArray shownIconImage_;

    QStackedWidget* pages_;

    QLabel* kind_;
    QLineEdit* title_;
    QComboBox* icon_;
    QPushButton* chooseIcon_;
    QPushButton* resetIcon_;
    QWidget* fileRow_;
    QLabel* filePath_;
    QPushButton* open_;
    QPushButton* relocate_;
    QLabel* preview_;
    QLineEdit* address_;
    QWidget* taskRow_;
    QComboBox* status_;
    QDateEdit* due_;
    QLabel* textLabel_;
    QPlainTextEdit* text_;
    QLabel* connections_;

    QLabel* linkEnds_;
    QComboBox* linkKind_;
    QLineEdit* linkLabel_;
};

} // namespace Ogt

#endif // OGT_INSPECTOR_H
