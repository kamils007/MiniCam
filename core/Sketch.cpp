#include "Sketch.h"

#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <Bnd_Box.hxx>
#include <gp_Ax2.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Circ.hxx>

#include <cmath>

namespace camcore {

namespace {

constexpr double kTol = 1e-6;
constexpr double kPi = 3.14159265358979323846;

gp_Pnt flat(const gp_Pnt& p) { return gp_Pnt(p.X(), p.Y(), 0.0); }

// Środek okręgu przez trzy punkty (w XY); brak, gdy leżą na jednej prostej.
std::optional<gp_Pnt> circumcentre(const gp_Pnt& a, const gp_Pnt& b, const gp_Pnt& c)
{
    const double d = 2 * (a.X() * (b.Y() - c.Y()) + b.X() * (c.Y() - a.Y()) + c.X() * (a.Y() - b.Y()));
    if (std::abs(d) < kTol)
        return std::nullopt;
    const double a2 = a.X() * a.X() + a.Y() * a.Y();
    const double b2 = b.X() * b.X() + b.Y() * b.Y();
    const double c2 = c.X() * c.X() + c.Y() * c.Y();
    return gp_Pnt((a2 * (b.Y() - c.Y()) + b2 * (c.Y() - a.Y()) + c2 * (a.Y() - b.Y())) / d,
                  (a2 * (c.X() - b.X()) + b2 * (a.X() - c.X()) + c2 * (b.X() - a.X())) / d, 0.0);
}

// Kąt [rad] od centre→a do centre→b przeciwnie do ruchu wskazówek zegara, 0..2π.
double ccwAngle(const gp_Pnt& centre, const gp_Pnt& a, const gp_Pnt& b)
{
    double t = std::atan2(b.Y() - centre.Y(), b.X() - centre.X())
               - std::atan2(a.Y() - centre.Y(), a.X() - centre.X());
    while (t < 0)
        t += 2 * kPi;
    while (t >= 2 * kPi)
        t -= 2 * kPi;
    return t;
}

} // namespace

std::optional<TopoDS_Edge> lineEdge(const gp_Pnt& a, const gp_Pnt& b)
{
    const gp_Pnt p = flat(a), q = flat(b);
    if (p.Distance(q) < kTol)
        return std::nullopt;
    return BRepBuilderAPI_MakeEdge(p, q).Edge();
}

std::optional<TopoDS_Edge> arcEdge(const gp_Pnt& centre, const gp_Pnt& start, double sweep)
{
    const gp_Pnt c = flat(centre), s = flat(start);
    const double r = c.Distance(s);
    if (r < kTol || std::abs(sweep) < 1e-9)
        return std::nullopt;
    if (std::abs(sweep) >= 2 * kPi - 1e-9)
        return circleEdge(c, r);
    // Oś okręgu +Z: parametr rośnie przeciwnie do ruchu wskazówek zegara, −Z: zgodnie.
    // Kierunek X okręgu w punkcie start – łuk to parametry 0..|sweep|.
    const gp_Ax2 axis(c, sweep > 0 ? gp_Dir(0, 0, 1) : gp_Dir(0, 0, -1), gp_Dir(gp_Vec(c, s)));
    return BRepBuilderAPI_MakeEdge(gp_Circ(axis, r), 0.0, std::abs(sweep)).Edge();
}

std::optional<TopoDS_Edge> arcThreePoints(const gp_Pnt& start, const gp_Pnt& second, const gp_Pnt& end)
{
    const gp_Pnt s = flat(start), m = flat(second), e = flat(end);
    const auto c = circumcentre(s, m, e);
    if (!c)
        return std::nullopt;
    // Przeciwnie do ruchu wskazówek, jeśli po drodze do końca mijamy drugi punkt; inaczej zgodnie.
    const double toEnd = ccwAngle(*c, s, e);
    const double toSecond = ccwAngle(*c, s, m);
    return arcEdge(*c, s, toSecond < toEnd ? toEnd : toEnd - 2 * kPi);
}

std::optional<TopoDS_Edge> arcTwoPointsRadius(const gp_Pnt& start, const gp_Pnt& end, double radius)
{
    const gp_Pnt s = flat(start), e = flat(end);
    const double chord = s.Distance(e);
    const double r = std::abs(radius);
    if (chord < kTol || r < chord / 2 - kTol)
        return std::nullopt;
    // Środek na symetralnej cięciwy: po lewej stronie kierunku start→end daje krótszy łuk
    // przeciwnie do ruchu wskazówek, po prawej – dłuższy.
    const double h = std::sqrt(std::max(0.0, r * r - chord * chord / 4)) * (radius > 0 ? 1 : -1);
    const gp_Pnt mid((s.X() + e.X()) / 2, (s.Y() + e.Y()) / 2, 0);
    const double nx = -(e.Y() - s.Y()) / chord, ny = (e.X() - s.X()) / chord;
    const gp_Pnt c(mid.X() + nx * h, mid.Y() + ny * h, 0);
    return arcEdge(c, s, ccwAngle(c, s, e));
}

std::optional<TopoDS_Edge> arcTwoPointsCentre(const gp_Pnt& start, const gp_Pnt& end, const gp_Pnt& centre)
{
    const gp_Pnt s = flat(start), e = flat(end), c = flat(centre);
    if (c.Distance(e) < kTol)
        return std::nullopt;
    // Promień wyznacza początek; koniec leży na kierunku od środka do punktu end.
    double sweep = ccwAngle(c, s, e);
    if (sweep < 1e-9)
        sweep = 2 * kPi; // koniec w kierunku początku – pełny okrąg
    return arcEdge(c, s, sweep);
}

std::optional<TopoDS_Edge> arcStartCentreAngle(const gp_Pnt& start, const gp_Pnt& centre, double angleDeg)
{
    return arcEdge(centre, start, angleDeg * kPi / 180.0);
}

double sweepAngleDeg(const gp_Pnt& centre, const gp_Pnt& start, const gp_Pnt& p)
{
    return ccwAngle(flat(centre), flat(start), flat(p)) * 180.0 / kPi;
}

std::optional<TopoDS_Edge> circleEdge(const gp_Pnt& centre, double radius)
{
    if (radius < kTol)
        return std::nullopt;
    return BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(flat(centre), gp_Dir(0, 0, 1)), radius)).Edge();
}

std::optional<TopoDS_Edge> circleThreePoints(const gp_Pnt& a, const gp_Pnt& b, const gp_Pnt& c)
{
    const auto centre = circumcentre(flat(a), flat(b), flat(c));
    if (!centre)
        return std::nullopt;
    return circleEdge(*centre, centre->Distance(flat(a)));
}

std::vector<TopoDS_Edge> rectangleEdges(const gp_Pnt& a, const gp_Pnt& b)
{
    if (std::abs(a.X() - b.X()) < kTol || std::abs(a.Y() - b.Y()) < kTol)
        return {};
    const gp_Pnt p[4] = {gp_Pnt(a.X(), a.Y(), 0), gp_Pnt(b.X(), a.Y(), 0), gp_Pnt(b.X(), b.Y(), 0),
                         gp_Pnt(a.X(), b.Y(), 0)};
    std::vector<TopoDS_Edge> edges;
    for (int i = 0; i < 4; ++i)
        edges.push_back(*lineEdge(p[i], p[(i + 1) % 4]));
    return edges;
}

Contour contourFromWire(const TopoDS_Wire& wire, double zTop, double zBottom)
{
    Contour c;
    if (wire.IsNull())
        return c;
    c.wire = wire;
    c.zTop = zTop;
    c.zBottom = zBottom;
    // Geometrie po kolei wzdłuż konturu (WireExplorer); gdy się nie da – w kolejności z wire.
    for (BRepTools_WireExplorer ex(wire); ex.More(); ex.Next())
        c.geometry.push_back(ex.Current());
    if (c.geometry.empty())
        for (TopExp_Explorer ex(wire, TopAbs_EDGE); ex.More(); ex.Next())
            c.geometry.push_back(TopoDS::Edge(ex.Current()));
    Bnd_Box box;
    BRepBndLib::Add(wire, box);
    box.SetGap(0.0);
    double x0, y0, z0, x1, y1, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
    c.x = (x0 + x1) / 2;
    c.y = (y0 + y1) / 2;
    c.sizeX = x1 - x0;
    c.sizeY = y1 - y0;
    if (c.geometry.size() == 1) {
        const BRepAdaptor_Curve curve(c.geometry.front());
        if (curve.GetType() == GeomAbs_Circle && curve.IsClosed())
            c.diameter = 2 * curve.Circle().Radius();
    }
    return c;
}

Contour sketchContour(const std::vector<TopoDS_Edge>& edges)
{
    if (edges.empty())
        return {};
    BRepBuilderAPI_MakeWire wire;
    for (const TopoDS_Edge& e : edges)
        wire.Add(e);
    return contourFromWire(wire.Wire(), 0.0, 0.0);
}

} // namespace camcore
