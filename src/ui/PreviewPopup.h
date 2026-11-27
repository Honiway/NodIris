/**
 * @file PreviewPopup.h
 * @brief Shows a short preview of a neuron when the mouse stays over it.
 */

#ifndef OGT_PREVIEW_POPUP_H
#define OGT_PREVIEW_POPUP_H

#include "core/Graph.h"

#include <QFrame>

class QLabel;

namespace Ogt
{

class PreviewPopup final : public QFrame
{
    Q_OBJECT

public:
    explicit PreviewPopup(QWidget* parent);

    /// Fills and resizes the popup for @p neuron.
    void SetNeuron(const Neuron& neuron, const Graph& graph, const QString& projectFile);
    /// Hides and drops the picture it was holding.
    void Clear();

private:
    QLabel* title_;
    QLabel* meta_;
    QLabel* body_;
    QLabel* image_;
};

} // namespace Ogt

#endif // OGT_PREVIEW_POPUP_H
