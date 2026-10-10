#pragma once
#include <vector>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>

namespace camcore {

// Zamknięty kontur 2D w płaszczyźnie XY. Składa się z geometrii – kolejnych
// krawędzi (odcinki, łuki, krzywe) – i ma wysokość swoich ścian bocznych:
// kontur leży na górze ścian (zTop), ściany schodzą do zBottom.
struct Contour
{
    TopoDS_Wire wire;                  // cały kontur (na wysokości zTop)
    std::vector<TopoDS_Edge> geometry; // jego geometrie po kolei, wzdłuż konturu
    double zTop = 0, zBottom = 0;      // góra i dół ścian [mm]
    double x = 0, y = 0;               // środek obrysu [mm]
    double sizeX = 0, sizeY = 0;       // wymiary obrysu w X i Y [mm]
    double diameter = 0;               // > 0, gdy kontur jest pełnym okręgiem [mm]
};

// Kieszeń (frezowana z góry): dno i kontury, które ją tworzą.
// Pierwszy kontur jest zewnętrzny, kolejne to kontury wewnętrzne (wyspy).
struct Pocket
{
    double zFloor = 0;             // wysokość dna [mm]
    double depth = 0;              // głębokość od góry ścian do dna [mm]
    std::vector<Contour> contours; // [0] = zewnętrzny, dalej wewnętrzne
};

// Wynik rozbioru płyty na kontury.
struct PartContours
{
    Contour outline;               // kontur po obrysie bryły (z rzutu bryły z góry)
    std::vector<Contour> inner;    // pozostałe wycięcia na wylot – kontury wewnętrzne
    std::vector<Pocket> pockets;   // wszystko, co nie przechodzi na wylot
};

// Rozbiera płytę (wyrównaną: grubość wzdłuż Z) na kontury i kieszenie.
// Kontur zewnętrzny to obrys rzutu bryły z góry. Dalej idzie po płaskich
// poziomych powierzchniach (bez wierzchu): spód daje wycięcia na wylot,
// powierzchnie patrzące w górę to dna kieszeni.
// Bierze tylko kontury, których ścianki rosną w Z do góry – kieszenie
// od spodu płyty są pomijane.
PartContours buildPartContours(const TopoDS_Shape& shape);

} // namespace camcore
