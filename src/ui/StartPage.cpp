/**
 * @file StartPage.cpp
 * @brief A short column centred in the window.
 */

#include "ui/StartPage.h"

#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace Ogt
{

StartPage::StartPage(QWidget* parent)
    : QWidget(parent)
{
    auto* column = new QWidget(this);
    column->setMaximumWidth(480);
    auto* layout = new QVBoxLayout(column);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* brand = new QHBoxLayout();
    brand->setSpacing(12);
    auto* mark = new QLabel(column);
    mark->setPixmap(MakeAppIcon().pixmap(32, 32));
    brand->addWidget(mark);
    auto* name = new QLabel("NodIris", column);
    name->setProperty("role", "heading");
    brand->addWidget(name);
    brand->addStretch();
    layout->addLayout(brand);
    layout->addSpacing(8);
    auto* pitch = new QLabel(tr("Lay out projects, roadmaps and research as a map of notes, tasks and files."),
                             column);
    pitch->setProperty("role", "secondary");
    pitch->setWordWrap(true);
    layout->addWidget(pitch);
    layout->addSpacing(32);

    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(8);
    create_ = new QPushButton(tr("New project"), column);
    create_->setProperty("primary", true);
    auto* open = new QPushButton(tr("Open"), column);
    buttons->addWidget(create_);
    buttons->addWidget(open);
    buttons->addStretch();
    layout->addLayout(buttons);
    layout->addSpacing(48);

    auto* recentLabel = new QLabel(tr("Recent"), column);
    recentLabel->setProperty("role", "section");
    layout->addWidget(recentLabel);
    layout->addSpacing(8);
    noRecent_ = new QLabel(tr("Projects you open will be listed here."), column);
    noRecent_->setProperty("role", "muted");
    layout->addWidget(noRecent_);
    recent_ = new QListWidget(column);
    recent_->setSelectionMode(QAbstractItemView::NoSelection);
    recent_->setCursor(Qt::PointingHandCursor);
    recent_->setSpacing(0);
    layout->addWidget(recent_);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(32, 32, 32, 32);
    outer->addStretch(1);
    outer->addWidget(column, 0, Qt::AlignCenter);
    outer->addStretch(1);

    connect(create_, &QPushButton::clicked, this, &StartPage::CreateRequested);
    connect(open, &QPushButton::clicked, this, &StartPage::OpenRequested);
    connect(&Theme::Get(), &Theme::Changed, this, qOverload<>(&QWidget::update));
    connect(recent_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item)
    {
        emit RecentChosen(item->data(Qt::UserRole).toString());
    });
}

void StartPage::paintEvent(QPaintEvent*)
{
    // The same dotted paper as the canvas, so opening a project feels like zooming into this page.
    const ThemeTokens& theme = Theme::Get().Tokens();
    QPainter painter(this);
    painter.fillRect(rect(), theme.canvas);
    painter.setPen(QPen(theme.gridDot, 1.5, Qt::SolidLine, Qt::RoundCap));
    for (int x = 12; x < width(); x += 24)
    {
        for (int y = 12; y < height(); y += 24)
            painter.drawPoint(x, y);
    }
}

void StartPage::SetRecent(const QStringList& paths)
{
    recent_->clear();
    for (const QString& path : paths)
    {
        const QFileInfo info(path);
        auto* item = new QListWidgetItem(QString("%1\n%2").arg(info.completeBaseName(),
                                                               QDir::toNativeSeparators(info.absolutePath())));
        item->setData(Qt::UserRole, path);
        item->setToolTip(QDir::toNativeSeparators(path));
        recent_->addItem(item);
    }
    noRecent_->setVisible(paths.isEmpty());
    recent_->setVisible(!paths.isEmpty());
    // As tall as its rows, so the page does not grow an empty band under them.
    const int rows = int(paths.size());
    recent_->setFixedHeight(rows > 0 ? rows * recent_->sizeHintForRow(0) + 4 : 0);
}

} // namespace Ogt
