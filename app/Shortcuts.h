#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>

#include <vector>

class QAction;
class QShortcut;

// Skróty klawiszowe programu – wszystkie w jednym miejscu (pod przyszłe okno
// Plik → Dostosuj → Klawiatura, jak w Alphacam). Każdy skrót ma stały identyfikator,
// nazwę do pokazania w oknie i klawisze domyślne. Zmienione klawisze zapisują się
// w ustawieniach programu (grupa "skroty"); akcje podpięte przez bind() dostają je od razu.
//
// Nowy skrót: identyfikator poniżej, wiersz w tabeli w Shortcuts.cpp i bind() przy akcji.
namespace shortcuts {

// Identyfikatory (zapisywane w ustawieniach – nie zmieniać).
inline const char* const kNew = "plik.nowy";
inline const char* const kOpen = "plik.otworz";
inline const char* const kSave = "plik.zapisz";
inline const char* const kSaveAs = "plik.zapiszJako";
inline const char* const kPrint = "plik.drukuj";
inline const char* const kQuit = "plik.zakoncz";
inline const char* const kUndo = "edycja.cofnij";
inline const char* const kRedo = "edycja.ponow";
inline const char* const kDelete = "edycja.usun";
inline const char* const kFitAll = "widok.dopasuj";
inline const char* const kToggleDirections = "widok.kierunki";
inline const char* const kRepeatCommand = "polecenie.powtorz";
inline const char* const kCancelCommand = "polecenie.przerwij";
inline const char* const kSnapEnd = "uchwyt.koniec";
inline const char* const kSnapMid = "uchwyt.srodek";
inline const char* const kSnapCentre = "uchwyt.centrum";

struct Definition
{
    QString id;
    QString group; // np. "Plik", "Widok" – grupy w oknie Klawiatura
    QString name;  // co robi skrót, po polsku
    QList<QKeySequence> defaults;
};

// Wszystkie skróty programu w kolejności do pokazania.
const std::vector<Definition>& all();

// Aktualne klawisze skrótu: zmienione przez użytkownika albo domyślne.
QList<QKeySequence> keys(const QString& id);
// Pierwszy klawisz jako tekst, np. "Ctrl+G" – do podpowiedzi i komunikatów.
QString text(const QString& id);

// Zmiana klawiszy (zapis w ustawieniach i od razu w podpiętych akcjach).
void setKeys(const QString& id, const QList<QKeySequence>& keys);
void resetToDefaults();

// Podpina akcję albo skrót pod identyfikator: ustawia klawisze teraz i po każdej zmianie.
void bind(QAction* action, const QString& id);
void bind(QShortcut* shortcut, const QString& id);

} // namespace shortcuts
