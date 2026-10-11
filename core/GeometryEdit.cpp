#include "GeometryEdit.h"

#include "Sketch.h"

#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Tool.hxx>
#include <ShapeAnalysis_FreeBounds.hxx>
#include <ShapeFix_Wire.hxx>
#include <TColStd_HSequenceOfTransient.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_HSequenceOfShape.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <cmath>

namespace camcore {

namespace {

// Elementy geometrii (gdy lista geometry jest pusta – krawędzie z wire).
std::vector<TopoDS_Edge> edgesOf(const Contour& c)
{
    if (!c.geometry.empty())
        return c.geometry;
    std::vector<TopoDS_Edge> edges;
    for (TopExp_Explorer ex(c.wire, TopAbs_EDGE); ex.More(); ex.Next())
        edges.push_back(TopoDS::Edge(ex.Current()));
    return edges;
}

} // namespace

std::vector<Contour> explodeContour(const Contour& contour)
{
    const std::vector<TopoDS_Edge> edges = edgesOf(contour);
    if (edges.size() <= 1)
        return {contour};
    std::vector<Contour> pieces;
    for (const TopoDS_Edge& e : edges)
        pieces.push_back(contourFromWire(BRepBuilderAPI_MakeWire(e).Wire(), contour.zTop, contour.zBottom));
    return pieces;
}

std::vector<Contour> joinContours(const std::vector<Contour>& contours, double tol)
{
    Handle(TopTools_HSequenceOfShape) edges = new TopTools_HSequenceOfShape;
    double zTop = 0, zBottom = 0;
    bool sameWalls = true;
    for (size_t i = 0; i < contours.size(); ++i) {
        const Contour& c = contours[i];
        if (i == 0) {
            zTop = c.zTop;
            zBottom = c.zBottom;
        } else if (std::abs(c.zTop - zTop) > tol || std::abs(c.zBottom - zBottom) > tol) {
            sameWalls = false;
        }
        for (const TopoDS_Edge& e : edgesOf(c))
            edges->Append(e);
    }
    if (edges->IsEmpty())
        return {};
    // OCCT sam układa krawędzie w ciągi koniec do końca (odwraca je, gdy trzeba).
    Handle(TopTools_HSequenceOfShape) wires;
    ShapeAnalysis_FreeBounds::ConnectEdgesToWires(edges, tol, Standard_False, wires);
    std::vector<Contour> joined;
    for (int i = 1; i <= wires->Length(); ++i) {
        TopoDS_Wire wire = TopoDS::Wire(wires->Value(i));
        // Końce stykające się w tolerancji – wspólne wierzchołki, kontur zamknięty, gdy trzeba.
        ShapeFix_Wire fix;
        fix.Load(wire);
        fix.SetPrecision(tol);
        TopoDS_Vertex first, last;
        TopExp::Vertices(wire, first, last);
        fix.ClosedWireMode() = !first.IsNull() && !last.IsNull()
                               && BRep_Tool::Pnt(first).Distance(BRep_Tool::Pnt(last)) < tol;
        fix.FixReorder();
        fix.FixConnected(tol);
        if (!fix.Wire().IsNull())
            wire = fix.Wire();
        // Różne wysokości ścian – połączona geometria ich nie ma (sam kontur).
        joined.push_back(contourFromWire(wire, zTop, sameWalls ? zBottom : zTop));
    }
    return joined;
}

} // namespace camcore
