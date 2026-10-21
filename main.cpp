#include "window.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QSystemTrayIcon>
#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    for (int i=1; i<argc; ++i) {
        if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-v") == 0) {
            std::puts("desktop-ruler 0.2.0"); return 0;
        }
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::puts("desktop-ruler 0.2.0\nUsage: desktop-ruler [--image FILE] [--tray] [--help] [--version]\n"
                      "Measure a frozen Wayland screen capture or local image. Qt6 Widgets and grim required for capture.");
            return 0;
        }
    }
    // Keep file dialogs parented, accessible and consistent across Qt platform themes.
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("desktop-ruler"));
    app.setDesktopFileName(QStringLiteral("desktop-ruler"));
    app.setApplicationVersion(QStringLiteral("0.2.0"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Desktop Ruler: measure a frozen screen capture or local image"));
    parser.addHelpOption(); parser.addVersionOption();
    QCommandLineOption imageOption({QStringLiteral("i"),QStringLiteral("image")},
                                   QStringLiteral("Open an image file"),QStringLiteral("file"));
    QCommandLineOption trayOption(QStringLiteral("tray"), QStringLiteral("Start in the system tray when available"));
    parser.addOption(imageOption); parser.addOption(trayOption); parser.process(app);
    RulerWindow window;
    if (!parser.isSet(trayOption) || !QSystemTrayIcon::isSystemTrayAvailable()) window.show();
    if (parser.isSet(imageOption))
        QTimer::singleShot(0, &window, [&window,&parser,imageOption] { window.openImageFile(parser.value(imageOption)); });
    return app.exec();
}
