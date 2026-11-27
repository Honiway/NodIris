/**
 * @file main.cpp
 * @brief Starts the application, picks the language and the theme, opens a project if given one.
 */

#include "ui/Icons.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("NodIris");
    QApplication::setOrganizationName("NodIris");
    QApplication::setApplicationVersion(OGT_VERSION);
    QApplication::setWindowIcon(Ogt::MakeAppIcon());

    // Our strings, then Qt's own (file dialogs, message box buttons), in the system language.
    QTranslator ours;
    if (ours.load(QLocale(), "nodiris", "_", ":/i18n"))
        QApplication::installTranslator(&ours);
    QTranslator qt;
    if (qt.load(QLocale(), "qtbase", "_", QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qt);

    const auto mode = Ogt::ThemeMode(QSettings().value("theme", int(Ogt::ThemeMode::System)).toInt());
    Ogt::Theme::Get().Apply(app, mode);

    Ogt::MainWindow window;
    window.show();
    const QStringList arguments = QApplication::arguments();
    if (arguments.size() > 1)
        window.OpenProject(arguments.at(1));
    return QApplication::exec();
}
