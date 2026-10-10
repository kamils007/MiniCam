#include "ContourLevels.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <Bnd_Box.hxx>
#include <ShapeAnalysis_FreeBounds.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_HSequenceOfShape.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pln.hxx>

#include <algorithm>
#include <cmath>
#include <map>

namespace {

constexpr double kTol = 1e-3; // tolerancja wysokości i łączenia krawędzi [mm]

// Płaska ściana pozioma? Zwraca +1 (normalna w górę), -1 (w dół) albo 0.
int horizontalSide(const TopoDS_Face& f, double& z)
{
    const BRepAdaptor_Surface s(f);
    if (s.GetType() != GeomAbs_Plane)
        return 0;
    const gp_Pln pln = s.Plane();
    double nz = pln.Axis().Direction().Z();
    if (f.Orientation() == TopAbs_REVERSED)
        nz = -nz;
    if (std::abs(std::abs(nz) - 1.0) > 1e-6)
        return 0;
    z = pln.Location().Z();
    return nz > 0 ? 1 : -1;
}

// Najwyższy punkt ścian bocznych konturu: ściany niepoziome, które stykają się
// z jego krawędziami. Bez ścian bocznych kontur zostaje na wysokości z.
double wallTop(const TopoDS_Wire& wire, const TopTools_IndexedDataMapOfShapeListOfShape& edgeFaces,
               double z)
{
    double top = z;
    for (TopExp_Explorer ex(wire, TopAbs_EDGE); ex.More(); ex.Next()) {
        const TopTools_ListOfShape* faces = edgeFaces.Seek(ex.Current());
        if (!faces)
            continue;
        for (const TopoDS_Shape& f : *faces) {
            double fz;
            if (horizontalSide(TopoDS::Face(f), fz) != 0)
                continue;
            Bnd_Box box;
            BRepBndLib::AddOptimal(f, box, Standard_False, Standard_False);
            double x0, y0, z0, x1, y1, z1;
            box.Get(x0, y0, z0, x1, y1, z1);
            top = std::max(top, z1);
        }
    }
    return top;
}

TopoDS_Wire moveToZ(const TopoDS_Wire& wire, double fromZ, double toZ)
{
    if (std::abs(toZ - fromZ) < 1e-9)
        return wire;
    gp_Trsf t;
    t.SetTranslation(gp_Vec(0, 0, toZ - fromZ));
    return TopoDS::Wire(BRepBuilderAPI_Transform(wire, t, Standard_True).Shape());
}

bool same(const camcore::LevelContour& a, const camcore::LevelContour& b)
{
    return std::abs(a.x - b.x) < kTol && std::abs(a.y - b.y) < kTol
           && std::abs(a.sizeX - b.sizeX) < kTol && std::abs(a.sizeY - b.sizeY) < kTol
           && std::abs(a.diameter - b.diameter) < kTol;
}

// Wypełnia wymiary konturu i rozpoznaje pełny okrąg.
camcore::LevelContour describe(const TopoDS_Wire& wire)
{
    camcore::LevelContour c;
    c.wire = wire;
    Bnd_Box box;
    BRepBndLib::AddOptimal(wire, box, Standard_False, Standard_False);
    double x0, y0, z0, x1, y1, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
    c.sizeX = x1 - x0;
    c.sizeY = y1 - y0;
    c.x = (x0 + x1) / 2;
    c.y = (y0 + y1) / 2;

    // Okrąg: wszystkie krawędzie to łuki tego samego okręgu.
    bool circle = true;
    double radius = -1;
    for (TopExp_Explorer ex(wire, TopAbs_EDGE); ex.More() && circle; ex.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(ex.Current()));
        if (curve.GetType() != GeomAbs_Circle) {
            circle = false;
            break;
        }
        const gp_Circ ci = curve.Circle();
        if (radius < 0)
            radius = ci.Radius();
        circle = std::abs(ci.Radius() - radius) < kTol
                 && std::abs(ci.Location().X() - c.x) < kTol
                 && std::abs(ci.Location().Y() - c.y) < kTol;
    }
    if (circle && radius > 0)
        c.diameter = 2 * radius;
    return c;
}

// Obrys a zawiera obrys b (porównanie prostokątów otaczających).
bool encloses(const camcore::LevelContour& a, const camcore::LevelContour& b)
{
    return a.sizeX * a.sizeY > b.sizeX * b.sizeY
           && b.x - b.sizeX / 2 > a.x - a.sizeX / 2 - kTol
           && b.x + b.sizeX / 2 < a.x + a.sizeX / 2 + kTol
           && b.y - b.sizeY / 2 > a.y - a.sizeY / 2 - kTol
           && b.y + b.sizeY / 2 < a.y + a.sizeY / 2 + kTol;
}

} // namespace

namespace camcore {

std::vector<ContourLevel> buildContourLevels(const TopoDS_Shape& shape)
{
    // Sąsiedztwo w całej bryle: krawędź → ściany, które się na niej stykają.
    TopTools_IndexedDataMapOfShapeListOfShape shapeEdgeFaces;
    TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, shapeEdgeFaces);
    Bnd_Box shapeBox;
    BRepBndLib::AddOptimal(shape, shapeBox, Standard_False, Standard_False);
    double bx0, by0, zBottom, bx1, by1, zTop;
    shapeBox.Get(bx0, by0, zBottom, bx1, by1, zTop);

    // 1. Zbieramy poziome ściany i grupujemy je po wysokości i stronie.
    //    Klucz: (Z zaokrąglone do tolerancji, strona) – mapa sortuje od dołu.
    //    Wierzch płyty pomijamy – jego kontury dają już ściany niższych poziomów.
    std::map<std::pair<long long, int>, std::vector<TopoDS_Face>> groups;
    std::map<std::pair<long long, int>, double> groupZ;
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        const TopoDS_Face f = TopoDS::Face(ex.Current());
        double z;
        const int side = horizontalSide(f, z);
        if (side == 0 || (side > 0 && std::abs(z - zTop) < kTol))
            continue;
        const std::pair<long long, int> key{std::llround(z / kTol), side};
        groups[key].push_back(f);
        groupZ[key] = z;
    }

    // Kontury pogrupowane po wysokości, na której ostatecznie leżą.
    std::map<long long, ContourLevel> byHeight;
    for (const auto& [key, faces] : groups) {
        // 2. Krawędzie wszystkich ścian poziomu. Krawędź wspólna dla dwóch ścian
        //    tego samego poziomu leży w środku (np. podział ściany w pliku) –
        //    na kontur trafiają tylko krawędzie należące do jednej ściany.
        TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
        for (const TopoDS_Face& f : faces)
            TopExp::MapShapesAndAncestors(f, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
        Handle(TopTools_HSequenceOfShape) edges = new TopTools_HSequenceOfShape;
        for (int i = 1; i <= edgeFaces.Extent(); ++i) {
            const TopoDS_Edge& e = TopoDS::Edge(edgeFaces.FindKey(i));
            int count = 0;
            for (const TopoDS_Face& f : faces) {
                for (TopExp_Explorer fe(f, TopAbs_EDGE); fe.More(); fe.Next())
                    if (fe.Current().IsSame(e)) {
                        ++count;
                        break;
                    }
            }
            if (count == 1)
                edges->Append(e);
        }
        if (edges->IsEmpty())
            continue;

        // 3. Łączymy krawędzie w zamknięte kontury (druty).
        Handle(TopTools_HSequenceOfShape) wires;
        ShapeAnalysis_FreeBounds::ConnectEdgesToWires(edges, kTol, Standard_False, wires);

        // 4. Każdy kontur przenosimy na wysokość najwyższej ze swoich ścian bocznych.
        const double z = groupZ.at(key);
        for (int i = 1; i <= wires->Length(); ++i) {
            const TopoDS_Wire wire = TopoDS::Wire(wires->Value(i));
            const double top = wallTop(wire, shapeEdgeFaces, z);
            ContourLevel& level = byHeight[std::llround(top / kTol)];
            level.z = top;
            LevelContour c = describe(moveToZ(wire, z, top));
            // Ten sam kontur mógł już przyjść z innego poziomu (np. otwór
            // nieprzelotowy: brzeg dna i brzeg w ścianie wyżej) – zostawiamy jeden.
            bool duplicate = false;
            for (const LevelContour& other : level.contours)
                duplicate = duplicate || same(other, c);
            if (!duplicate)
                level.contours.push_back(c);
        }
    }

    std::vector<ContourLevel> levels;
    for (auto& [height, level] : byHeight) {
        // 5. Kontury wewnątrz innych (otwory, wnęki) oznaczamy jako wewnętrzne.
        for (LevelContour& c : level.contours)
            for (const LevelContour& other : level.contours)
                if (&c != &other && encloses(other, c)) {
                    c.inner = true;
                    break;
                }
        // Największe obrysy na początek listy.
        std::stable_sort(level.contours.begin(), level.contours.end(),
                         [](const LevelContour& a, const LevelContour& b) {
                             return a.sizeX * a.sizeY > b.sizeX * b.sizeY;
                         });
        levels.push_back(std::move(level));
    }
    return levels;
}

} // namespace camcore
