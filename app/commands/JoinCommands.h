#pragma once

#include "Command.h"

#include <vector>

// Polecenia z grupy Edycja → Przerwij, połącz itd. (jak w Alphacam): wybór geometrii
// (LPM; bryła się nie liczy), PPM, Enter albo OK wykonuje, Esc anuluje. Ctrl+Z cofa.
class GeometrySelectCommand : public Command
{
    Q_OBJECT

public:
    using Command::Command;

    bool start() override;
    void selectionChanged(int count) override;
    void selectionConfirmed() override;
    bool isSelecting() const override { return true; }

protected:
    // Co polecenie robi z wybranymi geometriami (numery rosnąco); zmianę zapisuje przez
    // host().commitEdit. Zwraca komunikat na koniec polecenia.
    virtual QString apply(const std::vector<int>& geometries) = 0;
    virtual QString hint() const = 0; // podpowiedź na stopce przy wyborze
};

// Rozdziel: każda wybrana geometria rozpada się na osobne elementy (odcinki, łuki).
class ExplodeCommand : public GeometrySelectCommand
{
    Q_OBJECT

public:
    using GeometrySelectCommand::GeometrySelectCommand;
    QString name() const override { return "Rozdziel"; }

protected:
    QString apply(const std::vector<int>& geometries) override;
    QString hint() const override { return "wskaż geometrie do rozdzielenia na elementy"; }
};

// Połącz: elementy wybranych geometrii łączą się koniec do końca w jedną geometrię
// (albo kilka, gdy się nie stykają). Wspólna warstwa zostaje, różne – APS.
class JoinCommand : public GeometrySelectCommand
{
    Q_OBJECT

public:
    using GeometrySelectCommand::GeometrySelectCommand;
    QString name() const override { return "Połącz"; }

protected:
    QString apply(const std::vector<int>& geometries) override;
    QString hint() const override { return "wskaż geometrie do połączenia"; }
};
