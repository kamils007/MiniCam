#pragma once
#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <vector>

namespace camcore {

// Kierunek obróbki geometrii (jak Direction w Alphacam): w którą stronę idzie frez.
// Dla geometrii zamkniętej – zgodnie (CW) albo przeciwnie (CCW) do ruchu wskazówek
// zegara, patrząc z góry. Dla otwartej CW = wzdłuż kolejności elementów, CCW = odwrotnie.
enum class CutDirection { CW, CCW };

// Strona narzędzia względem kierunku ruchu (jak Tool side w Alphacam): po lewej,
// po prawej albo środkiem geometrii. Przy CW lewa strona to zewnątrz konturu,
// przy CCW – jego wnętrze.
enum class ToolSide { Left, Right, Centre };

// Punkty wzdłuż konturu w kolejności jego elementów (łuki i krzywe gęsto), w rzucie
// nadają się na wielobok.
std::vector<gp_Pnt> contourPoints(const TopoDS_Wire& wire);
bool isClosed(const TopoDS_Wire& wire);
// Środek pierwszego elementu konturu i kierunek konturu w tym punkcie (w XY).
bool contourMidPoint(const TopoDS_Wire& wire, gp_Pnt& point, gp_Dir& tangent);

// Czy zamknięty kontur w kolejności swoich elementów biegnie zgodnie z ruchem
// wskazówek zegara (rzut na XY).
bool isClockwise(const TopoDS_Wire& wire);

// Gdzie stoi strzałka kierunku: początek ruchu frezu po geometrii, kierunek ruchu
// w tym punkcie i strona narzędzia (wektor jednostkowy w XY, zerowy dla Centre).
struct DirectionMarker
{
    bool valid = false;
    gp_Pnt point;
    gp_Dir travel;
    gp_Vec side;
};
DirectionMarker directionMarker(const TopoDS_Wire& wire, CutDirection direction, ToolSide side);

} // namespace camcore
