#pragma once

#include "JoinCommands.h"
#include "ToolSides.h"

// Technologia → Kierunki narzędzia → Wybierz geometrie: wybór geometrii (LPM),
// PPM zmienia ich kierunek i stronę według opcji z okna. Ctrl+Z cofa.
class ToolDirectionsCommand : public GeometrySelectCommand
{
    Q_OBJECT

public:
    ToolDirectionsCommand(const camcore::DirectionChange& change, CommandHost& host, QObject* parent = nullptr)
        : GeometrySelectCommand(host, parent), m_change(change)
    {
    }
    QString name() const override { return "Kierunki narzędzia"; }

    // Zmienia geometrie (puste indices = wszystkie) i zapisuje zmianę do cofania.
    static QString applyTo(CommandHost& host, const std::vector<int>& indices, const camcore::DirectionChange& change);

protected:
    QString apply(const std::vector<int>& geometries) override { return applyTo(host(), geometries, m_change); }
    QString hint() const override { return "wskaż geometrie do zmiany kierunku i strony"; }

private:
    camcore::DirectionChange m_change;
};
