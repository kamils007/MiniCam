#include "ModelAlign.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <Bnd_OBB.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <gp_Ax3.hxx>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

// Orientacja płyty: oś grubości (ma trafić w Z) i kierunek najdłuższej krawędzi.
struct PanelFrame
{
    bool valid = false;
    gp_Dir thickness; // kierunek, który po obrocie będzie osią -Z (spód płyty)
    gp_Dir length;    // kierunek najdłuższej krawędzi
};

// Normalna płaskiej ściany skierowana NA ZEWNĄTRZ bryły.
gp_Dir outwardNormal(const TopoDS_Face& face)
{
    gp_Dir n = BRepAdaptor_Surface(face).Plane().Axis().Direction();
    if (face.Orientation() == TopAbs_REVERSED)
        n.Reverse();
    return n;
}

// Wariant "płyta": największa płaska ściana to spód lub wierzch płyty.
PanelFrame frameFromLargestFace(const TopoDS_Shape& shape)
{
    PanelFrame frame;
    double bestArea = 0.0;
    TopoDS_Face bestFace;
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        const TopoDS_Face face = TopoDS::Face(ex.Current());
        if (BRepAdaptor_Surface(face).GetType() != GeomAbs_Plane)
            continue;
        GProp_GProps props;
        BRepGProp::SurfaceProperties(face, props);
        const double area = props.Mass();
        // Przy równych polach (np. gładka płyta: wierzch = spód) wybieramy ścianę,
        // która już patrzy w dół – wtedy płyta nie jest niepotrzebnie odwracana.
        const bool tie = !bestFace.IsNull() && std::abs(area - bestArea) <= 1e-6 * std::max(area, bestArea);
        if ((!tie && area > bestArea) || (tie && outwardNormal(face).Z() < outwardNormal(bestFace).Z())) {
            bestArea = area;
            bestFace = face;
        }
    }
    if (bestFace.IsNull())
        return frame; // brak płaskich ścian – nie obracamy

    // Kładziemy tę ścianę na stole, czyli jej normalna ma patrzeć w dół (-Z).
    gp_Dir normal = outwardNormal(bestFace);
    // Bryła już leży płasko (największa ściana pozioma)? Wtedy NIE odwracamy jej
    // na drugą stronę – zostaje spodem tam, gdzie była; najwyżej obrót wokół Z.
    if (std::abs(normal.Z()) > 1.0 - 1e-6)
        normal = -gp::DZ();

    // Najdłuższa prosta krawędź tej ściany wyznacza kierunek długości.
    double bestLen = 0.0;
    for (TopExp_Explorer ex(bestFace, TopAbs_EDGE); ex.More(); ex.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(ex.Current()));
        if (curve.GetType() != GeomAbs_Line)
            continue;
        const double len = curve.Value(curve.FirstParameter())
                               .Distance(curve.Value(curve.LastParameter()));
        if (len > bestLen) {
            bestLen = len;
            frame.length = curve.Line().Direction();
        }
    }
    if (bestLen <= 0.0)
        return frame;

    frame.thickness = normal;
    frame.valid = true;
    return frame;
}

// Wariant "minimalny prostopadłościan": najmniejsze pudełko (dowolnie obrócone),
// w które mieści się bryła. Najkrótszy bok = grubość, najdłuższy = długość.
PanelFrame frameFromMinimalBox(const TopoDS_Shape& shape)
{
    PanelFrame frame;
    Bnd_OBB obb;
    BRepBndLib::AddOBB(shape, obb, Standard_False, Standard_True, Standard_False);
    if (obb.IsVoid())
        return frame;

    const std::array<std::pair<double, gp_Dir>, 3> axes = {{
        {obb.XHSize(), gp_Dir(obb.XDirection())},
        {obb.YHSize(), gp_Dir(obb.YDirection())},
        {obb.ZHSize(), gp_Dir(obb.ZDirection())},
    }};
    int iMin = 0, iMax = 0;
    for (int i = 1; i < 3; ++i) {
        if (axes[i].first < axes[iMin].first) iMin = i;
        if (axes[i].first > axes[iMax].first) iMax = i;
    }
    if (iMin == iMax)
        return frame; // sześcian – każda orientacja tak samo dobra

    // Zwrot osi grubości wybieramy tak, żeby jak najmniej obracać bryłę.
    frame.thickness = axes[iMin].second;
    if (frame.thickness.Z() > 0)
        frame.thickness.Reverse();
    frame.length = axes[iMax].second;
    frame.valid = true;
    return frame;
}

// Obrót, który ustawia płytę: grubość w osi Z, długość wzdłuż wybranej osi.
gp_Trsf panelRotation(const PanelFrame& frame, camcore::AlignSettings::LongEdge longEdge)
{
    // Lokalny układ płyty: Z = do góry (przeciwnie do normalnej spodu), X = długość.
    const gp_Dir up = frame.thickness.Reversed();
    // Kierunek długości musi być prostopadły do "góry" – usuwamy ewentualną odchyłkę.
    gp_Vec len(frame.length);
    len -= gp_Vec(up) * len.Dot(gp_Vec(up));
    if (len.Magnitude() < 1e-9)
        return gp_Trsf();
    const gp_Ax3 panelAxes(gp::Origin(), up, gp_Dir(len));

    // Docelowy układ: Z do góry, długość wzdłuż X albo Y.
    const gp_Dir target = (longEdge == camcore::AlignSettings::LongEdge::X) ? gp::DX() : gp::DY();
    const gp_Ax3 worldAxes(gp::Origin(), gp::DZ(), target);

    gp_Trsf rot;
    rot.SetDisplacement(panelAxes, worldAxes); // przenosi układ płyty na układ docelowy
    return rot;
}

} // namespace

namespace camcore {

gp_Trsf computeAlignment(const TopoDS_Shape& shape, const AlignSettings& s)
{
    // 1. Obrót (tylko w trybie "płyta").
    gp_Trsf rotation;
    if (s.panel) {
        const PanelFrame frame = s.minimalBox ? frameFromMinimalBox(shape)
                                              : frameFromLargestFace(shape);
        if (frame.valid)
            rotation = panelRotation(frame, s.longEdge);
    }

    // 2. Przesunięcie: prostopadłościan otaczający już obróconą bryłę,
    //    liczony z dokładnej geometrii (nie z trójkątów).
    Bnd_Box box;
    BRepBndLib::AddOptimal(shape.Moved(TopLoc_Location(rotation)), box,
                           Standard_False, Standard_False);
    if (box.IsVoid())
        return rotation;
    double xMin, yMin, zMin, xMax, yMax, zMax;
    box.Get(xMin, yMin, zMin, xMax, yMax, zMax);

    const auto pick = [](double lo, double hi, int which) {
        return which == 0 ? lo : which == 1 ? (lo + hi) / 2 : hi;
    };
    const double x = pick(xMin, xMax, static_cast<int>(s.baseX));
    const double y = pick(yMin, yMax, static_cast<int>(s.baseY));
    // ZZero: Top=0, Middle=1, Bottom=2 – odwrotnie niż lo/hi, stąd 2 - ...
    const double z = pick(zMin, zMax, 2 - static_cast<int>(s.zZero));

    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(-x, -y, -z));
    return translation * rotation; // najpierw obrót, potem przesunięcie
}

ImportedModel transformed(const ImportedModel& model, const gp_Trsf& trsf)
{
    // Przekształcenie zapisujemy jako "położenie" (location) kształtu – geometria
    // nie jest kopiowana. Kolory są przypięte do fragmentów bryły, więc dostają
    // to samo położenie, żeby dalej pasowały.
    const TopLoc_Location loc(trsf);
    ImportedModel out = model;
    out.shape.Move(loc);
    for (ShapeColor& c : out.colors)
        c.shape.Move(loc);
    return out;
}

} // namespace camcore
