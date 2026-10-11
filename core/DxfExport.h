#pragma once
#include <string>
#include <vector>

#include "Geometry.h"
#include "Layers.h"

namespace camcore {

// Zapisuje geometrie 2D do tekstu pliku DXF (R12, ASCII) – jak Output CAD w Alphacam:
// odcinki → LINE, łuki → ARC, pełne okręgi → CIRCLE, pozostałe krzywe → ciąg
// krótkich odcinków. Każdy element leży na swojej wysokości Z. Warstwy projektu
// stają się warstwami DXF (z kolorem najbliższym kolorowi warstwy); geometrie APS
// trafiają na warstwę "0". Pomija geometrie ukryte i te na ukrytych warstwach.
// Zwraca zawartość pliku – zapis na dysk robi program (ścieżki z polskimi znakami).
std::string writeDxf(const std::vector<Geometry>& geometries, const LayerList& layers);

// Nazwa warstwy w DXF: APS → "0", reszta bez polskich znaków i spacji.
std::string dxfLayerName(const std::string& layer);

} // namespace camcore
