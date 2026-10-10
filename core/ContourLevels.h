#pragma once
#include <vector>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>

namespace camcore {

// Jeden zamknięty kontur 2D leżący w płaszczyźnie XY na wysokości swoich ścian.
struct LevelContour
{
    TopoDS_Wire wire;
    double x = 0, y = 0;         // środek obrysu [mm]
    double sizeX = 0, sizeY = 0; // wymiary obrysu w X i Y [mm]
    double diameter = 0;         // > 0, gdy kontur jest pełnym okręgiem [mm]
    bool inner = false;          // leży wewnątrz innego konturu tego poziomu (otwór, wnęka)
};

// Kontury leżące na tej samej wysokości Z.
struct ContourLevel
{
    double z = 0;
    std::vector<LevelContour> contours;
};

// Idzie od dołu do góry po poziomych powierzchniach bryły (bez wierzchu)
// i z ich brzegów buduje kontury. Każdy kontur trafia na wysokość najwyższej
// ze swoich ścian bocznych; powtórzone kontury są usuwane.
// Wynik pogrupowany po wysokości, rosnąco po Z.
std::vector<ContourLevel> buildContourLevels(const TopoDS_Shape& shape);

} // namespace camcore
