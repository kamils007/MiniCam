#pragma once
#include <cmath>
#include <string>

#include "Layers.h"
#include "PartContours.h"

namespace camcore {

// Warstwy użytkownika tworzone przy rozpoznawaniu cech.
inline const char* const kOuterContourLayer = "userKonturZew";      // kontur zewnętrzny (obrys bryły)
inline const char* const kInnerContourLayer = "userKonturWew";      // na wylot, wewnątrz obrysu
inline const char* const kHoleLayer = "userOtwory";                 // okręgi o średnicach wierteł
inline const char* const kRoundPocketLayer = "userKieszenOkragla";  // pozostałe okręgi (nie na wylot)

// Kolor, z jakim rozpoznawanie tworzy swoją warstwę.
inline Quantity_Color autoLayerColor(const std::string& layer)
{
    if (layer == kOuterContourLayer)
        return Quantity_Color(0.10, 0.45, 1.00, Quantity_TOC_sRGB); // niebieski
    if (layer == kInnerContourLayer)
        return Quantity_Color(0.85, 0.10, 0.85, Quantity_TOC_sRGB); // fioletowy
    if (layer == kHoleLayer)
        return Quantity_Color(0.90, 0.10, 0.10, Quantity_TOC_sRGB); // czerwony
    if (layer == kRoundPocketLayer)
        return Quantity_Color(0.95, 0.55, 0.00, Quantity_TOC_sRGB); // pomarańczowy
    return kApsColor;
}

// Średnice wierteł [mm] – okrąg o takiej średnicy to otwór do wiercenia.
inline bool isDrillDiameter(double d)
{
    for (int drill = 2; drill <= 15; ++drill)
        if (std::abs(d - drill) < 0.01)
            return true;
    return false;
}

// Warstwa dla konturu z rozpoznawania cech:
// - okrąg o średnicy wiertła → otwory (na wylot czy nie),
// - pozostałe na wylot (wewnątrz obrysu) → kontur wewnętrzny,
// - pozostałe okręgi → kieszeń okrągła,
// - reszta na razie niesklasyfikowana → APS.
inline std::string layerForContour(const Contour& c, bool through)
{
    const bool circle = c.diameter > 0;
    if (circle && isDrillDiameter(c.diameter))
        return kHoleLayer;
    if (through)
        return kInnerContourLayer;
    if (circle)
        return kRoundPocketLayer;
    return kApsLayer;
}

// Geometria w projekcie: kontur plus jego właściwości. Kolor nie jest
// właściwością geometrii – bierze się z warstwy, do której geometria należy.
struct Geometry
{
    Contour contour;
    std::string layer = kApsLayer; // warstwa, do której należy
    bool visible = true;
};

} // namespace camcore
