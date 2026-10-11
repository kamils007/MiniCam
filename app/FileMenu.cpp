// Menu Plik (niebieski przycisk na wstążce) – polecenia jak w Alphacam.
// Działa Otwórz, Ostatnie i Zakończ; pozostałe są widoczne, ale wyłączone
// i będą uzupełniane po kolei.
#include "MainWindow.h"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QSettings>
#include <QStyle>

namespace {

constexpr int kMaxRecent = 10;
const char* const kRecentKey = "ostatniePliki";

} // namespace

void MainWindow::createFileMenu(QMenu* menu, QAction* quitAct)
{
    auto soon = [this, menu](QStyle::StandardPixmap icon, const QString& text,
                             const QKeySequence& shortcut = {}) {
        QAction* a = menu->addAction(style()->standardIcon(icon), text);
        a->setShortcut(shortcut);
        a->setEnabled(false);
        a->setToolTip(QString(text).remove("…") + " (wkrótce)");
        return a;
    };

    soon(QStyle::SP_FileIcon, "Nowy", QKeySequence::New);

    QAction* openAct = menu->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton), "Otwórz…");
    openAct->setShortcut(QKeySequence::Open);
    openAct->setShortcutContext(Qt::WidgetShortcut); // skrót Ctrl+O obsługuje już okno
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpen);

    soon(QStyle::SP_DialogSaveButton, "Zapisz", QKeySequence::Save);
    soon(QStyle::SP_DialogSaveButton, "Zapisz jako…");
    menu->addSeparator();
    soon(QStyle::SP_ComputerIcon, "Drukarka/Ploter…", QKeySequence::Print);
    soon(QStyle::SP_FileDialogContentsView, "Podgląd wydruku…");
    menu->addSeparator();

    m_recentMenu = menu->addMenu(style()->standardIcon(QStyle::SP_DirOpenIcon), "Ostatnie");
    updateRecentMenu();

    soon(QStyle::SP_DialogHelpButton, "Pomoc");

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
        connect(a, &QAction::triggered, this, [this, file] { openFile(file); });
    }
    if (files.isEmpty())
        m_recentMenu->addAction("(brak)")->setEnabled(false);
}
