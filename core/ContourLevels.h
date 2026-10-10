#pragma once
#include <vector>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>

namespace camcore {

// Jeden zamknięty kontur 2D leżący w płaszczyźnie XY na wysokości poziomu.
struct LevelContour
{
    TopoDS_Wire wire;
    double x = 0, y = 0;         // środek obrysu [mm]
    double sizeX = 0, sizeY = 0; // wymiary obrysu w X i Y [mm]
    double diameter = 0;         // > 0, gdy kontur jest pełnym okręgiem [mm]
    bool inner = false;          // leży wewnątrz innego konturu tego poziomu (otwór, wnęka)
};

// Poziom = wszystkie płaskie poziome powierzchnie bryły na tej samej wysokości Z,
// patrzące w tę samą stronę. Ich wspólny brzeg to kontury poziomu.
struct ContourLevel
{
    double z = 0;
    bool facingUp = true; // powierzchnia widoczna z góry (dno kieszeni, wierzch)
                          // czy od spodu (spód płyty, sufit kieszeni od spodu)
    std::vector<LevelContour> contours;
};

// Idzie od dołu do góry po poziomych powierzchniach bryły i na każdym
// poziomie buduje kontury. Wynik posortowany rosnąco po Z.
std::vector<ContourLevel> buildContourLevels(const TopoDS_Shape& shape);

} // namespace camcore
