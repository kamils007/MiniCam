#include "MainWindow.h"
#include "AlignSettingsDialog.h"
#include "LayersPanel.h"
#include "OccView.h"
#include "Ribbon.h"

#include <QAction>
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

#include <cmath>
#include <exception>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("MiniCAM");

    m_view = new OccView(this);
    setCentralWidget(m_view);
    m_alignSettings = loadAlignSettings();
    m_geometryLayers = {
        {camcore::kApsLayer, camcore::kApsColor},
        {camcore::kOuterContourLayer, Quantity_Color(0.10, 0.45, 1.00, Quantity_TOC_sRGB)}, // niebieska
        {camcore::kInnerContourLayer, Quantity_Color(0.85, 0.10, 0.85, Quantity_TOC_sRGB)}, // fioletowa
    };

    createDock();
    createRibbon();
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
    std::vector<LayersPanel::LayerRow> userLayers;
    for (size_t i = 1; i < m_geometryLayers.size(); ++i) {
        double r, g, b;
        m_geometryLayers[i].color.Values(r, g, b, Quantity_TOC_sRGB);
        userLayers.push_back({QString::fromStdString(m_geometryLayers[i].name), QColor::fromRgbF(r, g, b)});
    }
    m_layers->setUserLayers(userLayers);
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
    m_view->showModel(model);
    m_layers->setModelName(m_fileName);
    clearFeatures(); // cechy dotyczyły poprzedniego położenia bryły
}

void MainWindow::clearFeatures()
{
    m_contours = {};
    m_geometries.clear();
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
        contours.push_back({g.contour.wire, layerOf(g).color, g.contour.zTop - g.contour.zBottom});
        rows.push_back({geometryText(i, g.contour), QString::fromStdString(g.layer), g.visible});
    }
    m_view->showGeometry(contours);
    for (size_t i = 0; i < m_geometries.size(); ++i)
        if (!m_geometries[i].visible)
            m_view->setGeometryVisible(static_cast<int>(i), false);
    m_layers->setGeometries(rows);
}

const camcore::Layer& MainWindow::layerOf(const camcore::Geometry& g) const
{
    for (const camcore::Layer& l : m_geometryLayers)
        if (l.name == g.layer)
            return l;
    return m_geometryLayers.front(); // nieznana warstwa – jak APS
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
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_contours = camcore::buildPartContours(m_shown.shape);
    QApplication::restoreOverrideCursor();

    // Kontur zewnętrzny (obrys bryły) idzie do warstwy użytkownika userKonturZew,
    // kontury wewnętrzne (wycięcia na wylot) do userKonturWew, reszta to na razie
    // geometrie niesklasyfikowane – warstwa APS. Kieszenie zostają w m_contours na później.
    m_geometries.clear();
    auto add = [this](const camcore::Contour& c, const char* layer = camcore::kApsLayer) {
        camcore::Geometry g;
        g.contour = c;
        g.layer = layer;
        m_geometries.push_back(g);
    };
    if (!m_contours.outline.geometry.empty())
        add(m_contours.outline, camcore::kOuterContourLayer);
    for (const camcore::Contour& c : m_contours.inner)
        add(c, camcore::kInnerContourLayer);
    for (const camcore::Pocket& p : m_contours.pockets)
        for (const camcore::Contour& c : p.contours)
            add(c);
    showGeometries();
    statusBar()->showMessage(QString("Wyciągnięto %1 geometrii (obrys → %2, wycięcia na wylot → %3, reszta → %4)")
                                 .arg(m_geometries.size())
                                 .arg(camcore::kOuterContourLayer, camcore::kInnerContourLayer, camcore::kApsLayer));
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
