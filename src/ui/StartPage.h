/**
 * @file StartPage.h
 * @brief What shows before a project is open: new, open, and the recent ones.
 */

#ifndef OGT_START_PAGE_H
#define OGT_START_PAGE_H

#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;

namespace Ogt
{

class StartPage final : public QWidget
{
    Q_OBJECT

public:
    explicit StartPage(QWidget* parent = nullptr);

    void SetRecent(const QStringList& paths);

signals:
    void CreateRequested();
    void OpenRequested();
    void RecentChosen(const QString& path);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPushButton* create_;
    QListWidget* recent_;
    QLabel* noRecent_;
};

} // namespace Ogt

#endif // OGT_START_PAGE_H
