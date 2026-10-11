#pragma once
#include <optional>
#include <vector>

#include <gp_Pnt.hxx>

#include "PartContours.h"

namespace camcore {

// Edycja geometrii (Edycja → Przerwij, połącz itd., jak w Alphacam).

// Rozdziel (Explode): geometria z kilku elementów rozpada się na osobne geometrie –
// każdy odcinek i łuk osobno, z tą samą wysokością ścian. Geometria z jednego
// elementu zostaje bez zmian (jedna pozycja na liście).
std::vector<Contour> explodeContour(const Contour& contour);

// Połącz (Join): elementy wszystkich podanych geometrii łączymy koniec do końca
// (tolerancja tol [mm]) w jak najdłuższe ciągi. Każdy ciąg to jedna geometria –
// zwykle wychodzi jedna, a gdy elementy się nie stykają, kilka.
std::vector<Contour> joinContours(const std::vector<Contour>& contours, double tol = 0.01);

// Utnij (Trim): z geometrii target usuwa kawałek, który wskazano punktem pick – między
// najbliższymi przecięciami z geometriami tnącymi (cutters) po obu stronach punktu.
// Przecięcia liczone w rzucie na XY (wysokości nie mają znaczenia). Otwarta geometria bez
// przecięcia po jednej stronie traci kawałek aż do swojego końca; zamknięta potrzebuje
// co najmniej dwóch przecięć i staje się otwarta. Wynik: 0–2 geometrie z tą samą wysokością
// ścian; nullopt = brak przecięć, nic nie ucięto.
std::optional<std::vector<Contour>> trimContour(const Contour& target, const std::vector<Contour>& cutters,
                                                const gp_Pnt& pick);

// Poziomy Z (3D → Ustaw poziomy Z, jak Z levels w Alphacam): geometria staje na górnym
// poziomie (top), a jej ściany schodzą do dolnego (bottom) – płaska geometria dostaje
// wysokość top − bottom. Poziomy mogą być poniżej 0.
Contour withZLevels(const Contour& contour, double bottom, double top);

} // namespace camcore
