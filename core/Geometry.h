#pragma once
#include <string>
#include <Quantity_Color.hxx>

#include "PartContours.h"

namespace camcore {

// Nazwa warstwy na geometrie jeszcze niesklasyfikowane (jak w Alphacam).
inline const char* const kApsLayer = "Geometrie APS";
// Geometrie APS są zawsze zielone, jak w Alphacam.
inline const Quantity_Color kApsColor(0.00, 0.75, 0.00, Quantity_TOC_sRGB);

// Warstwa: nazwa, kolor i widoczność. Geometrie rysują się kolorem swojej warstwy.
struct Layer
{
    std::string name;
    Quantity_Color color = Quantity_Color(0.10, 0.45, 1.00, Quantity_TOC_sRGB);
    bool visible = true;
};

// Geometria w projekcie: kontur plus jego właściwości. Kolor nie jest
// właściwością geometrii – bierze się z warstwy, do której geometria należy.
struct Geometry
{
    Contour contour;
    std::string layer = kApsLayer; // warstwa, do której należy
    bool visible = true;
};

} // namespace camcore
