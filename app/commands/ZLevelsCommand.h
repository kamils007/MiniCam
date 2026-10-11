#pragma once

#include "JoinCommands.h"

// 3D → Ustaw poziomy Z (jak Z levels w Alphacam): okno z dolnym i górnym poziomem Z,
// potem wybór geometrii (LPM), PPM, Enter albo OK zatwierdza – płaska geometria dostaje
// wysokość: leży na górnym poziomie, ściany schodzą do dolnego. Esc anuluje, Ctrl+Z cofa.
class ZLevelsCommand : public GeometrySelectCommand
{
    Q_OBJECT

public:
    using GeometrySelectCommand::GeometrySelectCommand;
    QString name() const override { return "Poziomy Z"; }
    bool start() override;

protected:
    QString apply(const std::vector<int>& geometries) override;
    QString hint() const override;

private:
    double m_bottom = 0, m_top = 0;
};
