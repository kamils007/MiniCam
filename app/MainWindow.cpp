#include "MainWindow.h"
#include "AlignSettingsDialog.h"
#include "InputBar.h"
#include "LayersPanel.h"
#include "OccView.h"
#include "Ribbon.h"

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
    EditCommand(MainWindow* window, const QString& text, MainWindow::EditState before, MainWindow::EditState after)
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
    MainWindow::EditState m_before, m_after;
    bool m_first = true;
};

// Ikony uchwytów (jak w Alphacam): szary element i pomarańczowy punkt przyciągania.
QIcon snapIcon(int kind)
{
    QPixmap pix(18, 18);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    const QPen line(QColor(60, 60, 60), 1.6);
    const QColor dot(240, 150, 20);
    auto point = [&](double x, double y) {
        p.setPen(QPen(QColor(120, 60, 0), 0.8));
        p.setBrush(dot);
        p.drawRect(QRectF(x - 2.2, y - 2.2, 4.4, 4.4));
    };
    p.setPen(line);
    p.setBrush(Qt::NoBrush);
    switch (kind) {
    case 0: // auto: element z kilkoma punktami
        p.drawLine(QPointF(3, 15), QPointF(15, 3));
        point(3, 15);
        point(9, 9);
        point(15, 3);
        break;
    case 1: // koniec
        p.drawLine(QPointF(4, 9), QPointF(16, 9));
        point(4, 9);
        break;
    case 2: // środek
        p.drawLine(QPointF(2, 9), QPointF(16, 9));
        point(9, 9);
        break;
    case 3: // centrum okręgu
        p.drawEllipse(QPointF(9, 9), 6.5, 6.5);
        point(9, 9);
        break;
    case 8: // ćwiartki
        p.drawEllipse(QPointF(9, 9), 6, 6);
        point(9, 3);
        point(15, 9);
        point(9, 15);
        point(3, 9);
        break;
    default:
        break;
    }
    return QIcon(pix);
}

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
    connect(m_view, &OccView::selectionConfirmed, this, &MainWindow::onSelectionConfirmed);
    connect(m_view, &OccView::pointPicked, this, &MainWindow::onPointPicked);
    connect(m_view, &OccView::cancelRequested, this, &MainWindow::cancelMove);
    connect(m_view, &OccView::selectionChanged, this, [this](int count) {
        m_inputBar->setPrompt(QString("Wskaż (wybrano %1)").arg(count));
        statusBar()->showMessage(QString("Przesuń: wybrano %1 – klikaj kolejne elementy, PPM zatwierdza, Esc anuluje")
                                     .arg(count));
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
        if (m_moveStep == MoveStep::Selecting && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)) {
            onSelectionConfirmed();
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
    connect(moveAct, &QAction::triggered, this, &MainWindow::onMove);

    RibbonPage* edit = ribbon->addPage("Edycja");
    RibbonGroup* editGroup = edit->addGroup("Edycja");
    QAction* undoAct = new QAction(style()->standardIcon(QStyle::SP_ArrowBack), "Cofnij", this);
    undoAct->setShortcut(QKeySequence::Undo);
    undoAct->setToolTip("Cofnij ostatnią zmianę (Ctrl+Z)");
    undoAct->setEnabled(false);
    connect(undoAct, &QAction::triggered, this, [this] {
        cancelMove();
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
        cancelMove();
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

namespace {
// Pusty przycisk – miejsce na przyszłą funkcję (na razie bez ikony i bez działania).
QToolButton* placeholderButton(QWidget* parent, const QString& tip, int width = 26)
{
    auto* b = new QToolButton(parent);
    b->setFixedSize(width, 24);
    b->setToolTip(tip + " (wkrótce)");
    b->setEnabled(false);
    b->setProperty("placeholder", true);
    return b;
}
} // namespace

void MainWindow::createBottomBars()
{
    // Belka polecenia (druga od dołu, jak w Alphacam): po lewej pasek wprowadzania
    // (podpowiedź i pola bieżącego polecenia), po prawej przyciąganie.
    m_commandBar = new QToolBar("Polecenie", this);
    m_commandBar->setObjectName("commandBar");
    m_commandBar->setMovable(false);
    m_commandBar->setFloatable(false);
    m_commandBar->setContextMenuPolicy(Qt::PreventContextMenu);
    m_commandBar->setMinimumHeight(32); // pusta belka (bez polecenia) zostaje na swoim miejscu
    m_inputBar = new InputBar(m_commandBar);
    m_commandBar->addWidget(m_inputBar);
    connect(m_inputBar, &InputBar::pointEntered, this, &MainWindow::onPointEntered);
    connect(m_inputBar, &InputBar::selectionDone, this, &MainWindow::onSelectionConfirmed);
    connect(m_inputBar, &InputBar::cancelled, this, &MainWindow::cancelMove);
    auto* spacer = new QWidget(m_commandBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_commandBar->addWidget(spacer);
    // Uchwyty (przyciąganie, Snaps) – jak w Alphacam widać je tylko wtedy, gdy polecenie
    // czeka na punkt. Wybrany uchwyt: znika krzyż, a kursor klei się do takich punktów
    // geometrii. Drugie kliknięcie tego samego uchwytu go wyłącza.
    std::vector<QAction*> snaps;
    snaps.push_back(m_commandBar->addSeparator());
    snaps.push_back(m_commandBar->addWidget(new QLabel(" Uchwyty ", m_commandBar)));
    struct SnapDef
    {
        const char* name;
        OccView::Snap snap; // None = jeszcze bez działania
        int key;            // skrót klawiszowy (0 = brak)
    };
    const SnapDef snapDefs[] = {
        {"AUTO uchwyt (końce, środki, ćwiartki)", OccView::Snap::Auto, 0},
        {"KONIEC elementu", OccView::Snap::End, Qt::Key_F6},
        {"ŚRODEK elementu", OccView::Snap::Mid, Qt::Key_F7},
        {"CENTRUM okręgu", OccView::Snap::Centre, Qt::Key_F8},
        {"PRZECIĘCIE elementów", OccView::Snap::None, 0},
        {"STYCZNA do łuku lub okręgu", OccView::Snap::None, 0},
        {"PROSTOPADŁA do elementu", OccView::Snap::None, 0},
        {"RÓWNOLEGŁA do elementu", OccView::Snap::None, 0},
        {"ĆWIARTKI koła", OccView::Snap::Quadrant, 0},
    };
    for (int i = 0; i < 9; ++i) {
        const SnapDef& d = snapDefs[i];
        const QString name = QString::fromUtf8(d.name);
        if (d.snap == OccView::Snap::None) {
            snaps.push_back(m_commandBar->addWidget(placeholderButton(m_commandBar, name)));
            continue;
        }
        auto* b = new QToolButton(m_commandBar);
        b->setObjectName("snapButton");
        b->setIcon(snapIcon(i));
        b->setIconSize(QSize(18, 18));
        b->setFixedSize(26, 24);
        b->setCheckable(true);
        b->setFocusPolicy(Qt::NoFocus);
        b->setToolTip(d.key ? name + "  " + QKeySequence(d.key).toString() : name);
        const OccView::Snap snap = d.snap;
        connect(b, &QToolButton::toggled, this, [this, b, snap](bool on) {
            if (on) {
                for (QToolButton* other : m_snapButtons)
                    if (other != b)
                        other->setChecked(false); // naraz działa jeden uchwyt
                m_view->setSnap(snap);
            } else if (std::none_of(m_snapButtons.begin(), m_snapButtons.end(),
                                    [](QToolButton* x) { return x->isChecked(); })) {
                m_view->setSnap(OccView::Snap::None);
            }
        });
        if (d.key) {
            // Skrót działa w całym oknie (także gdy kursor stoi w polu X/Y), ale tylko
            // wtedy, gdy przycisk jest widoczny – czyli polecenie czeka na punkt.
            auto* act = new QAction(this);
            act->setShortcut(d.key);
            connect(act, &QAction::triggered, b, [b] {
                if (b->isVisible())
                    b->toggle();
            });
            addAction(act);
        }
        m_snapButtons.push_back(b);
        snaps.push_back(m_commandBar->addWidget(b));
    }
    snaps.push_back(m_commandBar->addWidget(new QLabel(" Filtry ", m_commandBar)));
    snaps.push_back(m_commandBar->addWidget(placeholderButton(m_commandBar, "Filtr 1")));
    snaps.push_back(m_commandBar->addWidget(placeholderButton(m_commandBar, "Filtr 2")));
    connect(m_inputBar, &InputBar::activeChanged, this, [this, snaps](bool pointInput) {
        for (QAction* a : snaps)
            a->setVisible(pointInput);
        if (!pointInput)
            for (QToolButton* b : m_snapButtons)
                b->setChecked(false); // koniec wskazywania – uchwyt się wyłącza
    });
    for (QAction* a : snaps)
        a->setVisible(false);
    addToolBar(Qt::BottomToolBarArea, m_commandBar);

    // Belki odcinamy od siebie liniami i lekkim cieniem, a pola i przyciski mają
    // wklęsłe ramki – żeby nic się nie zlewało z tłem. Kolory tekstu są stałe:
    // przy ciemnym motywie Windows Qt dałby jasny tekst na naszym jasnym tle.
    const QString barStyle = R"(
        QToolBar#commandBar QLabel, QStatusBar, QStatusBar QLabel { color: black; }
        QToolBar#commandBar QPushButton {
            color: black;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e6e6e6);
            border: 1px solid #a8a8a8; border-radius: 2px; padding: 2px 14px;
        }
        QToolBar#commandBar QPushButton:pressed { background: #cfe3f7; }
        QToolBar#commandBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f7f7f7, stop:1 #e4e4e4);
            border-top: 1px solid #8c8c8c; border-bottom: 1px solid #8c8c8c;
            padding: 2px 4px; spacing: 3px;
        }
        QStatusBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ececec, stop:1 #dcdcdc);
            border-top: 1px solid #ffffff;
        }
        QStatusBar::item { border: none; }
        QToolButton[placeholder="true"] {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e6e6e6);
            border: 1px solid #a8a8a8; border-radius: 2px;
        }
        QToolButton#snapButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e6e6e6);
            border: 1px solid #a8a8a8; border-radius: 2px;
        }
        QToolButton#snapButton:hover { border-color: #3d7fd1; }
        QToolButton#snapButton:checked { background: #9a9a9a; border: 1px solid #505050; }
        QLabel#cursorLabel {
            background: #fafafa; border: 1px solid #a8a8a8; border-radius: 2px; padding: 1px 6px;
        }
        QWidget#barGroup { border-left: 1px solid #b0b0b0; }
        QLabel#inputCommand { font-weight: bold; }
        QWidget#inputBar QLineEdit {
            color: black; background: #ffffff; border: 1px solid #a8a8a8; border-radius: 2px; padding: 1px 3px;
            selection-background-color: #3d7fd1; selection-color: white;
        }
        QWidget#inputBar QLineEdit[error="true"] { background: #ffd6d6; border-color: #c03030; }
        QToolBar#commandBar QPushButton#inputHint {
            color: #202020; background: #b4b4b4; border: 1px solid #7a7a7a; border-radius: 1px;
            padding: 3px 12px;
        }
        QToolButton#inputF1 {
            color: black;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e6e6e6);
            border: 1px solid #a8a8a8; border-radius: 2px; padding: 1px 5px;
        }
    )";
    setStyleSheet(styleSheet() + barStyle);

    // Stopka (najniżej): komunikaty, współrzędne kursora, widoki i przełączniki.
    m_cursorLabel = new QLabel("X –   Y –", this);
    m_cursorLabel->setObjectName("cursorLabel");
    m_cursorLabel->setMinimumWidth(190);
    m_cursorLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    statusBar()->addPermanentWidget(m_cursorLabel);
    connect(m_view, &OccView::cursorMoved, this, [this](double x, double y) {
        m_cursorLabel->setText(QString("X %1   Y %2").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3));
    });

    // Esc przerywa polecenie także wtedy, gdy widok 3D nie ma fokusu.
    auto* escAct = new QAction(this);
    escAct->setShortcut(Qt::Key_Escape);
    connect(escAct, &QAction::triggered, this, &MainWindow::cancelMove);
    addAction(escAct);

    auto* views = new QWidget(this);
    views->setObjectName("barGroup");
    auto* viewsLayout = new QHBoxLayout(views);
    viewsLayout->setContentsMargins(8, 0, 0, 0);
    viewsLayout->setSpacing(1);
    for (int i = 1; i <= 15; ++i)
        viewsLayout->addWidget(placeholderButton(views, QString("Widok %1").arg(i), 22));
    statusBar()->addPermanentWidget(views);

    auto* toggles = new QWidget(this);
    toggles->setObjectName("barGroup");
    auto* togglesLayout = new QHBoxLayout(toggles);
    togglesLayout->setContentsMargins(8, 0, 0, 0);
    togglesLayout->setSpacing(1);
    for (int i = 1; i <= 4; ++i)
        togglesLayout->addWidget(placeholderButton(toggles, QString("Przełącznik %1").arg(i), 50));
    statusBar()->addPermanentWidget(toggles);
}

void MainWindow::onMove()
{
    if (m_shown.shape.IsNull()) {
        statusBar()->showMessage("Najpierw otwórz model (Ctrl+O)");
        return;
    }
    m_moveStep = MoveStep::Selecting;
    m_inputBar->startSelect("Przesuń:", "Wskaż");
    m_view->clearSelection();
    m_view->setInteraction(OccView::Interaction::Select);
    m_view->setFocus();
    statusBar()->showMessage("Przesuń: wskaż bryłę lub geometrie, PPM zatwierdza, Esc anuluje");
}

void MainWindow::onSelectionConfirmed()
{
    if (m_moveStep != MoveStep::Selecting)
        return;
    m_moveGeometries = m_view->selectedGeometries();
    m_moveModel = m_view->isModelSelected();
    if (m_moveGeometries.empty() && !m_moveModel) {
        statusBar()->showMessage("Przesuń: nic nie wybrano – kliknij element (LPM), potem PPM");
        return;
    }
    // Wybór zostaje podświetlony; teraz wskazujemy, o ile przesunąć.
    m_moveStep = MoveStep::PickBase;
    m_view->setInteraction(OccView::Interaction::PickPoint);
    m_inputBar->startPoint("Przesuń:", "Punkt bazowy");
    statusBar()->showMessage("Przesuń: kliknij punkt bazowy albo wpisz go w pasku wprowadzania i Enter");
}

void MainWindow::onPointPicked(double x, double y, double /*z*/)
{
    // Kliknięcie w widoku: wartości wpisane (przypięte) w pasku wprowadzania
    // zastępują odpowiednie współrzędne kursora.
    if (const auto p = m_inputBar->resolveClick(x, y))
        onPointEntered(p->X(), p->Y(), p->Z());
    else
        statusBar()->showMessage("Błędna wartość w pasku wprowadzania – popraw pole zaznaczone na czerwono");
}

void MainWindow::onPointEntered(double x, double y, double z)
{
    // Po zatwierdzeniu punktu klawiatura wraca do widoku – kolejne pisanie
    // zaczyna się od pierwszego pola następnego punktu.
    m_view->setFocus();
    if (m_moveStep == MoveStep::PickBase) {
        m_moveBase = gp_Pnt(x, y, z);
        m_moveStep = MoveStep::PickTarget;
        m_inputBar->startPoint("Przesuń:", "Punkt docelowy");
        statusBar()->showMessage(QString("Przesuń: punkt bazowy X %1 Y %2 Z %3 – wskaż punkt docelowy")
                                     .arg(x, 0, 'f', 2)
                                     .arg(y, 0, 'f', 2)
                                     .arg(z, 0, 'f', 2));
    } else if (m_moveStep == MoveStep::PickTarget) {
        applyMove(gp_Vec(m_moveBase, gp_Pnt(x, y, z)));
    }
}

void MainWindow::applyMove(const gp_Vec& offset)
{
    const EditState before = editState();
    if (m_moveModel) {
        gp_Trsf t;
        t.SetTranslation(offset);
        m_shown = camcore::transformed(m_shown, t);
        m_view->updateModel(m_shown);
    }
    for (int i : m_moveGeometries) {
        camcore::Geometry& g = m_geometries[static_cast<size_t>(i)];
        g.contour = camcore::translated(g.contour, offset);
    }
    if (!m_moveGeometries.empty())
        showGeometries();
    m_undo->push(new EditCommand(this, "Przesuń", before, editState()));
    finishMove(QString("Przesunięto o dX %1  dY %2  dZ %3 mm")
                   .arg(offset.X(), 0, 'f', 2)
                   .arg(offset.Y(), 0, 'f', 2)
                   .arg(offset.Z(), 0, 'f', 2));
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

void MainWindow::cancelMove()
{
    if (m_moveStep != MoveStep::None)
        finishMove("Przesuwanie anulowane");
}

void MainWindow::finishMove(const QString& message)
{
    m_moveStep = MoveStep::None;
    m_moveGeometries.clear();
    m_moveModel = false;
    m_inputBar->showIdle();
    m_view->setInteraction(OccView::Interaction::Navigate);
    statusBar()->showMessage(message);
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
