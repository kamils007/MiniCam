#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
#if defined(__linux__)
    // OCCT rysuje przez X11; pod Waylandem Qt musi użyć XWayland.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "xcb");
#endif

    QApplication app(argc, argv);

    MainWindow window;
    window.resize(1280, 800);
    window.show();

    // Można też podać plik w linii poleceń: minicam detal.step
    const QStringList args = QApplication::arguments();
    if (args.size() > 1)
        window.openFile(args.at(1));

    return app.exec();
}
