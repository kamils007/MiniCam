#pragma once
#include <string>
#include <Quantity_Color.hxx>

#include "PartContours.h"

namespace camcore {

// Nazwa warstwy na geometrie jeszcze niesklasyfikowane (jak w Alphacam).
inline const char* const kApsLayer = "Geometrie APS";

// Geometria w projekcie: kontur plus jego właściwości. Właściwości zmienia
// użytkownik (checkbox widoczności, później kolor, warstwa), a widok 3D
// rysuje geometrię według nich.
struct Geometry
{
    Contour contour;
    std::string layer = kApsLayer; // warstwa, do której należy
    Quantity_Color color = Quantity_Color(0.10, 0.45, 1.00, Quantity_TOC_sRGB);
    bool visible = true;
};

} // namespace camcore
