#include "PartContours.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BOPAlgo_Tools.hxx>
#include <BRepLib.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <TopoDS_Compound.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <Bnd_Box.hxx>
#include <ShapeAnalysis_FreeBounds.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_HSequenceOfShape.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pln.hxx>

#include <algorithm>
#include <cmath>
#include <map>

namespace {

constexpr double kTol = 1e-3; // tolerancja wysokości i łączenia krawędzi [mm]

// Płaska ściana pozioma? Zwraca +1 (normalna w górę), -1 (w dół) albo 0.
int horizontalSide(const TopoDS_Shape& shape, double& z)
{
    const TopoDS_Face& f = TopoDS::Face(shape);
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

void zRange(const TopoDS_Shape& s, double& z0, double& z1)
{
    Bnd_Box box;
    BRepBndLib::AddOptimal(s, box, Standard_False, Standard_False);
    double x0, y0, x1, y1;
    box.Get(x0, y0, z0, x1, y1, z1);
}

// Obrys bryły z rzutu z góry (dokładny algorytm HLR – linie i łuki zostają
// prawdziwymi liniami i łukami). Zwraca zamknięty drut w płaszczyźnie Z = 0
// albo pusty drut, gdy się nie udało.
TopoDS_Wire projectedOutline(const TopoDS_Shape& shape)
{
    // 1. Rzut wszystkich krawędzi bryły na XY, patrząc wzdłuż osi Z.
    //    Bierzemy krawędzie widoczne z góry i linie sylwetki (np. brzeg walca
    //    widziany z boku). Obrys widziany z góry nigdy nie jest zasłonięty, a
    //    zasłonięte krawędzie nakładałyby się na widoczne (np. góra i dół
    //    pionowej ściany dają w rzucie tę samą linię).
    Handle(HLRBRep_Algo) hlr = new HLRBRep_Algo;
    hlr->Add(shape);
    hlr->Projector(HLRAlgo_Projector(gp_Ax2(gp::Origin(), gp::DZ(), gp::DX())));
    hlr->Update();
    hlr->Hide();
    HLRBRep_HLRToShape toShape(hlr);
    TopoDS_Compound edges;
    BRep_Builder builder;
    builder.MakeCompound(edges);
    bool any = false;
    for (const TopoDS_Shape& part : {toShape.VCompound(), toShape.OutLineVCompound(),
                                     toShape.Rg1LineVCompound()}) {
        if (part.IsNull())
            continue;
        // Krawędzie z rzutu mają tylko krzywe 2D na płaszczyźnie rzutu –
        // dobudowujemy im krzywe 3D, których potrzebują dalsze algorytmy.
        BRepLib::BuildCurves3d(part);
        for (TopExp_Explorer ex(part, TopAbs_EDGE); ex.More(); ex.Next()) {
            builder.Add(edges, ex.Current());
            any = true;
        }
    }
    if (!any)
        return {};

    // 2. Krawędzie rzutu dzielimy w miejscach przecięć, składamy w zamknięte
    //    druty i z nich w płaskie obszary (ściany) – jak zamalowanie rysunku.
    TopoDS_Shape wires, faces;
    if (BOPAlgo_Tools::EdgesToWires(edges, wires, Standard_False) != 0
        || !BOPAlgo_Tools::WiresToFaces(wires, faces))
        return {};

    // 3. Obrys całości: krawędzie należące tylko do jednego obszaru (krawędź
    //    między dwoma obszarami leży w środku rzutu), połączone w druty.
    //    Największy z nich to obrys bryły.
    TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
    TopExp::MapShapesAndAncestors(faces, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
    Handle(TopTools_HSequenceOfShape) free = new TopTools_HSequenceOfShape;
    for (int i = 1; i <= edgeFaces.Extent(); ++i) {
        TopTools_IndexedMapOfShape distinct;
        for (const TopoDS_Shape& f : edgeFaces(i))
            distinct.Add(f);
        if (distinct.Extent() == 1)
            free->Append(edgeFaces.FindKey(i));
    }
    if (free->IsEmpty())
        return {};
    Handle(TopTools_HSequenceOfShape) loops;
    ShapeAnalysis_FreeBounds::ConnectEdgesToWires(free, kTol, Standard_False, loops);
    TopoDS_Wire best;
    double bestArea = 0;
    for (int i = 1; i <= loops->Length(); ++i) {
        Bnd_Box wb;
        BRepBndLib::Add(loops->Value(i), wb);
        double a0, b0, c0, a1, b1, c1;
        wb.Get(a0, b0, c0, a1, b1, c1);
        if ((a1 - a0) * (b1 - b0) > bestArea) {
            bestArea = (a1 - a0) * (b1 - b0);
            best = TopoDS::Wire(loops->Value(i));
        }
    }
    if (best.IsNull() || !BRep_Tool::IsClosed(best))
        return {};

    // 4. Krawędzie obrysu pocięte w miejscach, gdzie dochodziły inne linie
    //    rzutu, sklejamy z powrotem (odcinki na jednej prostej, łuki jednego okręgu).
    const TopoDS_Face region = BRepBuilderAPI_MakeFace(best, Standard_True).Face();
    ShapeUpgrade_UnifySameDomain unify(region, Standard_True, Standard_True, Standard_False);
    unify.Build();
    for (TopExp_Explorer ux(unify.Shape(), TopAbs_FACE); ux.More(); ux.Next())
        return BRepTools::OuterWire(TopoDS::Face(ux.Current()));
    return best;
}

class Builder
{
public:
    explicit Builder(const TopoDS_Shape& shape)
        : m_shape(shape)
    {
        TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, m_edgeFaces);
        zRange(shape, m_zBottom, m_zTop);
        TopExp::MapShapes(shape, TopAbs_FACE, m_faces);
    }

    camcore::PartContours run()
    {
        // 1. Poziome ściany grupujemy po wysokości i stronie, a w grupie łączymy
        //    stykające się ściany w obszary (jedno dno może być podzielone w pliku).
        //    Mapa sortuje od dołu. Wierzch płyty pomijamy.
        std::map<std::pair<long long, int>, std::vector<TopoDS_Face>> groups;
        std::map<std::pair<long long, int>, double> groupZ;
        for (int i = 1; i <= m_faces.Extent(); ++i) {
            double z;
            const int side = horizontalSide(m_faces(i), z);
            if (side == 0 || (side > 0 && std::abs(z - m_zTop) < kTol))
                continue;
            const std::pair<long long, int> key{std::llround(z / kTol), side};
            groups[key].push_back(TopoDS::Face(m_faces(i)));
            groupZ[key] = z;
        }
        for (const auto& [key, faces] : groups)
            for (const std::vector<TopoDS_Face>& region : connectedRegions(faces))
                addRegion(region, groupZ.at(key), key.second);

        // Kontur zewnętrzny: obrys rzutu całej bryły z góry, na pełną grubość.
        // Fazy i zaokrąglenia przy spodzie czy wierzchu nie zmniejszają go.
        const TopoDS_Wire outline = projectedOutline(m_shape);
        if (!outline.IsNull())
            m_result.outline = makeContour(outline, 0.0, m_zBottom, m_zTop);
        return m_result;
    }

private:
    // Ściany niepoziome stykające się z krawędziami konturu – jego ściany boczne.
    std::vector<TopoDS_Shape> walls(const TopoDS_Wire& wire) const
    {
        std::vector<TopoDS_Shape> out;
        for (TopExp_Explorer ex(wire, TopAbs_EDGE); ex.More(); ex.Next()) {
            const TopTools_ListOfShape* faces = m_edgeFaces.Seek(ex.Current());
            if (!faces)
                continue;
            for (const TopoDS_Shape& f : *faces) {
                double fz;
                if (horizontalSide(f, fz) != 0)
                    continue;
                if (std::none_of(out.begin(), out.end(), [&](const TopoDS_Shape& g) { return g.IsSame(f); }))
                    out.push_back(f);
            }
        }
        return out;
    }

    void wallRange(const TopoDS_Wire& wire, double z, double& bottom, double& top) const
    {
        bottom = top = z;
        for (const TopoDS_Shape& w : walls(wire)) {
            double z0, z1;
            zRange(w, z0, z1);
            bottom = std::min(bottom, z0);
            top = std::max(top, z1);
        }
    }

    // Ściany boczne konturu kończą się na suficie (poziomej ścianie patrzącej
    // w dół, innej niż spód płyty)? Tak wygląda wejście do kieszeni od spodu.
    bool wallsEndAtCeiling(const TopoDS_Wire& wire) const
    {
        for (const TopoDS_Shape& w : walls(wire))
            for (TopExp_Explorer ex(w, TopAbs_EDGE); ex.More(); ex.Next())
                for (const TopoDS_Shape& f : m_edgeFaces.FindFromKey(ex.Current())) {
                    double fz;
                    if (horizontalSide(f, fz) < 0 && std::abs(fz - m_zBottom) > kTol)
                        return true;
                }
        return false;
    }

    // Dzieli ściany na grupy stykające się krawędziami.
    std::vector<std::vector<TopoDS_Face>> connectedRegions(const std::vector<TopoDS_Face>& faces) const
    {
        std::vector<int> region(faces.size(), -1);
        int count = 0;
        for (size_t i = 0; i < faces.size(); ++i) {
            if (region[i] >= 0)
                continue;
            region[i] = count;
            std::vector<size_t> stack{i};
            while (!stack.empty()) {
                const size_t a = stack.back();
                stack.pop_back();
                TopTools_IndexedMapOfShape edgesA;
                TopExp::MapShapes(faces[a], TopAbs_EDGE, edgesA);
                for (size_t b = 0; b < faces.size(); ++b) {
                    if (region[b] >= 0)
                        continue;
                    for (TopExp_Explorer ex(faces[b], TopAbs_EDGE); ex.More(); ex.Next())
                        if (edgesA.Contains(ex.Current())) {
                            region[b] = count;
                            stack.push_back(b);
                            break;
                        }
                }
            }
            ++count;
        }
        std::vector<std::vector<TopoDS_Face>> out(static_cast<size_t>(count));
        for (size_t i = 0; i < faces.size(); ++i)
            out[static_cast<size_t>(region[i])].push_back(faces[i]);
        return out;
    }

    // Brzeg obszaru: krawędzie należące tylko do jednej jego ściany (krawędzie
    // między ścianami obszaru leżą w środku), połączone w zamknięte druty.
    // Największy drut na początku – to brzeg zewnętrzny.
    static std::vector<TopoDS_Wire> boundary(const std::vector<TopoDS_Face>& faces)
    {
        TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
        for (const TopoDS_Face& f : faces)
            TopExp::MapShapesAndAncestors(f, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
        Handle(TopTools_HSequenceOfShape) edges = new TopTools_HSequenceOfShape;
        for (int i = 1; i <= edgeFaces.Extent(); ++i) {
            TopTools_IndexedMapOfShape distinct; // ta sama ściana może być na liście 2×
            for (const TopoDS_Shape& f : edgeFaces(i))
                distinct.Add(f);
            if (distinct.Extent() == 1)
                edges->Append(edgeFaces.FindKey(i));
        }
        std::vector<TopoDS_Wire> out;
        if (edges->IsEmpty())
            return out;
        Handle(TopTools_HSequenceOfShape) wires;
        ShapeAnalysis_FreeBounds::ConnectEdgesToWires(edges, kTol, Standard_False, wires);
        for (int i = 1; i <= wires->Length(); ++i)
            out.push_back(TopoDS::Wire(wires->Value(i)));
        std::stable_sort(out.begin(), out.end(), [](const TopoDS_Wire& a, const TopoDS_Wire& b) {
            return area(a) > area(b);
        });
        return out;
    }

    static double area(const TopoDS_Wire& w)
    {
        Bnd_Box box;
        BRepBndLib::AddOptimal(w, box, Standard_False, Standard_False);
        double x0, y0, z0, x1, y1, z1;
        box.Get(x0, y0, z0, x1, y1, z1);
        return (x1 - x0) * (y1 - y0);
    }

    // Kontur z drutu leżącego na wysokości z: kopia przeniesiona na górę ścian,
    // jej geometrie po kolei, wymiary i rozpoznanie okręgu.
    static camcore::Contour makeContour(const TopoDS_Wire& wire, double z, double bottom, double top)
    {
        camcore::Contour c;
        c.zTop = top;
        c.zBottom = bottom;
        c.wire = wire;
        if (std::abs(top - z) > 1e-9) {
            gp_Trsf t;
            t.SetTranslation(gp_Vec(0, 0, top - z));
            c.wire = TopoDS::Wire(BRepBuilderAPI_Transform(wire, t, Standard_True).Shape());
        }
        for (BRepTools_WireExplorer ex(c.wire); ex.More(); ex.Next())
            c.geometry.push_back(ex.Current());
        if (c.geometry.empty()) // drut bez ciągłości – bierzemy krawędzie w kolejności zapisu
            for (TopExp_Explorer ex(c.wire, TopAbs_EDGE); ex.More(); ex.Next())
                c.geometry.push_back(TopoDS::Edge(ex.Current()));

        Bnd_Box box;
        BRepBndLib::AddOptimal(c.wire, box, Standard_False, Standard_False);
        double x0, y0, z0, x1, y1, z1;
        box.Get(x0, y0, z0, x1, y1, z1);
        c.sizeX = x1 - x0;
        c.sizeY = y1 - y0;
        c.x = (x0 + x1) / 2;
        c.y = (y0 + y1) / 2;

        // Okrąg: wszystkie geometrie to łuki tego samego okręgu.
        double radius = -1;
        bool circle = !c.geometry.empty();
        for (const TopoDS_Edge& e : c.geometry) {
            const BRepAdaptor_Curve curve(e);
            if (curve.GetType() != GeomAbs_Circle) {
                circle = false;
                break;
            }
            const gp_Circ ci = curve.Circle();
            if (radius < 0)
                radius = ci.Radius();
            if (std::abs(ci.Radius() - radius) > kTol || std::abs(ci.Location().X() - c.x) > kTol
                || std::abs(ci.Location().Y() - c.y) > kTol) {
                circle = false;
                break;
            }
        }
        if (circle)
            c.diameter = 2 * radius;
        return c;
    }

    void addRegion(const std::vector<TopoDS_Face>& faces, double z, int side)
    {
        const std::vector<TopoDS_Wire> wires = boundary(faces);
        if (wires.empty())
            return;

        if (side < 0 && std::abs(z - m_zBottom) < kTol) {
            // 2. Spód płyty: brzeg zewnętrzny pomijamy – kontur zewnętrzny bierzemy
            //    z rzutu bryły (przy fazie na spodzie ten brzeg jest mniejszy od bryły).
            //    Wewnętrzne brzegi, których ściany nie kończą się na suficie, idą
            //    na wylot. Pozostałe to wejścia do kieszeni od spodu – te pomijamy.
            for (size_t i = 1; i < wires.size(); ++i) {
                double bottom, top;
                wallRange(wires[i], z, bottom, top);
                if (!wallsEndAtCeiling(wires[i]))
                    m_result.inner.push_back(makeContour(wires[i], z, z, top));
            }
            return;
        }

        // Sufity kieszeni od spodu (ścianki rosną w dół) nie biorą udziału w ekstrakcji.
        if (side < 0)
            return;

        // 3. Dno kieszeni: ścianki rosną w górę od dna. Brzeg zewnętrzny to kontur
        //    zewnętrzny kieszeni, wewnętrzne brzegi ze ściankami w górę to wyspy.
        //    Brzegi ze ściankami w dół to wejścia do głębszych kieszeni – pomijamy.
        camcore::Pocket pocket;
        pocket.zFloor = z;
        for (size_t i = 0; i < wires.size(); ++i) {
            double bottom, top;
            wallRange(wires[i], z, bottom, top);
            if (top < z + kTol)
                continue;
            pocket.contours.push_back(makeContour(wires[i], z, z, top));
        }
        if (pocket.contours.empty())
            return;
        const camcore::Contour& outer = pocket.contours.front();
        pocket.depth = outer.zTop - outer.zBottom;
        m_result.pockets.push_back(std::move(pocket));
    }

    TopoDS_Shape m_shape;
    double m_zTop = 0, m_zBottom = 0;
    TopTools_IndexedMapOfShape m_faces;
    TopTools_IndexedDataMapOfShapeListOfShape m_edgeFaces;
    camcore::PartContours m_result;
};

} // namespace

namespace camcore {

Contour translated(const Contour& contour, const gp_Vec& offset)
{
    gp_Trsf t;
    t.SetTranslation(offset);
    Contour c = contour;
    c.wire = TopoDS::Wire(BRepBuilderAPI_Transform(contour.wire, t, Standard_True).Shape());
    c.geometry.clear();
    for (BRepTools_WireExplorer ex(c.wire); ex.More(); ex.Next())
        c.geometry.push_back(ex.Current());
    if (c.geometry.empty())
        for (TopExp_Explorer ex(c.wire, TopAbs_EDGE); ex.More(); ex.Next())
            c.geometry.push_back(TopoDS::Edge(ex.Current()));
    c.x += offset.X();
    c.y += offset.Y();
    c.zTop += offset.Z();
    c.zBottom += offset.Z();
    return c;
}

PartContours buildPartContours(const TopoDS_Shape& shape)
{
    return Builder(shape).run();
}

} // namespace camcore
