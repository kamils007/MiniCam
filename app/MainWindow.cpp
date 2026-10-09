#include "MainWindow.h"
#include "AlignSettingsDialog.h"
#include "OccView.h"
#include "Ribbon.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QLabel>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>
#include <QStyle>

#include <exception>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("MiniCAM");

    m_view = new OccView(this);
    setCentralWidget(m_view);
    m_alignSettings = loadAlignSettings();

    createDock();
    createRibbon();
    statusBar()->showMessage("Otwórz bryłę: Plik → Otwórz (Ctrl+O)");
}

void MainWindow::createDock()
{
    // Dokowane okno po lewej – miejsce na późniejsze dodatki (np. lista operacji).
    // Można je przeciągnąć na prawą stronę, odczepić jako osobne okno albo zamknąć.
    m_dock = new QDockWidget("Dodatki", this);
    m_dock->setObjectName("dodatkiDock"); // potrzebne do zapamiętania układu okien
    m_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    auto* placeholder = new QLabel("Tu pojawią się dodatki.", m_dock);
    placeholder->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    placeholder->setMargin(12);
    m_dock->setWidget(placeholder);
    m_dock->setMinimumWidth(220);
    // Tło trochę jaśniejsze niż pas ikon na wstążce, kolory niezależne od motywu Windows.
    m_dock->setStyleSheet(R"(
        QDockWidget { color: black; }
        QDockWidget::title { background: #d4d4d4; padding: 4px; }
        QDockWidget > QWidget { background: #efefef; color: black; }
    )");
    addDockWidget(Qt::LeftDockWidgetArea, m_dock);

    // Odczepione okno na Windows ma systemowy pasek tytułu – dwuklik w niego
    // maksymalizowałby okno. Przechwytujemy go i przyczepiamy okno z powrotem.
    m_dock->installEventFilter(this);
}

void MainWindow::dockToHome()
{
    m_dock->setFloating(false);
    addDockWidget(Qt::LeftDockWidgetArea, m_dock); // "baza" – lewa strona okna
    m_dock->show();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_dock && m_dock->isFloating()
        && (event->type() == QEvent::NonClientAreaMouseButtonDblClick
            || event->type() == QEvent::MouseButtonDblClick)) {
        dockToHome();
        return true; // zdarzenie obsłużone – system go już nie dostanie
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::createRibbon()
{
    // Akcje (QAction) to "polecenia" programu. Ten sam obiekt może siedzieć
    // w menu Plik i na wstążce, a skrót klawiszowy działa w całym oknie.
    QAction* openAct = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton),
                                   "Otwórz\nmodel", this);
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpen);

    QAction* quitAct = new QAction("Zakończ", this);
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);

    QAction* fitAct = new QAction(style()->standardIcon(QStyle::SP_TitleBarMaxButton),
                                  "Dopasuj\ndo okna", this);
    fitAct->setShortcut(Qt::Key_F);
    connect(fitAct, &QAction::triggered, m_view, &OccView::fitAll);

    QAction* autoAlignAct = new QAction(style()->standardIcon(QStyle::SP_ArrowDown),
                                        "Auto-Wyrównanie\nCzęści", this);
    autoAlignAct->setToolTip("Wyrównaj bryłę według ustawień z okna Konfiguracja;\n"
                             "każde kolejne kliknięcie odwraca ją na drugą stronę");
    connect(autoAlignAct, &QAction::triggered, this, &MainWindow::onAutoAlign);

    QAction* settingsAct = new QAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView),
                                       "Rozpoznawanie\ncech modelu…", this);
    connect(settingsAct, &QAction::triggered, this, &MainWindow::onAlignSettings);

    // Skróty muszą być zarejestrowane w oknie, inaczej działałyby tylko
    // przy otwartym menu.
    addActions({openAct, quitAct, fitAct});

    auto* ribbon = new Ribbon(this);
    QAction* openMenuAct = ribbon->fileMenu()->addAction("Otwórz model…");
    openMenuAct->setShortcut(QKeySequence::Open);
    openMenuAct->setShortcutContext(Qt::WidgetShortcut); // skrót już obsługuje openAct
    connect(openMenuAct, &QAction::triggered, this, &MainWindow::onOpen);
    ribbon->fileMenu()->addSeparator();
    ribbon->fileMenu()->addAction(quitAct);

    // Akcja "pokaż/ukryj okno" gotowa od Qt – zaznaczona, gdy okno jest widoczne.
    QAction* dockAct = m_dock->toggleViewAction();
    dockAct->setText("Okno\nDodatki");
    dockAct->setIcon(style()->standardIcon(QStyle::SP_FileDialogListView));

    RibbonPage* home = ribbon->addPage("Narzędzia główne");
    home->addGroup("Plik")->addAction(openAct);
    RibbonGroup* viewGroup = home->addGroup("Widok");
    viewGroup->addAction(fitAct);
    viewGroup->addAction(dockAct);

    ribbon->addPage("Ekstrakcja modelu bryłowego"); // na razie pusta

    RibbonPage* solids = ribbon->addPage("Bryły - Użytkowe");
    solids->addGroup("Wyrównanie")->addAction(autoAlignAct);
    solids->addGroup("Ustawienia")->addAction(settingsAct);

    // Wstążka zajmuje miejsce zwykłego paska menu.
    setMenuWidget(ribbon);
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
        m_original = camcore::importModel(path.toUtf8().toStdString());
        m_fileName = QFileInfo(path).fileName();

        m_flipped = false;
        m_aligned = m_alignSettings.alignAfterImport;
        QString how = "położenie z pliku";
        if (m_aligned) {
            showAligned();
            how = "wyrównano automatycznie";
        } else {
            m_view->showModel(m_original);
        }
        QApplication::restoreOverrideCursor();

        setWindowTitle("MiniCAM – " + m_fileName);
        statusBar()->showMessage(QString("Wczytano %1 w %2 ms (%3)")
                                     .arg(m_fileName)
                                     .arg(timer.elapsed())
                                     .arg(how));
    } catch (const std::exception& ex) {
        QApplication::restoreOverrideCursor();
        statusBar()->clearMessage();
        QMessageBox::critical(this, "Błąd wczytywania", QString::fromUtf8(ex.what()));
    }
}

void MainWindow::showAligned()
{
    // Liczymy zawsze od oryginału z pliku – wynik zależy tylko od ustawień i m_flipped.
    const gp_Trsf trsf = camcore::computeAlignment(m_original.shape, m_alignSettings, m_flipped);
    m_view->showModel(camcore::transformed(m_original, trsf));
}

void MainWindow::onAutoAlign()
{
    if (m_original.shape.IsNull()) {
        statusBar()->showMessage("Najpierw otwórz model (Ctrl+O)");
        return;
    }
    if (!m_aligned) {
        // Pierwsze użycie (gdy "Wyrównaj po imporcie" jest wyłączone): zwykłe wyrównanie.
        m_aligned = true;
        statusBar()->showMessage("Wyrównano " + m_fileName);
    } else {
        // Kolejne kliknięcia: przekładamy bryłę na drugą stronę i z powrotem.
        m_flipped = !m_flipped;
        statusBar()->showMessage(m_flipped ? "Odwrócono na drugą stronę"
                                           : "Przywrócono stronę wybraną automatycznie");
    }
    showAligned();
}

void MainWindow::onAlignSettings()
{
    AlignSettingsDialog dialog(m_alignSettings, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_alignSettings = dialog.settings();
    saveAlignSettings(m_alignSettings);
    // Nowe ustawienia od razu widać na wyrównanej bryle.
    if (m_aligned && !m_original.shape.IsNull())
        showAligned();
}
