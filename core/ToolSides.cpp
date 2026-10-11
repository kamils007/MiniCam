#include "ToolSides.h"

#include <BRepClass3d_SolidClassifier.hxx>
#include <TopExp_Explorer.hxx>

#include <cmath>

namespace camcore {

namespace {

constexpr double kProbe = 0.1; // jak daleko od geometrii sprawdzamy materiał [mm]

// Czy punkt (XY) leży wewnątrz wieloboku.
bool insidePolygon(const std::vector<gp_Pnt>& polygon, double x, double y)
{
    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const gp_Pnt& a = polygon[i];
        const gp_Pnt& b = polygon[j];
        if ((a.Y() > y) != (b.Y() > y) && x < (b.X() - a.X()) * (y - a.Y()) / (b.Y() - a.Y()) + a.X())
            inside = !inside;
    }
    return inside;
}

double area(const std::vector<gp_Pnt>& p)
{
    double a = 0;
    for (size_t i = 0; i < p.size(); ++i)
        a += p[i].X() * p[(i + 1) % p.size()].Y() - p[(i + 1) % p.size()].X() * p[i].Y();
    return std::abs(a / 2);
}

bool inMaterial(const TopoDS_Shape& solid, const gp_Pnt& p)
{
    for (TopExp_Explorer ex(solid, TopAbs_SOLID); ex.More(); ex.Next()) {
        BRepClass3d_SolidClassifier classifier(ex.Current(), p, 1e-6);
        if (classifier.State() == TopAbs_IN)
            return true;
    }
    return false;
}

// Strona z bryły: Outside/Inside, albo Centre, gdy bryła nic nie rozstrzyga.
ToolPlacement placementFromSolid(const Geometry& g, const TopoDS_Shape& solid)
{
    const Contour& c = g.contour;
    gp_Pnt p;
    gp_Dir t;
    if (solid.IsNull() || !isClosed(c.wire) || !contourMidPoint(c.wire, p, t))
        return ToolPlacement::Centre;
    const double z = c.zTop - c.zBottom > 1e-6 ? (c.zTop + c.zBottom) / 2 : c.zTop - kProbe;
    const gp_Vec left(-t.Y(), t.X(), 0);
    const bool leftIn = inMaterial(solid, gp_Pnt(p.X() + left.X() * kProbe, p.Y() + left.Y() * kProbe, z));
    const bool rightIn = inMaterial(solid, gp_Pnt(p.X() - left.X() * kProbe, p.Y() - left.Y() * kProbe, z));
    if (leftIn == rightIn)
        return ToolPlacement::Centre;
    // Po lewej stronie konturu (w kolejności elementów) jest zewnątrz, gdy kontur biegnie CW.
    const bool freeLeft = !leftIn;
    return freeLeft == isClockwise(c.wire) ? ToolPlacement::Outside : ToolPlacement::Inside;
}

} // namespace

void setPlacement(Geometry& g, ToolPlacement placement)
{
    g.direction = placement == ToolPlacement::Inside ? CutDirection::CCW : CutDirection::CW;
    g.toolSide = placement == ToolPlacement::Centre ? ToolSide::Centre : ToolSide::Left;
}

void assignToolSide(std::vector<Geometry>& geometries, size_t index)
{
    Geometry& g = geometries[index];
    if (!isClosed(g.contour.wire)) {
        setPlacement(g, ToolPlacement::Centre);
        return;
    }
    const std::vector<gp_Pnt> own = contourPoints(g.contour.wire);
    const double ownArea = area(own);
    int depth = 0;
    for (size_t i = 0; i < geometries.size(); ++i) {
        if (i == index || !isClosed(geometries[i].contour.wire))
            continue;
        const std::vector<gp_Pnt> other = contourPoints(geometries[i].contour.wire);
        if (area(other) > ownArea && insidePolygon(other, own.front().X(), own.front().Y()))
            ++depth;
    }
    setPlacement(g, depth % 2 == 0 ? ToolPlacement::Outside : ToolPlacement::Inside);
}

void assignToolSides(std::vector<Geometry>& geometries, const TopoDS_Shape& solid)
{
    for (size_t i = 0; i < geometries.size(); ++i) {
        Geometry& g = geometries[i];
        if (g.layer == kOuterContourLayer) {
            setPlacement(g, ToolPlacement::Outside);
            continue;
        }
        if (g.layer == kHoleLayer) {
            setPlacement(g, ToolPlacement::Centre);
            continue;
        }
        const ToolPlacement fromSolid = placementFromSolid(g, solid);
        if (fromSolid != ToolPlacement::Centre)
            setPlacement(g, fromSolid);
        else
            assignToolSide(geometries, i);
    }
}

} // namespace camcore
