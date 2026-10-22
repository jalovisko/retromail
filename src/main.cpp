#include "BuildConfig.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RetroMail"));
    QApplication::setOrganizationName(QStringLiteral("RetroMail"));
    QApplication::setApplicationVersion(QStringLiteral(RETROMAIL_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("retromail"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("A 1998-style e-mail client"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption guest(QStringLiteral("guest"), QStringLiteral("Open the demonstration mailbox right away."));
    parser.addOption(guest);
    QCommandLineOption tour(QStringLiteral("screenshot-tour"), QStringLiteral("Save screenshots of every screen to DIR, then quit."), QStringLiteral("dir"));
    tour.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(tour);
    parser.process(app);

    Theme::apply(app);

    MainWindow window;
    window.show();
    if (parser.isSet(tour))
        window.runScreenshotTour(parser.value(tour));
    else if (parser.isSet(guest))
        window.signOnAsGuest();

    return app.exec();
}
