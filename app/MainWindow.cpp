#include "MainWindow.h"
#include "AlignSettingsDialog.h"
#include "BottomBars.h"
#include "InputBar.h"
#include "LayersPanel.h"
#include "OccView.h"
#include "Ribbon.h"
#include "commands/MoveCommand.h"

#include <QAction>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QKeyEvent>
#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QUndoCommand>
#include <QUndoStack>

#include <algorithm>
#include <cmath>
#include <exception>

namespace {

// Ikona "Przesuń": strzałki w cztery strony.
QIcon moveIcon()
{
    QPixmap pix(32, 32);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(200, 40, 30), 2.5));
    p.drawLine(16, 4, 16, 28);
    p.drawLine(4, 16, 28, 16);
    p.setBrush(QColor(200, 40, 30));
    const QPointF up[] = {{16, 1}, {11, 8}, {21, 8}};
    const QPointF down[] = {{16, 31}, {11, 24}, {21, 24}};
    const QPointF left[] = {{1, 16}, {8, 11}, {8, 21}};
    const QPointF right[] = {{31, 16}, {24, 11}, {24, 21}};
    p.drawPolygon(up, 3);
    p.drawPolygon(down, 3);
    p.drawPolygon(left, 3);
    p.drawPolygon(right, 3);
    return QIcon(pix);
}

// Jedna zmiana na liście cofania: stan przed i po. Qt wywołuje redo() od razu przy
// dodaniu na stos – wtedy zmiana jest już zrobiona, więc pierwszy raz nic nie robimy.
class EditCommand : public QUndoCommand
{
public:
    EditCommand(MainWindow* window, const QString& text, EditState before, EditState after)
        : QUndoCommand(text), m_window(window), m_before(std::move(before)), m_after(std::move(after))
    {
    }
    void undo() override { m_window->restoreEditState(m_before); }
    void redo() override
    {
        if (m_first) {
            m_first = false;
            return;
        }
        m_window->restoreEditState(m_after);
    }

private:
    MainWindow* m_window;
    EditState m_before, m_after;
    bool m_first = true;
};

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("MiniCAM");

    m_undo = new QUndoStack(this);
    m_view = new OccView(this);
    setCentralWidget(m_view);
    m_alignSettings = loadAlignSettings();

    createDock();
    createBottomBars();
    createRibbon();
    // Zdarzenia z widoku trafiają do trwającego polecenia.
    connect(m_view, &OccView::selectionConfirmed, this, [this] {
        if (m_command)
            m_command->selectionConfirmed();
    });
    connect(m_view, &OccView::pointPicked, this, &MainWindow::onPointPicked);
    connect(m_view, &OccView::cancelRequested, this, &MainWindow::cancelCommand);
    connect(m_view, &OccView::selectionChanged, this, [this](int count) {
        if (m_command)
            m_command->selectionChanged(count);
    });
    // Klawiatura w widoku 3D trafia do paska wprowadzania (pisanie liczb, Enter, Tab).
    m_view->installEventFilter(this);
    statusBar()->showMessage("Otwórz bryłę: Plik → Otwórz (Ctrl+O)");
}

void MainWindow::createDock()
{
    // Dokowane okno po lewej – miejsce na późniejsze dodatki (np. lista operacji).
    // Można je przeciągnąć na prawą stronę, odczepić jako osobne okno albo zamknąć.
    m_dock = new QDockWidget("Warstwy", this);
    m_dock->setObjectName("dodatkiDock"); // potrzebne do zapamiętania układu okien
    m_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    // Panel warstw jak w Alphacam; checkboxy sterują widocznością w widoku 3D.
    m_layers = new LayersPanel(m_dock);
    connect(m_layers, &LayersPanel::geometryVisibilityChanged, this, &MainWindow::setGeometryVisible);
    connect(m_layers, &LayersPanel::modelVisibilityChanged, m_view, &OccView::setModelVisible);
    connect(m_layers, &LayersPanel::geometriesSelected, m_view, &OccView::highlightGeometry);
    m_dock->setWidget(m_layers);
    m_dock->setMinimumWidth(220);
    // Tło trochę jaśniejsze niż pas ikon na wstążce, kolory niezależne od motywu Windows.
    m_dock->setStyleSheet(R"(
        QDockWidget { color: black; }
        QDockWidget::title { background: #d4d4d4; padding: 4px; }
        QDockWidget > QWidget { background: #efefef; color: black; border: none; }
        QTreeWidget { background: #efefef; color: black; border: none; }
        QTreeWidget::item:selected { background: #cfe3f7; color: black; }
        QToolBar { background: #e6e6e6; border: none; spacing: 2px; }
        QToolBar#layersSide { border-right: 1px solid #c8c8c8; }
        QToolBar#layersBar { border-bottom: 1px solid #c8c8c8; }
        QToolButton:checked { background: #cfe3f7; border: 1px solid #7aa7d6; }
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
    if (watched == m_view && event->type() == QEvent::KeyPress) {
        auto* e = static_cast<QKeyEvent*>(event);
        if (m_inputBar->handleViewKey(e))
            return true;
        // Enter w trakcie wyboru elementów działa jak PPM.
        if (m_command && m_command->isSelecting() && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)) {
            m_command->selectionConfirmed();
            return true;
        }
    }
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

    QAction* recognizeAct = new QAction(style()->standardIcon(QStyle::SP_FileDialogContentsView),
                                        "Rozpoznaj\ncechy", this);
    recognizeAct->setToolTip("Narysuj kontury 2D z bryły: od dołu, na każdej\npoziomej powierzchni");
    connect(recognizeAct, &QAction::triggered, this, &MainWindow::onRecognizeFeatures);

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
    dockAct->setText("Okno\nWarstwy");
    dockAct->setIcon(style()->standardIcon(QStyle::SP_FileDialogListView));

    RibbonPage* home = ribbon->addPage("Narzędzia główne");
    home->addGroup("Plik")->addAction(openAct);
    RibbonGroup* viewGroup = home->addGroup("Widok");
    viewGroup->addAction(fitAct);
    viewGroup->addAction(dockAct);

    // Zakładka Edycja – układ grup jak w Alphacam. Na razie działa "Przesuń",
    // pozostałe polecenia są widoczne, ale wyłączone (przyjdą później).
    auto soon = [this](QStyle::StandardPixmap icon, const QString& text) {
        auto* a = new QAction(style()->standardIcon(icon), text, this);
        a->setEnabled(false);
        a->setToolTip(QString(text).replace('\n', ' ') + " (wkrótce)");
        return a;
    };
    QAction* moveAct = new QAction(moveIcon(), "Przesuń", this);
    moveAct->setToolTip("Przesuń bryłę lub geometrie: wybierz elementy (LPM),\n"
                        "zatwierdź PPM, potem kliknij punkt bazowy i docelowy\n"
                        "albo wpisz przesunięcie dX/dY/dZ");
    connect(moveAct, &QAction::triggered, this, [this] { runCommand(new MoveCommand(*this, this)); });

    RibbonPage* edit = ribbon->addPage("Edycja");
    RibbonGroup* editGroup = edit->addGroup("Edycja");
    QAction* undoAct = new QAction(style()->standardIcon(QStyle::SP_ArrowBack), "Cofnij", this);
    undoAct->setShortcut(QKeySequence::Undo);
    undoAct->setToolTip("Cofnij ostatnią zmianę (Ctrl+Z)");
    undoAct->setEnabled(false);
    connect(undoAct, &QAction::triggered, this, [this] {
        cancelCommand();
        const QString what = m_undo->undoText();
        m_undo->undo();
        statusBar()->showMessage("Cofnięto: " + what);
    });
    connect(m_undo, &QUndoStack::canUndoChanged, undoAct, &QAction::setEnabled);
    QAction* redoAct = new QAction(style()->standardIcon(QStyle::SP_ArrowForward), "Ponów", this);
    redoAct->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)});
    redoAct->setToolTip("Ponów cofniętą zmianę (Ctrl+Y)");
    redoAct->setEnabled(false);
    connect(redoAct, &QAction::triggered, this, [this] {
        cancelCommand();
        const QString what = m_undo->redoText();
        m_undo->redo();
        statusBar()->showMessage("Ponowiono: " + what);
    });
    connect(m_undo, &QUndoStack::canRedoChanged, redoAct, &QAction::setEnabled);
    addActions({undoAct, redoAct}); // skróty działają w całym oknie
    editGroup->addAction(undoAct);
    editGroup->addAction(redoAct);
    editGroup->addAction(soon(QStyle::SP_DialogDiscardButton, "Usuń"));
    RibbonGroup* clipboard = edit->addGroup("Schowek");
    clipboard->addAction(soon(QStyle::SP_FileIcon, "Kopiuj"));
    clipboard->addAction(soon(QStyle::SP_DialogResetButton, "Wytnij"));
    clipboard->addAction(soon(QStyle::SP_DialogSaveButton, "Wklej"));
    RibbonGroup* moveGroup = edit->addGroup("Przesuń, kopiuj itd.");
    moveGroup->addAction(moveAct);
    moveGroup->addAction(soon(QStyle::SP_BrowserReload, "Obróć"));
    moveGroup->addAction(soon(QStyle::SP_MediaSeekBackward, "Lustro"));
    moveGroup->addAction(soon(QStyle::SP_TitleBarMaxButton, "Skaluj"));
    RibbonGroup* breakGroup = edit->addGroup("Przerwij, połącz itd.");
    breakGroup->addAction(soon(QStyle::SP_MediaPause, "Przerwij"));
    breakGroup->addAction(soon(QStyle::SP_DialogCancelButton, "Przytnij"));
    breakGroup->addAction(soon(QStyle::SP_DialogApplyButton, "Połącz"));

    RibbonPage* extraction = ribbon->addPage("Ekstrakcja modelu bryłowego");
    extraction->addGroup("Cechy")->addAction(recognizeAct);

    RibbonPage* solids = ribbon->addPage("Bryły - Użytkowe");
    solids->addGroup("Wyrównanie")->addAction(autoAlignAct);
    solids->addGroup("Ustawienia")->addAction(settingsAct);

    // Spacja powtarza ostatnie polecenie (jak w Alphacam) – działa tak, jakby kliknąć
    // jego przycisk na wstążce. Zapamiętujemy polecenia, które coś robią z modelem;
    // widok, okna, plik i Cofnij/Ponów się nie liczą.
    for (QAction* a : {moveAct, recognizeAct, autoAlignAct, settingsAct}) {
        connect(a, &QAction::triggered, this, [this, a] {
            m_lastCommand = a;
            m_repeatAct->setToolTip("Powtórz: " + a->text().replace('\n', ' ') + " (spacja)");
        });
    }
    // Skrót okna: pole tekstowe z fokusem zabiera spację dla siebie, więc pisanie
    // w polach paska wprowadzania nie powtarza polecenia.
    m_repeatAct = new QAction("Powtórz ostatnie polecenie", this);
    m_repeatAct->setShortcut(Qt::Key_Space);
    connect(m_repeatAct, &QAction::triggered, this, [this] {
        if (!m_lastCommand) {
            statusBar()->showMessage("Spacja powtarza ostatnie polecenie – na razie żadnego nie było");
            return;
        }
        if (m_command || !m_lastCommand->isEnabled())
            return; // w trakcie polecenia spacja nic nie robi
        m_lastCommand->trigger();
    });
    addAction(m_repeatAct);

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
            showModel(m_original);
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
    showModel(camcore::transformed(m_original, trsf));
}

void MainWindow::showModel(const camcore::ImportedModel& model)
{
    m_shown = model;
    m_undo->clear(); // nowa bryła albo nowe położenie – starej historii nie da się już cofnąć
    m_view->showModel(model);
    m_layers->setModelName(m_fileName);
    clearFeatures(); // cechy dotyczyły poprzedniego położenia bryły
}

void MainWindow::clearFeatures()
{
    m_contours = {};
    m_geometries.clear();
    m_layerList.clearUserLayers(); // warstwy z rozpoznawania dotyczyły poprzedniej bryły
    showGeometries();
}

namespace {

QString num(double v) { return QString::number(v, 'f', v == std::floor(v) ? 0 : 1); }

QString geometryText(size_t index, const camcore::Contour& c)
{
    const QString shape = c.diameter > 0 ? "Ø" + num(c.diameter) : num(c.sizeX) + " × " + num(c.sizeY);
    return QString("Geometria %1: %2, wys. %3").arg(index + 1).arg(shape, num(c.zTop - c.zBottom));
}

} // namespace

// Rysuje geometrie według ich właściwości i odświeża warstwę APS w panelu.
void MainWindow::showGeometries()
{
    std::vector<OccView::Contour> contours;
    std::vector<LayersPanel::GeometryRow> rows;
    for (size_t i = 0; i < m_geometries.size(); ++i) {
        const camcore::Geometry& g = m_geometries[i];
        // Kolor z warstwy geometrii.
        contours.push_back({g.contour.wire, m_layerList.layerOrAps(g.layer).color, g.contour.zTop - g.contour.zBottom});
        rows.push_back({geometryText(i, g.contour), QString::fromStdString(g.layer), g.visible});
    }
    // Panel pokazuje wszystkie warstwy użytkownika z listy (także puste –
    // przydadzą się przy ręcznym dodawaniu warstw).
    std::vector<LayersPanel::LayerRow> userLayers;
    for (size_t i = 1; i < m_layerList.all().size(); ++i) {
        const camcore::Layer& layer = m_layerList.all()[i];
        double r, g, b;
        layer.color.Values(r, g, b, Quantity_TOC_sRGB);
        userLayers.push_back({QString::fromStdString(layer.name), QColor::fromRgbF(r, g, b)});
    }
    m_layers->setUserLayers(userLayers);

    m_view->showGeometry(contours);
    for (size_t i = 0; i < m_geometries.size(); ++i)
        if (!m_geometries[i].visible)
            m_view->setGeometryVisible(static_cast<int>(i), false);
    m_layers->setGeometries(rows);
}

void MainWindow::setGeometryVisible(int index, bool visible)
{
    if (index < 0 || index >= static_cast<int>(m_geometries.size()))
        return;
    m_geometries[static_cast<size_t>(index)].visible = visible; // właściwość geometrii
    m_view->setGeometryVisible(index, visible);
}

void MainWindow::onRecognizeFeatures()
{
    if (m_shown.shape.IsNull()) {
        statusBar()->showMessage("Najpierw otwórz model (Ctrl+O)");
        return;
    }
    m_undo->clear(); // geometrie budujemy od nowa – starych zmian nie da się już cofnąć
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_contours = camcore::buildPartContours(m_shown.shape);
    QApplication::restoreOverrideCursor();

    // Kontur zewnętrzny (obrys bryły) idzie do warstwy userKonturZew, pozostałe
    // kontury dostają warstwę według reguł z layerForContour (otwory, kontury
    // wewnętrzne, kieszenie okrągłe); reszta to na razie geometrie niesklasyfikowane – APS.
    m_geometries.clear();
    m_layerList.clearUserLayers();
    // Geometria trafia do warstwy; warstwa użytkownika powstaje przy pierwszej
    // geometrii, która do niej trafia (createLayer zwraca istniejącą, jeśli już jest).
    auto add = [this](const camcore::Contour& c, const std::string& layer) {
        if (layer != camcore::kApsLayer)
            m_layerList.createLayer(layer, camcore::autoLayerColor(layer));
        camcore::Geometry g;
        g.contour = c;
        g.layer = layer;
        m_geometries.push_back(g);
    };
    if (!m_contours.outline.geometry.empty())
        add(m_contours.outline, camcore::kOuterContourLayer);
    for (const camcore::Contour& c : m_contours.inner)
        add(c, camcore::layerForContour(c, true));
    for (const camcore::Pocket& p : m_contours.pockets)
        for (const camcore::Contour& c : p.contours)
            add(c, camcore::layerForContour(c, false));
    showGeometries();
    statusBar()->showMessage(QString("Wyciągnięto %1 geometrii").arg(m_geometries.size()));
}

void MainWindow::createBottomBars()
{
    m_commandBar = new CommandBar(this, this);
    m_inputBar = m_commandBar->inputBar();
    connect(m_inputBar, &InputBar::pointEntered, this, &MainWindow::onPointEntered);
    connect(m_inputBar, &InputBar::selectionDone, this, [this] {
        if (m_command)
            m_command->selectionConfirmed();
    });
    connect(m_inputBar, &InputBar::cancelled, this, &MainWindow::cancelCommand);
    connect(m_commandBar, &CommandBar::snapChanged, m_view, &OccView::setSnap);
    addToolBar(Qt::BottomToolBarArea, m_commandBar);
    setStyleSheet(styleSheet() + bottomBarsStyleSheet());

    // Stopka: komunikaty, współrzędne kursora, widoki i przełączniki.
    m_cursorLabel = new QLabel("X –   Y –", this);
    m_cursorLabel->setObjectName("cursorLabel");
    m_cursorLabel->setMinimumWidth(190);
    m_cursorLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    statusBar()->addPermanentWidget(m_cursorLabel);
    connect(m_view, &OccView::cursorMoved, this, [this](double x, double y) {
        m_cursorLabel->setText(QString("X %1   Y %2").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3));
    });
    setupFooter(statusBar());

    // Esc przerywa polecenie także wtedy, gdy widok 3D nie ma fokusu.
    auto* escAct = new QAction(this);
    escAct->setShortcut(Qt::Key_Escape);
    connect(escAct, &QAction::triggered, this, &MainWindow::cancelCommand);
    addAction(escAct);
}

void MainWindow::runCommand(Command* command)
{
    cancelCommand();
    m_command = command;
    if (!m_command->start()) {
        m_command->deleteLater();
        m_command = nullptr;
    }
}

void MainWindow::onPointPicked(double x, double y, double z)
{
    // Kliknięcie w widoku: wartości wpisane (przypięte) w pasku wprowadzania
    // zastępują odpowiednie współrzędne kursora.
    if (const auto p = m_inputBar->resolveClick(x, y, z))
        onPointEntered(p->X(), p->Y(), p->Z());
    else
        statusBar()->showMessage("Błędna wartość w pasku wprowadzania – popraw pole zaznaczone na czerwono");
}

void MainWindow::onPointEntered(double x, double y, double z)
{
    // Po zatwierdzeniu punktu klawiatura wraca do widoku – kolejne pisanie
    // zaczyna się od pierwszego pola następnego punktu.
    m_view->setFocus();
    if (m_command)
        m_command->pointEntered(gp_Pnt(x, y, z));
}

void MainWindow::cancelCommand()
{
    if (m_command)
        m_command->cancel();
}

void MainWindow::commandFinished(const QString& message)
{
    // Polecenie woła to ze swojej metody – usuwamy je dopiero po powrocie do pętli zdarzeń.
    if (m_command) {
        m_command->deleteLater();
        m_command = nullptr;
    }
    m_inputBar->showIdle();
    m_view->setInteraction(OccView::Interaction::Navigate);
    statusBar()->showMessage(message);
}

void MainWindow::showMessage(const QString& message)
{
    statusBar()->showMessage(message);
}

void MainWindow::releaseSnap()
{
    m_commandBar->releaseSnap();
}

void MainWindow::commitEdit(const QString& text, const EditState& before, const EditState& after)
{
    restoreEditState(after);
    m_undo->push(new EditCommand(this, text, before, after));
}

void MainWindow::restoreEditState(const EditState& state)
{
    const bool modelChanged = !state.shown.shape.IsSame(m_shown.shape);
    m_shown = state.shown;
    m_geometries = state.geometries;
    if (modelChanged)
        m_view->updateModel(m_shown);
    showGeometries();
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
