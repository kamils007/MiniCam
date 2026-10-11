#include "MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char* argv[])
{
#if defined(__linux__)
    // OCCT rysuje przez X11; pod Waylandem Qt musi użyć XWayland.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "xcb");
#endif

    QApplication app(argc, argv);

    // Ikona okna i paska zadań (kostka z logo), w kilku rozmiarach.
    QIcon appIcon;
    for (int size : {16, 24, 32, 48, 64, 128, 256})
        appIcon.addFile(QString(":/ikona/minicam_%1.png").arg(size), QSize(size, size));
    app.setWindowIcon(appIcon);

    MainWindow window;
    window.resize(1280, 800);
    window.show();

    // Można też podać plik w linii poleceń: minicam detal.step
    const QStringList args = QApplication::arguments();
    if (args.size() > 1)
        window.openFile(args.at(1));

    return app.exec();
}
