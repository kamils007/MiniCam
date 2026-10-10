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

// Pasek wprowadzania – jak Input Bar w Alphacam ("LINE From  X [0] F1=?  Y [0] F1=?  OK"),
// u nas z polem Z – bryłę przesuwamy także w pionie.
//  - Bez polecenia pasek jest pusty.
//  - Polecenie, które potrzebuje danych, pokazuje podpowiedź (np. "PRZESUŃ Punkt bazowy"),
//    pola X, Y, Z oraz przycisk OK. Punkt można wpisać w pola albo kliknąć w widoku.
//  - W polach można wpisywać wyrażenia, np. 100/3+2*(5-1); przecinek = kropka.
//  - F1 albo przycisk "F1=?" przy polu pomija wartość, której nie znamy. Pasek pokazuje
//    wtedy inną podpowiedź: brakującą współrzędną bierzemy z kliknięcia w widoku,
//    a wpisane zostają.
//  - Enter = OK, Tab / Shift+Tab przechodzi między polami, Esc przerywa polecenie.
//    Pisanie w widoku 3D od razu trafia do pola X.
class InputBar : public QWidget
{
    Q_OBJECT

public:
    explicit InputBar(QWidget* parent = nullptr);

    // Brak polecenia – pusty pasek.
    void showIdle();
    // Polecenie wybiera elementy: podpowiedź i przyciski Poprzednie / Zakończ (ESC) /
    // Wszystko (A) / Warstwy (L) – na razie bez działania; wybór zatwierdza PPM lub Enter.
    void startSelect(const QString& command, const QString& prompt);
    // Polecenie czeka na punkt: podpowiedź, pola X, Y, Z, OK.
    void startPoint(const QString& command, const QString& prompt);
    void setPrompt(const QString& prompt);

    bool isPicking() const { return m_picking; }

    // Punkt kliknięty w widoku. Gdy jakieś pole pominięto (F1), wpisane pola
    // zastępują współrzędne kliknięcia. Brak wartości = błąd w polu.
    std::optional<gp_Pnt> resolveClick(double x, double y, double z);
    // Klawisz wciśnięty w widoku 3D: cyfry zaczynają wpisywanie, Enter = OK,
    // Tab przechodzi do pól, F1 pomija pole X. Zwraca true, gdy klawisz obsłużono.
    bool handleViewKey(QKeyEvent* e);

signals:
    void pointEntered(double x, double y, double z); // punkt wpisany i zatwierdzony OK
    void selectionDone();                            // OK przy wyborze elementów
    void cancelled();                                // Esc w polu
    void activeChanged(bool pointInput);             // pasek pokazuje pola punktu albo nie

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void onOk();
    void bypass(int field);
    void focusField(int field);
    void setError(int field, bool error);
    bool anyBypassed() const;
    void showPrompt();

    QLabel* m_command = nullptr;
    QLabel* m_prompt = nullptr;
    QWidget* m_fieldsBox = nullptr;
    QWidget* m_selectBox = nullptr; // podpowiedzi przy wyborze: Poprzednie, Zakończ, Wszystko, Warstwy
    std::vector<QLineEdit*> m_fields;
    std::vector<bool> m_bypassed;
    QPushButton* m_ok = nullptr;

    QString m_basePrompt;
    bool m_picking = false;
    bool m_selecting = false;
};

// Liczy wyrażenie z pola (+ - * / i nawiasy, przecinek jak kropka).
// Pusty tekst albo błąd składni = brak wartości.
std::optional<double> evaluateExpression(const QString& text);
