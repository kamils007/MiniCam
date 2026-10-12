#pragma once

#include <QCursor>
#include <QString>
#include <QToolBar>

#include <vector>

#include "OccView.h"

class InputBar;
class QStatusBar;
class QToolButton;

// Belka polecenia (druga od dołu, jak w Alphacam): po lewej pasek wprowadzania,
// po prawej uchwyty (Snaps) i filtry – widoczne tylko wtedy, gdy polecenie czeka na punkt.
class CommandBar : public QToolBar
{
    Q_OBJECT

public:
    // shortcutWindow: okno, w którym działają skróty uchwytów (F6, F7, F8).
    explicit CommandBar(QWidget* shortcutWindow, QWidget* parent = nullptr);

    InputBar* inputBar() const { return m_inputBar; }
    void releaseSnap(); // wyłącza aktywny uchwyt

signals:
    // Wybrany uchwyt (None = żaden) i kursor z jego ikonką.
    void snapChanged(OccView::Snap snap, const QCursor& cursor);

private:
    InputBar* m_inputBar = nullptr;
    std::vector<QToolButton*> m_snapButtons;
};

// Stopka (najniżej): przyciski widoków i przełączniki obok komunikatów.
void setupFooter(QStatusBar* statusBar);

// Wygląd obu dolnych belek.
QString bottomBarsStyleSheet();
