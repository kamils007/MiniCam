#include "ToolDirection.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>

#include <vector>

namespace camcore {

namespace {

// Krawędzie konturu po kolei, każda z kierunkiem, w jakim biegnie w konturze.
struct OrientedEdge
{
    TopoDS_Edge edge;
    bool reversed = false;
};

std::vector<OrientedEdge> orderedEdges(const TopoDS_Wire& wire)
{
    std::vector<OrientedEdge> edges;
    for (BRepTools_WireExplorer ex(wire); ex.More(); ex.Next())
        edges.push_back({ex.Current(), ex.Orientation() == TopAbs_REVERSED});
    if (edges.empty()) // np. kontur z niepołączonych elementów
        for (TopExp_Explorer ex(wire, TopAbs_EDGE); ex.More(); ex.Next())
            edges.push_back({TopoDS::Edge(ex.Current()), ex.Current().Orientation() == TopAbs_REVERSED});
    return edges;
}

// Punkty wzdłuż konturu w kolejności jego elementów.
std::vector<gp_Pnt> samplePoints(const std::vector<OrientedEdge>& edges)
{
    std::vector<gp_Pnt> points;
    for (const OrientedEdge& e : edges) {
        const BRepAdaptor_Curve curve(e.edge);
        const int n = curve.GetType() == GeomAbs_Line ? 1 : 32;
        for (int k = 0; k <= n; ++k) {
            const double f = double(e.reversed ? n - k : k) / n;
            points.push_back(curve.Value(curve.FirstParameter() + (curve.LastParameter() - curve.FirstParameter()) * f));
        }
    }
    return points;
}

// Pole ze znakiem w rzucie na XY: > 0 – przeciwnie do wskazówek zegara.
double signedArea(const std::vector<gp_Pnt>& points)
{
    double area = 0;
    for (size_t i = 0; i < points.size(); ++i) {
        const gp_Pnt& a = points[i];
        const gp_Pnt& b = points[(i + 1) % points.size()];
        area += a.X() * b.Y() - b.X() * a.Y();
    }
    return area / 2;
}

} // namespace

bool isClockwise(const TopoDS_Wire& wire)
{
    return signedArea(samplePoints(orderedEdges(wire))) < 0;
}

DirectionMarker directionMarker(const TopoDS_Wire& wire, CutDirection direction, ToolSide side)
{
    DirectionMarker marker;
    if (wire.IsNull())
        return marker;
    const std::vector<OrientedEdge> edges = orderedEdges(wire);
    if (edges.empty())
        return marker;
    const std::vector<gp_Pnt> points = samplePoints(edges);
    const bool closed = points.front().Distance(points.back()) < 1e-6;

    // Czy frez idzie odwrotnie do kolejności elementów konturu.
    bool backwards = direction == CutDirection::CCW;
    if (closed)
        backwards = (direction == CutDirection::CW) != (signedArea(points) < 0);

    // Początek ruchu: początek pierwszego elementu albo koniec ostatniego.
    const OrientedEdge& e = backwards ? edges.back() : edges.front();
    const BRepAdaptor_Curve curve(e.edge);
    // Na krzywej: koniec, z którego frez wchodzi na element, i znak kierunku ruchu.
    const bool atLast = e.reversed != backwards;
    const double t = atLast ? curve.LastParameter() : curve.FirstParameter();
    gp_Pnt p;
    gp_Vec d;
    curve.D1(t, p, d);
    if (atLast)
        d.Reverse();
    d.SetZ(0); // geometria leży w płaszczyźnie XY
    if (d.Magnitude() < 1e-9)
        return marker;

    marker.valid = true;
    marker.point = p;
    marker.travel = gp_Dir(d);
    const gp_Vec left(-marker.travel.Y(), marker.travel.X(), 0);
    marker.side = side == ToolSide::Left ? left : side == ToolSide::Right ? -left : gp_Vec(0, 0, 0);
    return marker;
}

} // namespace camcore
