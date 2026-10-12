#include "ArdPaths.h"

#include <BRepAdaptor_Curve.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <TopAbs_Orientation.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <utility>

namespace camcore {

namespace {

constexpr double kDeflection = 0.01; // dokładność zamiany krzywych na odcinki [mm], jak w DXF

// Punkty krawędzi w kierunku przejścia po konturze (z uwzględnieniem orientacji krawędzi).
std::vector<gp_Pnt> edgePoints(const TopoDS_Edge& edge)
{
    std::vector<gp_Pnt> points;
    const BRepAdaptor_Curve curve(edge);
    const double first = curve.FirstParameter(), last = curve.LastParameter();
    if (curve.GetType() == GeomAbs_Line) {
        points = {curve.Value(first), curve.Value(last)};
    } else {
        GCPnts_QuasiUniformDeflection sampler(curve, kDeflection, first, last);
        if (sampler.IsDone())
            for (int i = 1; i <= sampler.NbPoints(); ++i)
                points.push_back(sampler.Value(i));
        else
            points = {curve.Value(first), curve.Value(last)};
    }
    if (edge.Orientation() == TopAbs_REVERSED)
        std::reverse(points.begin(), points.end());
    return points;
}

} // namespace

std::vector<ArdPath> ardPaths(const std::vector<Geometry>& geometries, const LayerList& layers)
{
    std::vector<ArdPath> paths;
    for (const Geometry& g : geometries) {
        ArdPath path;
        path.layerId = ardLayerId(layers, g.layer);
        path.zBottom = g.contour.zBottom;
        path.zTop = g.contour.zTop;
        for (const TopoDS_Edge& edge : g.contour.geometry) {
            const std::vector<gp_Pnt> points = edgePoints(edge);
            for (size_t i = 1; i < points.size(); ++i)
                if (points[i - 1].Distance(points[i]) > 1e-9)
                    path.segments.push_back({points[i - 1].X(), points[i - 1].Y(), points[i].X(), points[i].Y()});
        }
        if (path.segments.empty())
            continue;

        // Kierunek: pole ze znakiem > 0 to CCW (patrząc z góry). Odwracamy, gdy nie zgadza się z geometrią.
        double area = 0;
        for (const ArdSegment& s : path.segments)
            area += s.x1 * s.y2 - s.x2 * s.y1;
        if ((area > 0) != (g.direction == CutDirection::CCW)) {
            std::reverse(path.segments.begin(), path.segments.end());
            for (ArdSegment& s : path.segments)
                s = {s.x2, s.y2, s.x1, s.y1};
        }
        paths.push_back(std::move(path));
    }
    return paths;
}

} // namespace camcore
