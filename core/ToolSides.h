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

} // namespace camcore
