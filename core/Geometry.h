#pragma once
#include <string>

#include "Layers.h"
#include "PartContours.h"

namespace camcore {

// Warstwy użytkownika tworzone przy rozpoznawaniu cech.
inline const char* const kOuterContourLayer = "userKonturZew"; // kontur zewnętrzny (obrys bryły)
inline const char* const kInnerContourLayer = "userKonturWew"; // kontury wewnętrzne (wycięcia na wylot)
inline const Quantity_Color kOuterContourColor(0.10, 0.45, 1.00, Quantity_TOC_sRGB); // niebieski
inline const Quantity_Color kInnerContourColor(0.85, 0.10, 0.85, Quantity_TOC_sRGB); // fioletowy

// Geometria w projekcie: kontur plus jego właściwości. Kolor nie jest
// właściwością geometrii – bierze się z warstwy, do której geometria należy.
struct Geometry
{
    Contour contour;
    std::string layer = kApsLayer; // warstwa, do której należy
    bool visible = true;
};

} // namespace camcore
