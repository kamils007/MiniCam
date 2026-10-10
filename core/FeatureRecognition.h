#pragma once
#include <string>
#include <vector>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

namespace camcore {

// Jedna rozpoznana cecha obróbkowa płyty.
struct Feature
{
    enum class Type { Outline, Hole, Pocket, Cutout };
    enum class Side { Top, Bottom, Through }; // skąd obrabiać

    Type type = Type::Pocket;
    Side side = Side::Top;
    double x = 0, y = 0;       // środek (otwór: oś) [mm]
    double sizeX = 0, sizeY = 0; // wymiary w X i Y (otwór: średnica w obu) [mm]
    double depth = 0;          // głębokość od powierzchni, z której obrabiamy [mm]
    double diameter = 0;       // tylko otwory [mm]
    std::vector<TopoDS_Face> faces; // ściany tworzące cechę
    // Geometria 2D do obróbki (jak w Alphacam): kontur leżący w płaszczyźnie XY
    // na wysokości, z której wchodzi narzędzie – obrys detalu, obrys kieszeni,
    // okrąg otworu, obrys wycięcia.
    TopoDS_Shape contour;
};

// Rozpoznaje obrys detalu, otwory, kieszenie i wycięcia przelotowe w płycie.
// Zakłada bryłę już wyrównaną: grubość wzdłuż Z (tak ustawia ją auto-wyrównanie).
std::vector<Feature> recognizeFeatures(const TopoDS_Shape& shape);

} // namespace camcore
