#pragma once

#include <QWidget>

#include <optional>
#include <vector>

#include <gp_Pnt.hxx>

class QKeyEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;

// Pasek wprowadzania (jak Input Bar w Alphacam), w belce polecenia.
//  - Po lewej podpowiedź: nazwa polecenia i co teraz wskazać ("Przesuń: Punkt bazowy").
//  - Pola X / Y / Z (albo dX / dY / dZ, albo L / Kąt / dZ) pokazują na żywo położenie
//    kursora. Wpisana wartość "przypina" pole (pogrubione), a pozostałe pola dalej
//    idą za kursorem. Kliknięcie w widoku bierze przypięte wartości z pól.
//  - Tab / Shift+Tab przechodzi między polami, Enter zatwierdza punkt, Esc przerywa.
//  - Pisanie w widoku 3D od razu trafia do pierwszego pola.
//  - Przełącznik Abs / Przyr / Bieg: współrzędne bezwzględne, przyrostowe (od
//    poprzedniego punktu) albo biegunowe (długość i kąt od poprzedniego punktu).
//  - W polach można wpisywać wyrażenia, np. 100/3+2*(5-1); przecinek = kropka.
class InputBar : public QWidget
{
    Q_OBJECT

public:
    enum class Mode { Absolute, Incremental, Polar };

    explicit InputBar(QWidget* parent = nullptr);

    // Brak polecenia – sama szara podpowiedź.
    void showIdle();
    // Polecenie wybiera elementy: podpowiedź + przyciski "Gotowe" i "Anuluj".
    void startSelect(const QString& command, const QString& prompt);
    // Polecenie czeka na punkt. reference = poprzedni punkt (dla Przyr i Bieg);
    // bez niego liczymy od 0,0,0.
    void startPoint(const QString& command, const QString& prompt,
                    std::optional<gp_Pnt> reference = std::nullopt);
    void setPrompt(const QString& prompt);

    bool isPicking() const { return m_picking; }

    // Kursor nad widokiem (płaszczyzna Z = 0) – nieprzypięte pola pokazują jego położenie.
    void trackCursor(double x, double y);
    // Punkt kliknięty w widoku z podstawionymi wartościami przypiętych pól.
    std::optional<gp_Pnt> resolveClick(double x, double y);
    // Klawisz wciśnięty w widoku 3D: cyfry zaczynają wpisywanie, Enter zatwierdza,
    // Tab przechodzi do pól. Zwraca true, gdy klawisz został obsłużony.
    bool handleViewKey(QKeyEvent* e);

    Mode mode() const { return m_mode; }
    void setMode(Mode mode);

signals:
    void pointEntered(double x, double y, double z); // punkt bezwzględny
    void selectionDone();                            // "Gotowe" przy wyborze
    void cancelled();                                // "Anuluj"

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateLabels();
    void refreshTracked();
    void setLocked(int field, bool locked);
    void focusField(int field);
    void accept();
    // Wartości pól dla punktu bezwzględnego p w bieżącym trybie.
    std::vector<double> fieldsFor(const gp_Pnt& p) const;
    // Punkt bezwzględny z wartości pól w bieżącym trybie.
    gp_Pnt pointFrom(const std::vector<double>& values) const;
    std::optional<gp_Pnt> resolve(const gp_Pnt& cursor, bool markErrors);

    QLabel* m_command = nullptr;
    QLabel* m_prompt = nullptr;
    QWidget* m_fieldsBox = nullptr;
    std::vector<QLabel*> m_labels;
    std::vector<QLineEdit*> m_fields;
    std::vector<bool> m_locked;
    QToolButton* m_modeButtons[3] = {};
    QPushButton* m_done = nullptr;
    QPushButton* m_cancel = nullptr;

    Mode m_mode = Mode::Absolute;
    bool m_picking = false;
    gp_Pnt m_reference{0, 0, 0};
    gp_Pnt m_cursor{0, 0, 0};
};

// Liczy wyrażenie z pola (+ - * / i nawiasy, przecinek jak kropka).
// Pusty tekst albo błąd składni = brak wartości.
std::optional<double> evaluateExpression(const QString& text);
