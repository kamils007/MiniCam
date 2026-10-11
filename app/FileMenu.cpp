// Menu Plik (niebieski przycisk na wstążce) – polecenia jak w Alphacam:
// Nowy, Otwórz, Zapisz, Zapisz jako, Drukarka/Ploter, Podgląd wydruku,
// Ostatnie, Pomoc, Dostosuj (na razie wyłączone), Zakończ.
#include "MainWindow.h"
#include "Icons.h"
#include "LayersPanel.h"
#include "DxfExport.h"
#include "ModelExport.h"
#include "OccView.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QSaveFile>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QSettings>
#include <QStatusBar>
#include <QStyle>
#include <QUndoStack>

#include <algorithm>
#include <exception>

namespace {

constexpr int kMaxRecent = 10;
const char* const kRecentKey = "ostatniePliki";

} // namespace

void MainWindow::createFileMenu(QMenu* menu, QAction* quitAct)
{
    auto add = [this, menu](QStyle::StandardPixmap icon, const QString& text,
                            const QKeySequence& shortcut = {}) {
        QAction* a = menu->addAction(style()->standardIcon(icon), text);
        a->setShortcut(shortcut);
        addAction(a); // skrót działa w całym oknie, nie tylko przy otwartym menu
        return a;
    };

    connect(add(QStyle::SP_FileIcon, "Nowy", QKeySequence::New), &QAction::triggered,
            this, &MainWindow::onNew);

    QAction* openAct = menu->addAction(appIcon("otworz-model"), "Otwórz…");
    openAct->setShortcut(QKeySequence::Open);
    openAct->setShortcutContext(Qt::WidgetShortcut); // skrót Ctrl+O obsługuje już okno
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpen);

    connect(add(QStyle::SP_DialogSaveButton, "Zapisz", QKeySequence::Save), &QAction::triggered,
            this, &MainWindow::onSave);
    connect(add(QStyle::SP_DialogSaveButton, "Zapisz jako…", QKeySequence::SaveAs),
            &QAction::triggered, this, &MainWindow::onSaveAs);
    menu->addSeparator();
    connect(add(QStyle::SP_ComputerIcon, "Drukarka/Ploter…", QKeySequence::Print),
            &QAction::triggered, this, [this] { onPrint(false); });
    connect(add(QStyle::SP_FileDialogContentsView, "Podgląd wydruku…"), &QAction::triggered,
            this, [this] { onPrint(true); });
    menu->addSeparator();

    m_recentMenu = menu->addMenu(style()->standardIcon(QStyle::SP_DirOpenIcon), "Ostatnie");
    updateRecentMenu();

    connect(add(QStyle::SP_DialogHelpButton, "Pomoc"), &QAction::triggered, this, [this] {
        QMessageBox::about(this, "Pomoc – MiniCAM",
                           "<b>MiniCAM</b><br>Prosty CAM do detali 3D (STEP/IGES/BREP), "
                           "wzorowany na Alphacam.<br><br>"
                           "Widok: LPM – obrót, ŚPM/PPM – przesuwanie, kółko – zoom, F – dopasuj.<br>"
                           "PPM w widoku – menu radialne.");
    });

    QMenu* customise = menu->addMenu(style()->standardIcon(QStyle::SP_FileDialogDetailedView), "Dostosuj");
    for (const char* text : {"Dostosuj wstążkę…", "Pasek narzędzi szybkiego dostępu…",
                             "Menu radialne…", "Klawiatura…", "Opcje…", "Wygląd…"}) {
        QAction* a = customise->addAction(QString::fromUtf8(text));
        a->setEnabled(false);
    }

    menu->addSeparator();
    quitAct->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    menu->addAction(quitAct);
}

void MainWindow::addRecentFile(const QString& path)
{
    const QString file = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
    QSettings settings("MiniCAM", "MiniCAM");
    QStringList files = settings.value(kRecentKey).toStringList();
    files.removeAll(file);
    files.prepend(file);
    while (files.size() > kMaxRecent)
        files.removeLast();
    settings.setValue(kRecentKey, files);
    updateRecentMenu();
}

void MainWindow::updateRecentMenu()
{
    if (!m_recentMenu)
        return;
    m_recentMenu->clear();
    const QStringList files = QSettings("MiniCAM", "MiniCAM").value(kRecentKey).toStringList();
    int number = 1;
    for (const QString& file : files) {
        // Numer z & – klawisz 1…9 wybiera plik przy otwartym menu.
        const QString text = number < 10 ? QString("&%1  %2").arg(number).arg(file)
                                         : QString("%1  %2").arg(number).arg(file);
        ++number;
        QAction* a = m_recentMenu->addAction(text);
        connect(a, &QAction::triggered, this, [this, file] {
            if (maybeSave())
                openFile(file);
        });
    }
    if (files.isEmpty())
        m_recentMenu->addAction("(brak)")->setEnabled(false);
}

void MainWindow::onNew()
{
    if (!maybeSave())
        return;
    cancelCommand();
    m_original = {};
    m_shown = {};
    m_fileName.clear();
    m_savePath.clear();
    m_aligned = m_flipped = false;
    m_view->updateModel(m_shown); // pusta bryła = nic nie rysujemy
    m_layers->setModelName(QString());
    clearFeatures();
    m_undo->clear();
    setWindowTitle("MiniCAM");
    statusBar()->showMessage("Nowy rysunek");
}

bool MainWindow::onSave()
{
    return m_savePath.isEmpty() ? onSaveAs() : saveTo(m_savePath);
}

bool MainWindow::onSaveAs()
{
    if (m_shown.shape.IsNull()) {
        QMessageBox::information(this, "Zapisz jako", "Brak bryły do zapisania.");
        return false;
    }
    QString suggested = m_savePath;
    if (suggested.isEmpty()) {
        const QStringList recent = QSettings("MiniCAM", "MiniCAM").value(kRecentKey).toStringList();
        const QString dir = recent.isEmpty() ? QDir::homePath() : QFileInfo(recent.first()).absolutePath();
        const QString base = m_fileName.isEmpty() ? "rysunek" : QFileInfo(m_fileName).completeBaseName();
        suggested = QDir(dir).filePath(base + ".step");
    }
    QString path = QFileDialog::getSaveFileName(this, "Zapisz jako", suggested,
                                                "STEP (*.step *.stp);;BREP (*.brep *.brp)");
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().isEmpty())
        path += ".step";
    return saveTo(path);
}

bool MainWindow::saveTo(const QString& path)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    try {
        // Bryła tak, jak jest na ekranie (po wyrównaniu i przesunięciach).
        camcore::exportModel(m_shown, path.toUtf8().toStdString());
    } catch (const std::exception& ex) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(this, "Błąd zapisu", QString::fromUtf8(ex.what()));
        return false;
    }
    QApplication::restoreOverrideCursor();

    m_savePath = path;
    m_fileName = QFileInfo(path).fileName();
    m_layers->setModelName(m_fileName);
    m_undo->setClean();
    addRecentFile(path);
    setWindowTitle("MiniCAM – " + m_fileName);
    statusBar()->showMessage("Zapisano " + QDir::toNativeSeparators(path));
    return true;
}

void MainWindow::onOutputCad()
{
    if (m_geometries.empty()) {
        QMessageBox::information(this, "Zapisz CAD", "Brak geometrii do zapisania.");
        return;
    }
    QString start = m_savePath.isEmpty() ? QString() : QFileInfo(m_savePath).path();
    if (!m_fileName.isEmpty())
        start = QDir(start).filePath(QFileInfo(m_fileName).completeBaseName() + ".dxf");
    QString path = QFileDialog::getSaveFileName(this, "Zapisz CAD", start, "DXF (*.dxf)");
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += ".dxf";

    const std::string dxf = camcore::writeDxf(m_geometries, m_layerList);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(dxf.data(), static_cast<qint64>(dxf.size())) != static_cast<qint64>(dxf.size())
        || !file.commit()) {
        QMessageBox::critical(this, "Błąd zapisu", "Nie udało się zapisać pliku " + QDir::toNativeSeparators(path));
        return;
    }
    statusBar()->showMessage("Zapisano DXF: " + QDir::toNativeSeparators(path));
}

bool MainWindow::maybeSave()
{
    if (m_shown.shape.IsNull() || m_undo->isClean())
        return true;
    const auto answer = QMessageBox::question(
        this, "MiniCAM",
        QString("Rysunek %1 został zmieniony. Zapisać zmiany?").arg(m_fileName),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
    if (answer == QMessageBox::Yes)
        return onSave();
    return answer == QMessageBox::No;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (maybeSave())
        event->accept();
    else
        event->ignore();
}

void MainWindow::onPrint(bool preview)
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageOrientation(QPageLayout::Landscape);
    printer.setDocName(m_fileName.isEmpty() ? "MiniCAM" : m_fileName);
    if (preview) {
        QPrintPreviewDialog dialog(&printer, this);
        dialog.setWindowTitle("Podgląd wydruku");
        connect(&dialog, &QPrintPreviewDialog::paintRequested, this, &MainWindow::printTo);
        dialog.exec();
    } else {
        QPrintDialog dialog(&printer, this);
        dialog.setWindowTitle("Drukarka/Ploter");
        if (dialog.exec() == QDialog::Accepted)
            printTo(&printer);
    }
}

// Drukuje bieżący widok (jak na ekranie, na białym tle) dopasowany do strony.
void MainWindow::printTo(QPrinter* printer)
{
    QPainter painter(printer);
    const QRect page = painter.viewport();

    // Obraz w proporcjach okna widoku; nie większy niż ~4000 px na dłuższym boku.
    QSize size = m_view->size();
    size.scale(page.size(), Qt::KeepAspectRatio);
    const double limit = 4000.0 / std::max(size.width(), size.height());
    if (limit < 1.0)
        size *= limit;
    const QImage image = m_view->renderImage(std::max(size.width(), 1), std::max(size.height(), 1));
    if (image.isNull()) {
        statusBar()->showMessage("Nie udało się przygotować obrazu do wydruku");
        return;
    }

    // Stopka: nazwa pliku.
    const int footer = painter.fontMetrics().height() * 2;
    QRect area = page.adjusted(0, 0, 0, -footer);
    QSize target = image.size();
    target.scale(area.size(), Qt::KeepAspectRatio);
    const QRect imageRect(area.x() + (area.width() - target.width()) / 2,
                          area.y() + (area.height() - target.height()) / 2, target.width(), target.height());
    painter.drawImage(imageRect, image);
    painter.drawText(page.adjusted(0, page.height() - footer, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                     "MiniCAM – " + (m_fileName.isEmpty() ? QString("bez nazwy") : m_fileName));
}
