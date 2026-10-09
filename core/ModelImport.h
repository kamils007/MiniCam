#pragma once
#include <string>
#include <vector>
#include <Quantity_Color.hxx>
#include <TopoDS_Shape.hxx>

namespace camcore {

// Kolor zapisany w pliku dla fragmentu bryły (ściany, bryły albo całej części).
struct ShapeColor
{
    TopoDS_Shape shape;
    Quantity_Color color;
};

// Wynik wczytania pliku: geometria w ORYGINALNYM położeniu z pliku + kolory.
struct ImportedModel
{
    TopoDS_Shape shape;
    std::vector<ShapeColor> colors; // puste dla BREP albo pliku bez kolorów
};

// Wczytuje bryłę z pliku STEP (.step/.stp), IGES (.iges/.igs) lub BREP (.brep).
// W razie błędu rzuca std::runtime_error.
ImportedModel importModel(const std::string& path);

} // namespace camcore
