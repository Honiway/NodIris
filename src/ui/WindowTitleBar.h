/**
 * @file WindowTitleBar.h
 * @brief Provides window controls with clear hover states.
 */

#ifndef OGT_WINDOW_TITLE_BAR_H
#define OGT_WINDOW_TITLE_BAR_H

#include <QFrame>

class QLabel;
class QMouseEvent;
class QToolButton;

namespace Ogt
{

class WindowTitleBar final : public QFrame
{
    Q_OBJECT

public:
    explicit WindowTitleBar(QWidget* window);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void ToggleMaximized();
    void UpdateMaximizeButton();

    QWidget* window_;
    QLabel* title_;
    QToolButton* maximize_;
    bool dragging_ = false;
    QPoint dragOffset_;
};

} // namespace Ogt

#endif // OGT_WINDOW_TITLE_BAR_H
