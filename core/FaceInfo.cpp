#include "FaceInfo.h"

#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_Orientation.hxx>
#include <gp_Pln.hxx>

#include <cmath>

namespace {

std::string surfaceTypeName(GeomAbs_SurfaceType type)
{
    switch (type) {
    case GeomAbs_Plane:              return "płaska";
    case GeomAbs_Cylinder:           return "walcowa";
    case GeomAbs_Cone:               return "stożkowa";
    case GeomAbs_Sphere:             return "kulista";
    case GeomAbs_Torus:              return "torusowa";
    case GeomAbs_BezierSurface:      return "Béziera";
    case GeomAbs_BSplineSurface:     return "B-spline";
    case GeomAbs_SurfaceOfRevolution: return "obrotowa";
    case GeomAbs_SurfaceOfExtrusion: return "wyciągnięta";
    case GeomAbs_OffsetSurface:      return "odsunięta";
    default:                         return "inna";
    }
}

} // namespace

namespace camcore {

FaceInfo describeFace(const TopoDS_Face& face)
{
    FaceInfo info;

    // Adapter daje jednolity dostęp do geometrii ściany, niezależnie od tego,
    // jak dokładnie jest zapisana w pliku.
    BRepAdaptor_Surface surface(face);
    info.surfaceType = surfaceTypeName(surface.GetType());

    // Pole liczymy całkując po powierzchni ściany (z uwzględnieniem otworów).
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    info.area = props.Mass();

    if (surface.GetType() == GeomAbs_Plane) {
        const gp_Pln plane = surface.Plane();
        gp_Dir n = plane.Axis().Direction();
        // Ściana "odwrócona" względem swojej płaszczyzny – wtedy normalna
        // płaszczyzny patrzy w głąb bryły, więc ją obracamy.
        if (face.Orientation() == TopAbs_REVERSED)
            n.Reverse();

        info.isPlanar = true;
        info.normal[0] = n.X() + 0.0; // +0.0 zamienia "-0" na "0"
        info.normal[1] = n.Y() + 0.0;
        info.normal[2] = n.Z() + 0.0;
        info.z = plane.Location().Z();
        info.isHorizontal = std::abs(std::abs(n.Z()) - 1.0) < 1e-6;
    }
    return info;
}

} // namespace camcore
