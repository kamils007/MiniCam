#include "FeatureRecognition.h"

#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Pln.hxx>

#include <cmath>
#include <set>

namespace {

constexpr double kTol = 1e-3; // tolerancja położeń [mm]
constexpr double kPi = 3.14159265358979323846;

struct ZRange
{
    double zMin, zMax;
};

Bnd_Box faceBox(const TopoDS_Face& f)
{
    Bnd_Box box;
    BRepBndLib::AddOptimal(f, box, Standard_False, Standard_False);
    return box;
}

ZRange zRange(const TopoDS_Face& f)
{
    double x0, y0, z0, x1, y1, z1;
    faceBox(f).Get(x0, y0, z0, x1, y1, z1);
    return {z0, z1};
}

// Płaska ściana pozioma? Zwraca kierunek normalnej na zewnątrz (+1 do góry, -1 w dół).
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

// Pionowa ściana walcowa "wklęsła" (materiał na zewnątrz) – fragment otworu.
bool isConcaveVerticalCylinder(const TopoDS_Face& f, gp_Cylinder& cyl)
{
    const BRepAdaptor_Surface s(f);
    if (s.GetType() != GeomAbs_Cylinder)
        return false;
    cyl = s.Cylinder();
    if (std::abs(std::abs(cyl.Axis().Direction().Z()) - 1.0) > 1e-6)
        return false;
    // Normalna w środku ściany vs kierunek od osi: przeciwne = ściana wklęsła.
    const double u = (s.FirstUParameter() + s.LastUParameter()) / 2;
    const double v = (s.FirstVParameter() + s.LastVParameter()) / 2;
    gp_Pnt p;
    gp_Vec du, dv;
    s.D1(u, v, p, du, dv);
    gp_Vec n = du.Crossed(dv);
    if (f.Orientation() == TopAbs_REVERSED)
        n.Reverse();
    gp_Vec radial(cyl.Axis().Location(), p);
    radial -= gp_Vec(cyl.Axis().Direction()) * radial.Dot(gp_Vec(cyl.Axis().Direction()));
    return n.Dot(radial) < 0;
}

double angularSpan(const TopoDS_Face& f)
{
    const BRepAdaptor_Surface s(f);
    return s.LastUParameter() - s.FirstUParameter();
}

void boxOf(const std::vector<TopoDS_Face>& faces, double& x0, double& y0, double& x1, double& y1)
{
    Bnd_Box box;
    for (const TopoDS_Face& f : faces)
        BRepBndLib::AddOptimal(f, box, Standard_False, Standard_False);
    double z0, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
}

class Recognizer
{
public:
    explicit Recognizer(const TopoDS_Shape& shape)
    {
        Bnd_Box box;
        BRepBndLib::AddOptimal(shape, box, Standard_False, Standard_False);
        double x0, y0, x1, y1;
        box.Get(x0, y0, m_zBottom, x1, y1, m_zTop);
        TopExp::MapShapes(shape, TopAbs_FACE, m_faces);
        // Mapa krawędź → ściany, które się na niej stykają (sąsiedztwo ścian).
        TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, m_edgeFaces);
    }

    std::vector<camcore::Feature> run()
    {
        findHoles();
        findPockets();
        findCutouts();
        return m_result;
    }

private:
    bool used(const TopoDS_Face& f) const { return m_used.count(m_faces.FindIndex(f)) > 0; }
    void markUsed(const std::vector<TopoDS_Face>& faces)
    {
        for (const TopoDS_Face& f : faces)
            m_used.insert(m_faces.FindIndex(f));
    }

    std::vector<TopoDS_Face> neighbours(const TopoDS_Face& f) const
    {
        std::vector<TopoDS_Face> out;
        for (TopExp_Explorer ex(f, TopAbs_EDGE); ex.More(); ex.Next()) {
            const TopTools_ListOfShape& faces = m_edgeFaces.FindFromKey(ex.Current());
            for (const TopoDS_Shape& g : faces)
                if (!g.IsSame(f))
                    out.push_back(TopoDS::Face(g));
        }
        return out;
    }

    // Otwory: wklęsłe walce pionowe o tej samej osi i promieniu, razem pełne 360°
    // (zaokrąglenia naroży kieszeni mają tylko 90°, więc nie są otworami).
    void findHoles()
    {
        struct Group { gp_Cylinder cyl; double span = 0; std::vector<TopoDS_Face> faces; };
        std::vector<Group> groups;
        for (int i = 1; i <= m_faces.Extent(); ++i) {
            const TopoDS_Face f = TopoDS::Face(m_faces(i));
            gp_Cylinder cyl;
            if (!isConcaveVerticalCylinder(f, cyl))
                continue;
            Group* g = nullptr;
            for (Group& cand : groups) {
                const gp_Pnt a = cand.cyl.Location(), b = cyl.Location();
                if (std::abs(cand.cyl.Radius() - cyl.Radius()) < kTol
                    && std::hypot(a.X() - b.X(), a.Y() - b.Y()) < kTol) {
                    g = &cand;
                    break;
                }
            }
            if (!g) {
                groups.push_back({cyl});
                g = &groups.back();
            }
            g->span += angularSpan(f);
            g->faces.push_back(f);
        }

        for (Group& g : groups) {
            if (g.span < 2 * kPi - 1e-3)
                continue;
            double zMin = 1e100, zMax = -1e100;
            for (const TopoDS_Face& f : g.faces) {
                const ZRange r = zRange(f);
                zMin = std::min(zMin, r.zMin);
                zMax = std::max(zMax, r.zMax);
            }
            camcore::Feature h;
            h.type = camcore::Feature::Type::Hole;
            h.diameter = 2 * g.cyl.Radius();
            h.sizeX = h.sizeY = h.diameter;
            h.x = g.cyl.Location().X();
            h.y = g.cyl.Location().Y();
            h.faces = g.faces;
            const bool fromTop = std::abs(zMax - m_zTop) < kTol;
            const bool fromBottom = std::abs(zMin - m_zBottom) < kTol;
            if (fromTop && fromBottom) {
                h.side = camcore::Feature::Side::Through;
                h.depth = m_zTop - m_zBottom;
            } else if (fromBottom) {
                h.side = camcore::Feature::Side::Bottom;
                h.depth = zMax - m_zBottom;
                addFloor(h, zMax, -1);
            } else {
                // Z góry – także otwór w dnie kieszeni (głębokość liczymy od wierzchu płyty).
                h.side = camcore::Feature::Side::Top;
                h.depth = m_zTop - zMin;
                addFloor(h, zMin, +1);
            }
            markUsed(h.faces);
            m_result.push_back(h);
        }
    }

    // Dno otworu nieprzelotowego: płaska ściana sąsiadująca z walcem na wysokości z.
    void addFloor(camcore::Feature& h, double z, int normalSide)
    {
        const std::vector<TopoDS_Face> walls = h.faces;
        for (const TopoDS_Face& w : walls) {
            for (const TopoDS_Face& n : neighbours(w)) {
                double nz;
                if (horizontalSide(n, nz) == normalSide && std::abs(nz - z) < kTol && !contains(h.faces, n))
                    h.faces.push_back(n);
            }
        }
    }

    static bool contains(const std::vector<TopoDS_Face>& v, const TopoDS_Face& f)
    {
        for (const TopoDS_Face& g : v)
            if (g.IsSame(f))
                return true;
        return false;
    }

    // Kieszenie: dno = pozioma ściana między wierzchem a spodem płyty.
    // Normalna do góry → kieszeń frezowana z góry, w dół → od spodu.
    void findPockets()
    {
        for (int i = 1; i <= m_faces.Extent(); ++i) {
            const TopoDS_Face floor = TopoDS::Face(m_faces(i));
            if (used(floor))
                continue;
            double z;
            const int side = horizontalSide(floor, z);
            if (side == 0)
                continue;
            if (side > 0 && std::abs(z - m_zTop) < kTol)
                continue; // to wierzch płyty
            if (side < 0 && std::abs(z - m_zBottom) < kTol)
                continue; // to spód płyty

            camcore::Feature p;
            p.type = camcore::Feature::Type::Pocket;
            p.side = side > 0 ? camcore::Feature::Side::Top : camcore::Feature::Side::Bottom;
            p.depth = side > 0 ? m_zTop - z : z - m_zBottom;
            p.faces = {floor};
            collectWalls(p.faces, z, side);

            double x0, y0, x1, y1;
            boxOf({floor}, x0, y0, x1, y1);
            p.sizeX = x1 - x0;
            p.sizeY = y1 - y0;
            p.x = (x0 + x1) / 2;
            p.y = (y0 + y1) / 2;
            markUsed({floor});
            m_result.push_back(p);
        }
    }

    // Ściany boczne kieszeni: sąsiedzi dna (i dalej), które leżą nad dnem
    // (dla kieszeni od spodu – pod nim), aż do powierzchni płyty.
    void collectWalls(std::vector<TopoDS_Face>& faces, double z, int side)
    {
        for (size_t i = 0; i < faces.size(); ++i) {
            for (const TopoDS_Face& n : neighbours(faces[i])) {
                if (contains(faces, n) || used(n))
                    continue;
                double nz;
                if (horizontalSide(n, nz) != 0)
                    continue; // poziome ściany to wierzch/spód/inne dna
                const ZRange r = zRange(n);
                const bool inside = side > 0 ? r.zMin > z - kTol : r.zMax < z + kTol;
                if (inside)
                    faces.push_back(n);
            }
        }
    }

    // Wycięcia przelotowe (nie okrągłe): wewnętrzne obrysy poziomych ścian patrzących
    // do góry, których ściany boczne schodzą aż do spodu płyty.
    void findCutouts()
    {
        for (int i = 1; i <= m_faces.Extent(); ++i) {
            const TopoDS_Face face = TopoDS::Face(m_faces(i));
            double z;
            if (horizontalSide(face, z) != 1)
                continue;
            const TopoDS_Wire outer = BRepTools::OuterWire(face);
            for (TopExp_Explorer wex(face, TopAbs_WIRE); wex.More(); wex.Next()) {
                if (wex.Current().IsSame(outer))
                    continue;
                // Ściany przylegające do wewnętrznego obrysu (bez samej ściany poziomej).
                std::vector<TopoDS_Face> walls;
                bool ok = true;
                for (TopExp_Explorer eex(wex.Current(), TopAbs_EDGE); eex.More(); eex.Next()) {
                    for (const TopoDS_Shape& g : m_edgeFaces.FindFromKey(eex.Current())) {
                        const TopoDS_Face w = TopoDS::Face(g);
                        if (w.IsSame(face) || contains(walls, w))
                            continue;
                        if (used(w)) { ok = false; break; } // otwór albo ściana kieszeni
                        walls.push_back(w);
                    }
                }
                if (!ok || walls.empty())
                    continue;
                // Przelotowe, jeśli ściany sięgają spodu płyty.
                double zMin = 1e100;
                for (const TopoDS_Face& w : walls)
                    zMin = std::min(zMin, zRange(w).zMin);
                if (std::abs(zMin - m_zBottom) > kTol)
                    continue;

                camcore::Feature c;
                c.type = camcore::Feature::Type::Cutout;
                c.side = camcore::Feature::Side::Through;
                c.depth = z - m_zBottom;
                c.faces = walls;
                double x0, y0, x1, y1;
                boxOf(walls, x0, y0, x1, y1);
                c.sizeX = x1 - x0;
                c.sizeY = y1 - y0;
                c.x = (x0 + x1) / 2;
                c.y = (y0 + y1) / 2;
                markUsed(walls);
                m_result.push_back(c);
            }
        }
    }

    double m_zTop = 0, m_zBottom = 0;
    TopTools_IndexedMapOfShape m_faces;
    TopTools_IndexedDataMapOfShapeListOfShape m_edgeFaces;
    std::set<int> m_used;
    std::vector<camcore::Feature> m_result;
};

} // namespace

namespace camcore {

std::vector<Feature> recognizeFeatures(const TopoDS_Shape& shape)
{
    return Recognizer(shape).run();
}

} // namespace camcore
