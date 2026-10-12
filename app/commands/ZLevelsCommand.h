#pragma once

#include "Command.h"

#include <QString>

#include <vector>

// 3D → Ustaw poziomy Z (jak Z levels w Alphacam): wybór geometrii (LPM), PPM, Enter albo OK
// otwiera okno "Definiuj poziomy Z" (Góra Z, Dół Z, Różnica). Przycisk "..." przy polu bierze
// Z ze wskazanego punktu (uchwyt albo wpisany w pasek) i wraca do okna. OK kończy polecenie –
// płaska geometria dostaje wysokość: leży na Górze Z, ściany schodzą do Dołu Z.
// Esc anuluje, Ctrl+Z cofa.
class ZLevelsCommand : public Command
{
    Q_OBJECT

public:
    using Command::Command;

    QString name() const override { return "Poziomy Z"; }
    bool start() override;
    void selectionChanged(int count) override;
    void selectionConfirmed() override;
    void pointEntered(const gp_Pnt& p) override;
    void cancel() override;
    bool isSelecting() const override { return m_step == Step::Selecting; }

private:
    void showDialog(); // okno z polami m_top / m_bottom
    void apply(double bottom, double top);

    enum class Step { Selecting, Dialog, PickTop, PickBottom };
    Step m_step = Step::Selecting;
    std::vector<int> m_geometries;
    QString m_top, m_bottom; // tekst pól okna (zostaje między wskazaniami punktu)
};
