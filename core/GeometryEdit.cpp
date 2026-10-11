#include "GeometryEdit.h"

#include "Sketch.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <GeomAPI.hxx>
#include <Geom2dAPI_InterCurveCurve.hxx>
#include <Geom2dAPI_ProjectPointOnCurve.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <Geom_Curve.hxx>
#include <gp_Pln.hxx>
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

namespace {

// Element geometrii w kolejności wzdłuż konturu: krzywa 3D, jej zakres i kierunek,
// plus kopia 2D (rzut na XY) do liczenia przecięć.
struct TrimEdge
{
    Handle(Geom_Curve) curve;
    double first = 0, last = 0;
    bool reversed = false;              // kontur idzie od last do first
    Handle(Geom2d_TrimmedCurve) curve2d;
};

std::vector<TrimEdge> trimEdges(const Contour& c)
{
    std::vector<TrimEdge> out;
    std::vector<TopoDS_Edge> edges;
    for (BRepTools_WireExplorer ex(c.wire); ex.More(); ex.Next())
        edges.push_back(ex.Current());
    if (edges.empty())
        for (TopExp_Explorer ex(c.wire, TopAbs_EDGE); ex.More(); ex.Next())
            edges.push_back(TopoDS::Edge(ex.Current()));
    for (const TopoDS_Edge& e : edges) {
        TrimEdge t;
        t.curve = BRep_Tool::Curve(e, t.first, t.last);
        if (t.curve.IsNull())
            continue;
        t.reversed = e.Orientation() == TopAbs_REVERSED;
        // Rzut na płaszczyznę, w której leży element (kontury są poziome) – parametry te same co w 3D.
        const gp_Pln plane(gp_Pnt(0, 0, t.curve->Value(t.first).Z()), gp::DZ());
        t.curve2d = new Geom2d_TrimmedCurve(GeomAPI::To2d(t.curve, plane), t.first, t.last);
        out.push_back(t);
    }
    return out;
}

// Położenie na konturze: numer elementu + część jego długości (0..1) w kierunku konturu.
struct ContourPos
{
    int edge = 0;
    double s = 0;
    double key() const { return edge + s; }
};

double toS(const TrimEdge& e, double u)
{
    const double s = (u - e.first) / (e.last - e.first);
    return e.reversed ? 1.0 - s : s;
}

double toU(const TrimEdge& e, double s)
{
    return e.first + (e.reversed ? 1.0 - s : s) * (e.last - e.first);
}

// Kawałek konturu od a do b (a przed b; przy zamkniętym b może być "za końcem" – zawija).
std::optional<Contour> piece(const std::vector<TrimEdge>& edges, double a, double b, const Contour& src)
{
    const int n = static_cast<int>(edges.size());
    BRepBuilderAPI_MakeWire wire;
    bool any = false;
    for (int k = static_cast<int>(std::floor(a)); k < b - 1e-9; ++k) {
        const TrimEdge& e = edges[static_cast<size_t>(k % n)];
        const double s0 = std::max(a - k, 0.0), s1 = std::min(b - k, 1.0);
        if (s1 - s0 < 1e-9)
            continue;
        double u0 = toU(e, s0), u1 = toU(e, s1);
        if (u0 > u1)
            std::swap(u0, u1);
        BRepBuilderAPI_MakeEdge make(e.curve, u0, u1);
        if (!make.IsDone())
            continue;
        TopoDS_Edge edge = make.Edge();
        if (e.reversed)
            edge.Reverse();
        wire.Add(edge);
        any = true;
    }
    if (!any || !wire.IsDone())
        return std::nullopt;
    return contourFromWire(wire.Wire(), src.zTop, src.zBottom);
}

} // namespace

std::optional<std::vector<Contour>> trimContour(const Contour& target, const std::vector<Contour>& cutters,
                                                const gp_Pnt& pick)
{
    const std::vector<TrimEdge> edges = trimEdges(target);
    if (edges.empty())
        return std::nullopt;
    const int n = static_cast<int>(edges.size());
    const bool closed = target.wire.Closed() || BRep_Tool::IsClosed(target.wire);

    // Przecięcia z geometriami tnącymi (położenia wzdłuż konturu).
    std::vector<double> cuts;
    for (const Contour& c : cutters)
        for (const TrimEdge& ce : trimEdges(c))
            for (int k = 0; k < n; ++k) {
                Geom2dAPI_InterCurveCurve inter(edges[static_cast<size_t>(k)].curve2d, ce.curve2d, 1e-6);
                for (int i = 1; i <= inter.NbPoints(); ++i) {
                    const double u = inter.Intersector().Point(i).ParamOnFirst();
                    cuts.push_back(k + std::clamp(toS(edges[static_cast<size_t>(k)], u), 0.0, 1.0));
                }
            }
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end(), [](double x, double y) { return y - x < 1e-7; }), cuts.end());
    // Na otwartej geometrii przecięcie w samym końcu niczego nie dzieli.
    if (!closed)
        cuts.erase(std::remove_if(cuts.begin(), cuts.end(), [n](double x) { return x < 1e-7 || x > n - 1e-7; }),
                   cuts.end());
    if (cuts.empty() || (closed && cuts.size() < 2))
        return std::nullopt;

    // Wskazany punkt: najbliższe miejsce na konturze (w rzucie na XY).
    const gp_Pnt2d p(pick.X(), pick.Y());
    double best = 1e300, at = 0;
    for (int k = 0; k < n; ++k) {
        const TrimEdge& e = edges[static_cast<size_t>(k)];
        Geom2dAPI_ProjectPointOnCurve proj(p, e.curve2d);
        for (int i = 1; i <= proj.NbPoints(); ++i)
            if (proj.Distance(i) < best) {
                best = proj.Distance(i);
                at = k + toS(e, proj.Parameter(i));
            }
        for (double u : {e.first, e.last}) {
            const double d = p.Distance(e.curve2d->Value(u));
            if (d < best) {
                best = d;
                at = k + toS(e, u);
            }
        }
    }

    // Przecięcia tuż przed i tuż za wskazanym miejscem.
    const auto after = std::upper_bound(cuts.begin(), cuts.end(), at);
    std::vector<Contour> result;
    if (closed) {
        // Zostaje reszta: od przecięcia za punktem dookoła do przecięcia przed nim.
        const double from = after == cuts.end() ? cuts.front() : *after;
        double to = after == cuts.begin() ? cuts.back() : *(after - 1);
        if (to <= from)
            to += n;
        if (auto c = piece(edges, from, to, target))
            result.push_back(*c);
    } else {
        if (after != cuts.begin())
            if (auto c = piece(edges, 0.0, *(after - 1), target))
                result.push_back(*c);
        if (after != cuts.end())
            if (auto c = piece(edges, *after, n, target))
                result.push_back(*c);
    }
    return result;
}

Contour withZLevels(const Contour& contour, double bottom, double top)
{
    Contour c = translated(contour, gp_Vec(0, 0, top - contour.zTop));
    c.zTop = top;
    c.zBottom = bottom;
    return c;
}

} // namespace camcore
