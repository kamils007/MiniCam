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
    g.toolDirectionKnown = true;
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

namespace {

// Domyślna strona jednej geometrii (reguły jak w assignToolSides).
void assignAutomatic(std::vector<Geometry>& geometries, size_t i, const TopoDS_Shape& solid)
{
    Geometry& g = geometries[i];
    if (g.layer == kOuterContourLayer) {
        setPlacement(g, ToolPlacement::Outside);
        return;
    }
    if (g.layer == kHoleLayer) {
        setPlacement(g, ToolPlacement::Centre);
        return;
    }
    const ToolPlacement fromSolid = placementFromSolid(g, solid);
    if (fromSolid != ToolPlacement::Centre)
        setPlacement(g, fromSolid);
    else
        assignToolSide(geometries, i);
}

ToolSide swapped(ToolSide side)
{
    return side == ToolSide::Left ? ToolSide::Right : side == ToolSide::Right ? ToolSide::Left : side;
}

CutDirection opposite(CutDirection d) { return d == CutDirection::CW ? CutDirection::CCW : CutDirection::CW; }

// Nowy kierunek, narzędzie zostaje po tej samej stronie geometrii.
void setDirectionKeepingSide(Geometry& g, CutDirection direction)
{
    if (direction == g.direction)
        return;
    g.direction = direction;
    g.toolSide = swapped(g.toolSide);
}

void changeOpen(Geometry& g, const DirectionChange& c)
{
    using Side = DirectionChange::OpenSide;
    switch (c.openSide) {
    case Side::Left: g.toolSide = ToolSide::Left; break;
    case Side::Right: g.toolSide = ToolSide::Right; break;
    case Side::Swap: g.toolSide = swapped(g.toolSide); break;
    case Side::Centre: g.toolSide = ToolSide::Centre; break;
    case Side::NoChange: break;
    }
    if (c.openDirection == DirectionChange::OpenDirection::Reverse)
        setDirectionKeepingSide(g, opposite(g.direction));
}

void changeClosed(std::vector<Geometry>& geometries, size_t i, const DirectionChange& c, const TopoDS_Shape& solid)
{
    Geometry& g = geometries[i];
    using Side = DirectionChange::ClosedSide;
    // Przy CW lewa strona to zewnątrz, przy CCW – wnętrze.
    const ToolSide outside = g.direction == CutDirection::CW ? ToolSide::Left : ToolSide::Right;
    switch (c.closedSide) {
    case Side::Outside: g.toolSide = outside; break;
    case Side::Inside: g.toolSide = swapped(outside); break;
    case Side::Left: g.toolSide = ToolSide::Left; break;
    case Side::Right: g.toolSide = ToolSide::Right; break;
    case Side::Swap: g.toolSide = swapped(g.toolSide); break;
    case Side::Centre: g.toolSide = ToolSide::Centre; break;
    case Side::NoChange: break;
    case Side::Automatic: assignAutomatic(geometries, i, solid); break;
    }
    using Dir = DirectionChange::ClosedDirection;
    switch (c.closedDirection) {
    case Dir::CW: setDirectionKeepingSide(g, CutDirection::CW); break;
    case Dir::CCW: setDirectionKeepingSide(g, CutDirection::CCW); break;
    case Dir::Reverse: setDirectionKeepingSide(g, opposite(g.direction)); break;
    case Dir::NoChange: break;
    case Dir::OnLeft:
        if (g.toolSide == ToolSide::Right)
            setDirectionKeepingSide(g, opposite(g.direction));
        break;
    case Dir::OnRight:
        if (g.toolSide == ToolSide::Left)
            setDirectionKeepingSide(g, opposite(g.direction));
        break;
    }
}

} // namespace

void assignToolSides(std::vector<Geometry>& geometries, const TopoDS_Shape& solid)
{
    for (size_t i = 0; i < geometries.size(); ++i)
        assignAutomatic(geometries, i, solid);
}

void applyDirectionChange(std::vector<Geometry>& geometries, const std::vector<int>& indices,
                          const DirectionChange& change, const TopoDS_Shape& solid)
{
    std::vector<size_t> which;
    if (indices.empty())
        for (size_t i = 0; i < geometries.size(); ++i)
            which.push_back(i);
    else
        for (int i : indices)
            if (i >= 0 && i < static_cast<int>(geometries.size()))
                which.push_back(static_cast<size_t>(i));
    for (size_t i : which) {
        if (isClosed(geometries[i].contour.wire)) {
            changeClosed(geometries, i, change, solid);
            if (change.closedDirection != DirectionChange::ClosedDirection::NoChange
                || change.closedSide != DirectionChange::ClosedSide::NoChange)
                geometries[i].toolDirectionKnown = true;
        } else {
            changeOpen(geometries[i], change);
            if (change.openDirection != DirectionChange::OpenDirection::NoChange
                || change.openSide != DirectionChange::OpenSide::NoChange)
                geometries[i].toolDirectionKnown = true;
        }
    }
}

} // namespace camcore
