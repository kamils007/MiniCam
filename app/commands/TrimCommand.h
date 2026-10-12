#pragma once

#include "Command.h"

#include <vector>

// Utnij (Edycja → Przerwij, połącz itd., jak Trim w Alphacam): najpierw geometrie tnące
// (LPM, PPM zatwierdza), potem geometrie do ucięcia – klika się je w kawałku, który ma
// zniknąć; PPM ucina. Kawałek znika między najbliższymi przecięciami z geometriami
// tnącymi (albo do końca otwartej geometrii). Esc anuluje, Ctrl+Z cofa.
class TrimCommand : public Command
{
    Q_OBJECT

public:
    using Command::Command;

    QString name() const override { return "Utnij"; }
    bool start() override;
    void selectionChanged(int count) override;
    void selectionConfirmed() override;
    void cancel() override;
    bool isSelecting() const override { return true; }

private:
    void done(const QString& message);

    enum class Step { Cutters, Targets };
    Step m_step = Step::Cutters;
    std::vector<int> m_cutters;
};
