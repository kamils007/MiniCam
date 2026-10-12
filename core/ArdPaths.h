#pragma once
#include <vector>

#include "ArdExport.h"
#include "Geometry.h"

namespace camcore {

// Geometrie MiniCAM → geometrie do pliku .ard: każda krawędź konturu jako
// odcinek, łuki i krzywe zamienione na krótkie odcinki (rekordów łuku Alphacama
// jeszcze nie znamy). Kolejność odcinków zgodna z kierunkiem geometrii (CW/CCW).
std::vector<ArdPath> ardPaths(const std::vector<Geometry>& geometries, const LayerList& layers);

} // namespace camcore
