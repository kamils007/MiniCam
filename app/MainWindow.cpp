#include "MainWindow.h"
#include "AlignSettingsDialog.h"
#include "OccView.h"
#include "Ribbon.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QPixmap>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>
#include <QStyle>
#include <QTreeWidget>

#include <cmath>
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
    // Na razie okno pokazuje listę rozpoznanych cech (drzewko: grupa → cecha).
    m_featureTree = new QTreeWidget(m_dock);
    m_featureTree->setHeaderHidden(true);
    connect(m_featureTree, &QTreeWidget::itemClicked, this, &MainWindow::onFeatureClicked);
    m_dock->setWidget(m_featureTree);
    clearFeatures();
    m_dock->setMinimumWidth(220);
    // Tło trochę jaśniejsze niż pas ikon na wstążce, kolory niezależne od motywu Windows.
    m_dock->setStyleSheet(R"(
        QDockWidget { color: black; }
        QDockWidget::title { background: #d4d4d4; padding: 4px; }
        QDockWidget > QWidget { background: #efefef; color: black; border: none; }
        QTreeWidget::item:selected { background: #cfe3f7; color: black; }
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
    dockAct->setText("Okno\nDodatki");
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
    clearFeatures(); // cechy dotyczyły poprzedniego położenia bryły
}

void MainWindow::clearFeatures()
{
    m_contours = {};
    m_featureTree->clear();
    auto* hint = new QTreeWidgetItem(m_featureTree, {"Kontury: Ekstrakcja → Rozpoznaj cechy"});
    hint->setFlags(Qt::NoItemFlags);
    m_view->showGeometry({});
}

namespace {

QString num(double v) { return QString::number(v, 'f', v == std::floor(v) ? 0 : 1); }

// Kolory jak warstwy w Alphacam: obrys, kontury wewnętrzne, kieszenie.
const Quantity_Color kOutlineColor(0.10, 0.45, 1.00, Quantity_TOC_sRGB); // niebieski
const Quantity_Color kInnerColor(0.85, 0.10, 0.85, Quantity_TOC_sRGB);   // fioletowy
const Quantity_Color kPocketColor(0.00, 0.65, 0.20, Quantity_TOC_sRGB);  // zielony

QIcon colorIcon(const Quantity_Color& color)
{
    QPixmap pix(12, 12);
    double r, g, b;
    color.Values(r, g, b, Quantity_TOC_sRGB);
    pix.fill(QColor::fromRgbF(r, g, b));
    return QIcon(pix);
}

QString shapeText(const camcore::Contour& c)
{
    return c.diameter > 0 ? "Ø" + num(c.diameter) : num(c.sizeX) + " × " + num(c.sizeY);
}

QString contourText(const camcore::Contour& c)
{
    return QString("%1, wys. %2, geometrii: %3")
        .arg(shapeText(c), num(c.zTop - c.zBottom))
        .arg(c.geometry.size());
}

} // namespace

void MainWindow::onRecognizeFeatures()
{
    if (m_shown.shape.IsNull()) {
        statusBar()->showMessage("Najpierw otwórz model (Ctrl+O)");
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    clearFeatures();
    m_contours = camcore::buildPartContours(m_shown.shape);
    QApplication::restoreOverrideCursor();

    // Wszystkie kontury trafiają do widoku jedną listą. Element drzewka pamięta
    // zakres numerów swoich konturów na tej liście [od, do) – kliknięcie go podświetla.
    std::vector<OccView::Contour> contours;
    auto addContour = [&](const camcore::Contour& c, const Quantity_Color& color) {
        contours.push_back({c.wire, color, c.zTop - c.zBottom});
    };
    auto setRange = [](QTreeWidgetItem* item, size_t first, size_t last) {
        item->setData(0, Qt::UserRole, static_cast<int>(first));
        item->setData(0, Qt::UserRole + 1, static_cast<int>(last));
    };

    m_featureTree->clear();
    if (!m_contours.outline.geometry.empty()) {
        auto* item = new QTreeWidgetItem(m_featureTree, {"Kontur: " + contourText(m_contours.outline)});
        item->setIcon(0, colorIcon(kOutlineColor));
        setRange(item, contours.size(), contours.size() + 1);
        addContour(m_contours.outline, kOutlineColor);
    }

    auto* innerGroup = new QTreeWidgetItem(m_featureTree);
    innerGroup->setText(0, QString("Kontury wewnętrzne (%1)").arg(m_contours.inner.size()));
    innerGroup->setIcon(0, colorIcon(kInnerColor));
    const size_t innerFirst = contours.size();
    for (const camcore::Contour& c : m_contours.inner) {
        auto* item = new QTreeWidgetItem(innerGroup, {contourText(c)});
        setRange(item, contours.size(), contours.size() + 1);
        addContour(c, kInnerColor);
    }
    setRange(innerGroup, innerFirst, contours.size());

    auto* pocketGroup = new QTreeWidgetItem(m_featureTree);
    pocketGroup->setText(0, QString("Kieszenie (%1)").arg(m_contours.pockets.size()));
    pocketGroup->setIcon(0, colorIcon(kPocketColor));
    const size_t pocketsFirst = contours.size();
    for (size_t pi = 0; pi < m_contours.pockets.size(); ++pi) {
        const camcore::Pocket& p = m_contours.pockets[pi];
        auto* pocketItem = new QTreeWidgetItem(pocketGroup);
        pocketItem->setText(0, QString("Kieszeń %1: %2, gł. %3")
                                   .arg(pi + 1)
                                   .arg(shapeText(p.contours.front()), num(p.depth)));
        const size_t first = contours.size();
        for (size_t ci = 0; ci < p.contours.size(); ++ci) {
            const QString kind = ci == 0 ? "zewnętrzny" : "wewnętrzny";
            auto* item = new QTreeWidgetItem(pocketItem, {"Kontur " + kind + ": " + contourText(p.contours[ci])});
            setRange(item, contours.size(), contours.size() + 1);
            addContour(p.contours[ci], kPocketColor);
        }
        setRange(pocketItem, first, contours.size());
    }
    setRange(pocketGroup, pocketsFirst, contours.size());
    m_featureTree->expandAll();

    m_view->showGeometry(contours);
    statusBar()->showMessage(QString("Kontur, %1 konturów wewnętrznych, %2 kieszeni – kliknij na liście, żeby podświetlić")
                                 .arg(m_contours.inner.size())
                                 .arg(m_contours.pockets.size()));
}

void MainWindow::onFeatureClicked(QTreeWidgetItem* item)
{
    if (!item->data(0, Qt::UserRole).isValid())
        return;
    const int first = item->data(0, Qt::UserRole).toInt();
    const int last = item->data(0, Qt::UserRole + 1).toInt();
    std::vector<int> selected;
    for (int i = first; i < last; ++i)
        selected.push_back(i);
    statusBar()->showMessage(item->text(0));
    m_view->highlightGeometry(selected);
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
