#include "MainWindow.h"
#include "OccView.h"

#include "FaceInfo.h"
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
    connect(m_view, &OccView::selectionChanged, this, &MainWindow::onSelectionChanged);

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
        m_fileName = QFileInfo(path).fileName();
        m_view->showShape(shape);
        QApplication::restoreOverrideCursor();

        setWindowTitle("MiniCAM – " + QFileInfo(path).fileName());
        statusBar()->showMessage(QString("Wczytano %1 w %2 ms – kliknij ścianę, aby ją wybrać")
                                     .arg(m_fileName)
                                     .arg(timer.elapsed()));
    } catch (const std::exception& ex) {
        QApplication::restoreOverrideCursor();
        statusBar()->clearMessage();
        QMessageBox::critical(this, "Błąd wczytywania", QString::fromUtf8(ex.what()));
    }
}

void MainWindow::onSelectionChanged()
{
    const std::vector<TopoDS_Face> faces = m_view->selectedFaces();

    if (faces.empty()) {
        if (!m_fileName.isEmpty())
            statusBar()->showMessage("Nic nie wybrano – kliknij ścianę (Ctrl+klik dodaje kolejne)");
        return;
    }

    if (faces.size() == 1) {
        // Jedna ściana – pokazujemy szczegóły.
        const camcore::FaceInfo info = camcore::describeFace(faces.front());
        QString msg = QString("Ściana %1, pole %2 mm²")
                          .arg(QString::fromStdString(info.surfaceType))
                          .arg(info.area, 0, 'f', 2);
        if (info.isPlanar) {
            msg += QString(", normalna (%1; %2; %3)")
                       .arg(info.normal[0], 0, 'f', 3)
                       .arg(info.normal[1], 0, 'f', 3)
                       .arg(info.normal[2], 0, 'f', 3);
            if (info.isHorizontal)
                msg += QString(", pozioma na Z = %1").arg(info.z, 0, 'f', 3);
        }
        statusBar()->showMessage(msg);
        return;
    }

    // Kilka ścian – podsumowanie.
    double totalArea = 0.0;
    for (const TopoDS_Face& f : faces)
        totalArea += camcore::describeFace(f).area;
    statusBar()->showMessage(QString("Wybrano %1 ścian(y), łączne pole %2 mm²")
                                 .arg(faces.size())
                                 .arg(totalArea, 0, 'f', 2));
}
