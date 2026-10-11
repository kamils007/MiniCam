#pragma once

#include "JoinCommands.h"

// 3D → Ustaw poziomy Z (jak Z levels w Alphacam): wybór geometrii (LPM), PPM, Enter albo OK
// otwiera okno z górnym i dolnym poziomem Z, OK w oknie kończy polecenie – płaska geometria
// dostaje wysokość: leży na górnym poziomie, ściany schodzą do dolnego. Esc anuluje, Ctrl+Z cofa.
class ZLevelsCommand : public GeometrySelectCommand
{
    Q_OBJECT

public:
    using GeometrySelectCommand::GeometrySelectCommand;
    QString name() const override { return "Poziomy Z"; }

protected:
    QString apply(const std::vector<int>& geometries) override;
    QString hint() const override;
};
