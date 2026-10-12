#pragma once

#include <QDialog>

#include "ToolSides.h"

class QButtonGroup;

// Okno Technologia → Kierunki narzędzia (układ jak Tool Directions w Alphacam):
// kierunek i strona osobno dla geometrii otwartych i zamkniętych. "Wszystkie geometrie"
// zmienia od razu wszystkie, "Wybierz geometrie" – potem wybór w widoku (LPM, PPM).
// Wybrane opcje zapamiętują się między uruchomieniami.
class ToolDirectionsDialog : public QDialog
{
    Q_OBJECT

public:
    enum Result { AllGeometries = QDialog::Accepted + 1, SelectGeometries };

    explicit ToolDirectionsDialog(QWidget* parent = nullptr);
    camcore::DirectionChange change() const;

private:
    void save() const;

    QButtonGroup* m_openDirection = nullptr;
    QButtonGroup* m_openSide = nullptr;
    QButtonGroup* m_closedDirection = nullptr;
    QButtonGroup* m_closedSide = nullptr;
};
