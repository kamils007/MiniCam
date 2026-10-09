#include "MainWindow.h"
#include "OccView.h"

#include "ModelImport.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

#include <exception>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("MiniCAM");

    m_view = new OccView(this);
    setCentralWidget(m_view);

    QMenu* fileMenu = menuBar()->addMenu("&Plik");
    QAction* openAct = fileMenu->addAction("&Otwórz model…", this, &MainWindow::onOpen);
    openAct->setShortcut(QKeySequence::Open);
    fileMenu->addSeparator();
    QAction* quitAct = fileMenu->addAction("&Zakończ", this, &QWidget::close);
    quitAct->setShortcut(QKeySequence::Quit);

    QMenu* viewMenu = menuBar()->addMenu("&Widok");
    QAction* fitAct = viewMenu->addAction("&Dopasuj do okna", m_view, &OccView::fitAll);
    fitAct->setShortcut(Qt::Key_F);

    statusBar()->showMessage("Otwórz bryłę: Plik → Otwórz (Ctrl+O)");
}

void MainWindow::onOpen()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Otwórz model", QString(),
        "Modele 3D (*.step *.stp *.iges *.igs *.brep *.brp);;"
        "STEP (*.step *.stp);;IGES (*.iges *.igs);;BREP (*.brep *.brp)");
    if (!path.isEmpty())
        openFile(path);
}

void MainWindow::openFile(const QString& path)
{
    statusBar()->showMessage("Wczytywanie " + QFileInfo(path).fileName() + "…");
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QElapsedTimer timer;
    timer.start();

    try {
        // Rdzeń dostaje ścieżkę w UTF-8 – działa też z polskimi znakami.
        const TopoDS_Shape shape = camcore::importModel(path.toUtf8().toStdString());
        m_view->showShape(shape);
        QApplication::restoreOverrideCursor();

        setWindowTitle("MiniCAM – " + QFileInfo(path).fileName());
        statusBar()->showMessage(QString("Wczytano %1 w %2 ms")
                                     .arg(QFileInfo(path).fileName())
                                     .arg(timer.elapsed()));
    } catch (const std::exception& ex) {
        QApplication::restoreOverrideCursor();
        statusBar()->clearMessage();
        QMessageBox::critical(this, "Błąd wczytywania", QString::fromUtf8(ex.what()));
    }
}
