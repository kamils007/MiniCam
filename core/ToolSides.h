#pragma once
#include <vector>

#include <TopoDS_Shape.hxx>

#include "Geometry.h"

namespace camcore {

// Gdzie frez idzie względem geometrii: na zewnątrz, do środka albo środkiem.
// Frezowanie współbieżne: narzędzie zawsze po lewej – na zewnątrz CW, do środka CCW.
enum class ToolPlacement { Outside, Inside, Centre };
void setPlacement(Geometry& g, ToolPlacement placement);

// Domyślna strona frezowania po rozpoznaniu cech (jest bryła):
// - obrys bryły (userKonturZew) – zawsze na zewnątrz,
// - otwory o średnicy wiertła (userOtwory) – środkiem (wiercenie),
// - reszta – po stronie, po której przy ściance nie ma materiału (sprawdzamy bryłę
//   w połowie wysokości ścianki tuż obok geometrii); gdy to nic nie rozstrzyga,
//   reguła zagnieżdżenia jak w assignToolSide.
void assignToolSides(std::vector<Geometry>& geometries, const TopoDS_Shape& solid);

// Domyślna strona frezowania bez bryły (np. geometria narysowana ręcznie):
// zamknięta geometria leżąca w parzystej liczbie innych zamkniętych geometrii
// (0, 2 – wyspa w kieszeni…) – na zewnątrz, w nieparzystej – do środka;
// otwarta – środkiem.
void assignToolSide(std::vector<Geometry>& geometries, size_t index);

// Zmiana kierunku i strony wybranych geometrii (okno Technologia → Kierunki narzędzia,
// jak Tool Directions w Alphacam). Najpierw zmienia się strona, potem kierunek –
// zmiana kierunku zostawia narzędzie po tej samej stronie geometrii (np. na zewnątrz).
struct DirectionChange
{
    // Geometrie otwarte.
    enum class OpenDirection { Reverse, NoChange };
    enum class OpenSide { Left, Right, Swap, Centre, NoChange };
    // Geometrie zamknięte. OnLeft/OnRight: kierunek tak, żeby narzędzie było po lewej
    // (frezowanie współbieżne) albo po prawej (przeciwbieżne).
    enum class ClosedDirection { CW, CCW, Reverse, NoChange, OnLeft, OnRight };
    enum class ClosedSide { Outside, Inside, Left, Right, Swap, Centre, NoChange, Automatic };

    OpenDirection openDirection = OpenDirection::NoChange;
    OpenSide openSide = OpenSide::NoChange;
    ClosedDirection closedDirection = ClosedDirection::OnLeft;
    ClosedSide closedSide = ClosedSide::Automatic;
};
// indices – które geometrie zmienić (puste = wszystkie). solid – bryła dla "Automatycznie"
// (może być pusta – wtedy reguła zagnieżdżenia).
void applyDirectionChange(std::vector<Geometry>& geometries, const std::vector<int>& indices,
                          const DirectionChange& change, const TopoDS_Shape& solid);

} // namespace camcore
