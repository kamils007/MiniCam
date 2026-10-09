#pragma once
#include <string>
#include <TopoDS_Face.hxx>

namespace camcore {

// Podstawowe dane o jednej ścianie bryły – to, co CAM musi wiedzieć,
// żeby np. uznać ścianę za dno kieszeni.
struct FaceInfo
{
    std::string surfaceType; // "płaska", "walcowa", "stożkowa", …
    double area = 0.0;       // pole powierzchni [mm²]

    // Tylko dla ścian płaskich:
    bool isPlanar = false;
    double normal[3] = {0.0, 0.0, 0.0}; // wektor normalny skierowany NA ZEWNĄTRZ bryły
    double z = 0.0;                     // wysokość płaszczyzny (sens ma dla ścian poziomych)
    bool isHorizontal = false;          // normalna równoległa do osi Z
};

FaceInfo describeFace(const TopoDS_Face& face);

} // namespace camcore
