/**
 * @file WindowTitleBar.cpp
 * @brief Draws and handles the application title bar.
 */

#include "ui/WindowTitleBar.h"

#include "ui/Icons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QToolButton>
#include <QWindow>

namespace Ogt
{

namespace
{

QToolButton* WindowButton(const QString& text, const QString& tip, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setText(text);
    button->setToolTip(tip);
    button->setProperty("windowButton", true);
    button->setCursor(Qt::ArrowCursor);
    return button;
}

} // namespace

WindowTitleBar::WindowTitleBar(QWidget* window)
    : QFrame(window)
    , window_(window)
{
    setProperty("role", "titleBar");
    setFixedHeight(35);
    window_->installEventFilter(this);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 0, 0);
    layout->setSpacing(0);

    auto* logo = new QLabel(this);
    logo->setPixmap(MakeAppIcon().pixmap(18, 18));
    logo->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(logo);
    layout->addSpacing(8);

    title_ = new QLabel(window->windowTitle(), this);
    title_->setAttribute(Qt::WA_TransparentForMouseEvents);
    title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(title_, 1);

    auto* minimize = WindowButton(QString::fromUtf8("\xE2\x88\x92"), tr("Minimize"), this);
    maximize_ = WindowButton(QString::fromUtf8("\xE2\x96\xA1"), tr("Maximize"), this);
    auto* close = WindowButton(QString::fromUtf8("\xC3\x97"), tr("Close"), this);
    close->setProperty("closeButton", true);
    layout->addWidget(minimize);
    layout->addWidget(maximize_);
    layout->addWidget(close);

    connect(window, &QWidget::windowTitleChanged, title_, &QLabel::setText);
    connect(minimize, &QToolButton::clicked, window, &QWidget::showMinimized);
    connect(maximize_, &QToolButton::clicked, this, &WindowTitleBar::ToggleMaximized);
    connect(close, &QToolButton::clicked, window, &QWidget::close);
}

bool WindowTitleBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == window_ && event->type() == QEvent::WindowStateChange)
        UpdateMaximizeButton();
    return QFrame::eventFilter(watched, event);
}

void WindowTitleBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    dragging_ = true;
    dragOffset_ = event->globalPosition().toPoint() - window_->frameGeometry().topLeft();
    if (window_->windowHandle() && window_->windowHandle()->startSystemMove())
        dragging_ = false;
    event->accept();
}

void WindowTitleBar::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_ && event->buttons().testFlag(Qt::LeftButton))
        window_->move(event->globalPosition().toPoint() - dragOffset_);
}

void WindowTitleBar::mouseReleaseEvent(QMouseEvent*)
{
    dragging_ = false;
}

void WindowTitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        ToggleMaximized();
}

void WindowTitleBar::ToggleMaximized()
{
    window_->isMaximized() ? window_->showNormal() : window_->showMaximized();
    UpdateMaximizeButton();
}

void WindowTitleBar::UpdateMaximizeButton()
{
    maximize_->setText(window_->isMaximized() ? QString::fromUtf8("\xE2\x9D\x90")
                                               : QString::fromUtf8("\xE2\x96\xA1"));
    maximize_->setToolTip(window_->isMaximized() ? tr("Restore") : tr("Maximize"));
}

} // namespace Ogt
