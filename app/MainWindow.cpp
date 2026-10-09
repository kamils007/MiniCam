#include "MainWindow.h"
#include "OccView.h"

#include "ModelAlign.h"
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
        camcore::ImportedModel model = camcore::importModel(path.toUtf8().toStdString());
        // Ustawiamy bryłę w zerze: lewy przedni dolny róg w 0,0,0.
        const gp_Vec shift = camcore::alignModel(model, camcore::AlignMode::BottomCorner);
        m_view->showModel(model);
        QApplication::restoreOverrideCursor();

        setWindowTitle("MiniCAM – " + QFileInfo(path).fileName());
        statusBar()->showMessage(QString("Wczytano %1 w %2 ms, przesunięto o X %3  Y %4  Z %5 mm")
                                     .arg(QFileInfo(path).fileName())
                                     .arg(timer.elapsed())
                                     .arg(shift.X(), 0, 'f', 3)
                                     .arg(shift.Y(), 0, 'f', 3)
                                     .arg(shift.Z(), 0, 'f', 3));
    } catch (const std::exception& ex) {
        QApplication::restoreOverrideCursor();
        statusBar()->clearMessage();
        QMessageBox::critical(this, "Błąd wczytywania", QString::fromUtf8(ex.what()));
    }
}
