#pragma once
#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

namespace camcore {

// Kierunek obróbki geometrii (jak Direction w Alphacam): w którą stronę idzie frez.
// Dla geometrii zamkniętej – zgodnie (CW) albo przeciwnie (CCW) do ruchu wskazówek
// zegara, patrząc z góry. Dla otwartej CW = wzdłuż kolejności elementów, CCW = odwrotnie.
enum class CutDirection { CW, CCW };

// Strona narzędzia względem kierunku ruchu (jak Tool side w Alphacam): po lewej,
// po prawej albo środkiem geometrii. Przy CW lewa strona to zewnątrz konturu,
// przy CCW – prawa.
enum class ToolSide { Left, Right, Centre };

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
