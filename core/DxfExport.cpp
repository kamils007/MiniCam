#include "DxfExport.h"

#include <BRepAdaptor_Curve.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GeomAbs_CurveType.hxx>
#include <gp_Circ.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>

namespace camcore {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeflection = 0.01; // dokładność zamiany krzywych na odcinki [mm]

// Pliki DXF to pary: kod grupy i wartość, każde w osobnej linii.
class DxfStream
{
public:
    DxfStream() { m_out.imbue(std::locale::classic()); } // kropka dziesiętna niezależnie od systemu
    void code(int c, const std::string& value) { m_out << c << '\n' << value << '\n'; }
    void code(int c, int value) { m_out << c << '\n' << value << '\n'; }
    void code(int c, double value)
    {
        if (std::abs(value) < 1e-9)
            value = 0; // bez "-0.000000"
        m_out << c << '\n' << std::fixed << std::setprecision(6) << value << '\n';
    }
    void point(int base, const gp_Pnt& p) // 10/20/30, 11/21/31 …
    {
        code(base, p.X());
        code(base + 10, p.Y());
        code(base + 20, p.Z());
    }
    std::string str() const { return m_out.str(); }

private:
    std::ostringstream m_out;
};

// Najbliższy kolor z palety AutoCAD (ACI) – warstwa w DXF R12 ma kolor jako numer.
int aciColor(const Quantity_Color& color)
{
    static const struct { int aci; double r, g, b; } palette[] = {
        {1, 1, 0, 0},       {2, 1, 1, 0},     {3, 0, 1, 0},     {4, 0, 1, 1},
        {5, 0, 0, 1},       {6, 1, 0, 1},     {7, 1, 1, 1},     {8, 0.5, 0.5, 0.5},
        {9, 0.75, 0.75, 0.75}, {30, 1, 0.5, 0}, {150, 0, 0.5, 1}, {200, 0.5, 0, 1},
        {34, 0.5, 0.25, 0}, {94, 0, 0.5, 0},  {134, 0, 0.5, 0.5},
    };
    double r, g, b;
    color.Values(r, g, b, Quantity_TOC_sRGB);
    int best = 7;
    double bestDist = 1e9;
    for (const auto& c : palette) {
        const double d = (r - c.r) * (r - c.r) + (g - c.g) * (g - c.g) + (b - c.b) * (b - c.b);
        if (d < bestDist) {
            bestDist = d;
            best = c.aci;
        }
    }
    return best;
}

double degrees(double radians)
{
    double d = radians * 180.0 / kPi;
    while (d < 0)
        d += 360.0;
    while (d >= 360.0)
        d -= 360.0;
    return d;
}

void writeLine(DxfStream& dxf, const std::string& layer, const gp_Pnt& a, const gp_Pnt& b)
{
    if (a.Distance(b) < 1e-9)
        return;
    dxf.code(0, "LINE");
    dxf.code(8, layer);
    dxf.point(10, a);
    dxf.point(11, b);
}

// Krzywa, której nie da się zapisać jako LINE/ARC/CIRCLE – ciąg odcinków.
void writePolyline(DxfStream& dxf, const std::string& layer, const BRepAdaptor_Curve& curve)
{
    GCPnts_QuasiUniformDeflection points(curve, kDeflection, curve.FirstParameter(), curve.LastParameter());
    if (!points.IsDone() || points.NbPoints() < 2) {
        writeLine(dxf, layer, curve.Value(curve.FirstParameter()), curve.Value(curve.LastParameter()));
        return;
    }
    for (int i = 1; i < points.NbPoints(); ++i)
        writeLine(dxf, layer, points.Value(i), points.Value(i + 1));
}

void writeEdge(DxfStream& dxf, const std::string& layer, const TopoDS_Edge& edge)
{
    BRepAdaptor_Curve curve(edge);
    const double first = curve.FirstParameter(), last = curve.LastParameter();
    switch (curve.GetType()) {
    case GeomAbs_Line:
        writeLine(dxf, layer, curve.Value(first), curve.Value(last));
        return;
    case GeomAbs_Circle: {
        const gp_Circ circ = curve.Circle();
        const double nz = circ.Axis().Direction().Z();
        if (std::abs(std::abs(nz) - 1.0) > 1e-6)
            break; // okrąg nie leży poziomo – odcinkami
        const gp_Pnt c = circ.Location();
        if (std::abs(last - first - 2 * kPi) < 1e-7) {
            dxf.code(0, "CIRCLE");
            dxf.code(8, layer);
            dxf.point(10, c);
            dxf.code(40, circ.Radius());
            return;
        }
        // ARC w DXF idzie zawsze przeciwnie do wskazówek zegara (patrząc z góry),
        // od kąta początkowego do końcowego. Łuk "od dołu" (oś w -Z) zamieniamy końcami.
        gp_Pnt a = curve.Value(first), b = curve.Value(last);
        if (nz < 0)
            std::swap(a, b);
        dxf.code(0, "ARC");
        dxf.code(8, layer);
        dxf.point(10, c);
        dxf.code(40, circ.Radius());
        dxf.code(50, degrees(std::atan2(a.Y() - c.Y(), a.X() - c.X())));
        dxf.code(51, degrees(std::atan2(b.Y() - c.Y(), b.X() - c.X())));
        return;
    }
    default:
        break;
    }
    writePolyline(dxf, layer, curve);
}

} // namespace

std::string dxfLayerName(const std::string& layer)
{
    if (layer == kApsLayer)
        return "0";
    // Polskie litery w UTF-8 → litery bez ogonków.
    static const struct { const char* utf8; char ascii; } letters[] = {
        {"ą", 'a'}, {"ć", 'c'}, {"ę", 'e'}, {"ł", 'l'}, {"ń", 'n'}, {"ó", 'o'},
        {"ś", 's'}, {"ź", 'z'}, {"ż", 'z'}, {"Ą", 'A'}, {"Ć", 'C'}, {"Ę", 'E'},
        {"Ł", 'L'}, {"Ń", 'N'}, {"Ó", 'O'}, {"Ś", 'S'}, {"Ź", 'Z'}, {"Ż", 'Z'},
    };
    std::string out;
    for (size_t i = 0; i < layer.size();) {
        bool replaced = false;
        for (const auto& l : letters)
            if (layer.compare(i, std::char_traits<char>::length(l.utf8), l.utf8) == 0) {
                out += l.ascii;
                i += std::char_traits<char>::length(l.utf8);
                replaced = true;
                break;
            }
        if (replaced)
            continue;
        const unsigned char ch = static_cast<unsigned char>(layer[i++]);
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '$')
            out += static_cast<char>(ch);
        else if (ch < 0x80 || (ch & 0xC0) == 0xC0)
            out += '_'; // spacja, kropka itp. albo początek innego znaku UTF-8
    }
    return out.empty() ? "0" : out;
}

std::string writeDxf(const std::vector<Geometry>& geometries, const LayerList& layers)
{
    auto exported = [&](const Geometry& g) { return g.visible && layers.layerOrAps(g.layer).visible; };

    // Warstwy użyte przez zapisywane geometrie (APS = "0" jest zawsze).
    std::set<std::string> used;
    for (const Geometry& g : geometries)
        if (exported(g))
            used.insert(g.layer);

    DxfStream dxf;
    dxf.code(0, "SECTION");
    dxf.code(2, "HEADER");
    dxf.code(9, "$ACADVER");
    dxf.code(1, "AC1009");
    dxf.code(9, "$INSUNITS");
    dxf.code(70, 4); // milimetry
    dxf.code(9, "$MEASUREMENT");
    dxf.code(70, 1); // metryczne
    dxf.code(0, "ENDSEC");

    dxf.code(0, "SECTION");
    dxf.code(2, "TABLES");
    dxf.code(0, "TABLE");
    dxf.code(2, "LAYER");
    std::vector<std::pair<std::string, int>> dxfLayers = {{"0", aciColor(kApsColor)}};
    for (const Layer& l : layers.all())
        if (l.name != kApsLayer && used.count(l.name))
            dxfLayers.push_back({dxfLayerName(l.name), aciColor(l.color)});
    dxf.code(70, static_cast<int>(dxfLayers.size()));
    for (const auto& [name, color] : dxfLayers) {
        dxf.code(0, "LAYER");
        dxf.code(2, name);
        dxf.code(70, 0);
        dxf.code(62, color);
        dxf.code(6, "CONTINUOUS");
    }
    dxf.code(0, "ENDTAB");
    dxf.code(0, "ENDSEC");

    dxf.code(0, "SECTION");
    dxf.code(2, "ENTITIES");
    for (const Geometry& g : geometries) {
        if (!exported(g))
            continue;
        const std::string layer = dxfLayerName(layers.find(g.layer) ? g.layer : kApsLayer);
        for (const TopoDS_Edge& edge : g.contour.geometry)
            if (!edge.IsNull())
                writeEdge(dxf, layer, edge);
    }
    dxf.code(0, "ENDSEC");
    dxf.code(0, "EOF");
    return dxf.str();
}

} // namespace camcore
