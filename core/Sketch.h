#pragma once
#include <optional>
#include <vector>

#include <TopoDS_Edge.hxx>
#include <gp_Pnt.hxx>

#include "PartContours.h"

namespace camcore {

// Rysowanie geometrii 2D (zakładka Geometria, jak w Alphacam): odcinki, łuki, okręgi
// i prostokąty w płaszczyźnie rysowania Z = 0. Punkty z innym Z są rzutowane na Z = 0.
// Brak wyniku (nullopt) = z tych punktów nie da się zbudować elementu
// (np. punkty się pokrywają albo leżą na jednej prostej).

// Odcinek od a do b.
std::optional<TopoDS_Edge> lineEdge(const gp_Pnt& a, const gp_Pnt& b);

// Łuk o środku centre, zaczynający się w start, o kącie sweep [rad]:
// dodatni – przeciwnie do ruchu wskazówek zegara, ujemny – zgodnie z nimi.
std::optional<TopoDS_Edge> arcEdge(const gp_Pnt& centre, const gp_Pnt& start, double sweep);

// Łuk – Początek + Drugi + Koniec: przez trzy punkty, od start do end przez second.
std::optional<TopoDS_Edge> arcThreePoints(const gp_Pnt& start, const gp_Pnt& second, const gp_Pnt& end);
// Łuk – 2 punkty + promień: przeciwnie do ruchu wskazówek zegara od start do end.
// Promień dodatni = krótszy łuk (do 180°), ujemny = dłuższy.
std::optional<TopoDS_Edge> arcTwoPointsRadius(const gp_Pnt& start, const gp_Pnt& end, double radius);
// Łuk – 2 punkty + środek: przeciwnie do ruchu wskazówek zegara od start do kierunku end.
std::optional<TopoDS_Edge> arcTwoPointsCentre(const gp_Pnt& start, const gp_Pnt& end, const gp_Pnt& centre);
// Łuk – Początek + środek + kąt rozwarcia [°]: dodatni przeciwnie do ruchu wskazówek, ujemny zgodnie.
std::optional<TopoDS_Edge> arcStartCentreAngle(const gp_Pnt& start, const gp_Pnt& centre, double angleDeg);
// Kąt [°] od kierunku centre→start do centre→p, przeciwnie do ruchu wskazówek zegara, 0..360.
double sweepAngleDeg(const gp_Pnt& centre, const gp_Pnt& start, const gp_Pnt& p);

// Okrąg o środku centre i promieniu radius.
std::optional<TopoDS_Edge> circleEdge(const gp_Pnt& centre, double radius);
// Okrąg przez trzy punkty.
std::optional<TopoDS_Edge> circleThreePoints(const gp_Pnt& a, const gp_Pnt& b, const gp_Pnt& c);

// Prostokąt o przeciwległych narożnikach a i b: cztery odcinki.
std::vector<TopoDS_Edge> rectangleEdges(const gp_Pnt& a, const gp_Pnt& b);

// Geometria z kolejnych elementów (odcinki, łuki) – kontur bez wysokości ścian
// na Z = 0, z wymiarami obrysu i średnicą, gdy to pełny okrąg.
Contour sketchContour(const std::vector<TopoDS_Edge>& edges);

} // namespace camcore
