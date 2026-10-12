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

// Najbliższy kolor z pełnej palety AutoCAD (ACI 1–255) – warstwa w DXF R12 ma kolor jako numer.
int aciColor(const Quantity_Color& color)
{
    static const unsigned char palette[256][3] = {
        {0,0,0}, {255,0,0}, {255,255,0}, {0,255,0}, {0,255,255}, {0,0,255}, {255,0,255}, {255,255,255},
        {128,128,128}, {192,192,192}, {255,0,0}, {255,127,127}, {165,0,0}, {165,82,82}, {127,0,0}, {127,63,63},
        {76,0,0}, {76,38,38}, {38,0,0}, {38,19,19}, {255,63,0}, {255,159,127}, {165,41,0}, {165,103,82},
        {127,31,0}, {127,79,63}, {76,19,0}, {76,47,38}, {38,9,0}, {38,23,19}, {255,127,0}, {255,191,127},
        {165,82,0}, {165,124,82}, {127,63,0}, {127,95,63}, {76,38,0}, {76,57,38}, {38,19,0}, {38,28,19},
        {255,191,0}, {255,223,127}, {165,124,0}, {165,145,82}, {127,95,0}, {127,111,63}, {76,57,0}, {76,66,38},
        {38,28,0}, {38,33,19}, {255,255,0}, {255,255,127}, {165,165,0}, {165,165,82}, {127,127,0}, {127,127,63},
        {76,76,0}, {76,76,38}, {38,38,0}, {38,38,19}, {191,255,0}, {223,255,127}, {124,165,0}, {145,165,82},
        {95,127,0}, {111,127,63}, {57,76,0}, {66,76,38}, {28,38,0}, {33,38,19}, {127,255,0}, {191,255,127},
        {82,165,0}, {124,165,82}, {63,127,0}, {95,127,63}, {38,76,0}, {57,76,38}, {19,38,0}, {28,38,19},
        {63,255,0}, {159,255,127}, {41,165,0}, {103,165,82}, {31,127,0}, {79,127,63}, {19,76,0}, {47,76,38},
        {9,38,0}, {23,38,19}, {0,255,0}, {127,255,127}, {0,165,0}, {82,165,82}, {0,127,0}, {63,127,63},
        {0,76,0}, {38,76,38}, {0,38,0}, {19,38,19}, {0,255,63}, {127,255,159}, {0,165,41}, {82,165,103},
        {0,127,31}, {63,127,79}, {0,76,19}, {38,76,47}, {0,38,9}, {19,88,23}, {0,255,127}, {127,255,191},
        {0,165,82}, {82,165,124}, {0,127,63}, {63,127,95}, {0,76,38}, {38,76,57}, {0,38,19}, {19,88,28},
        {0,255,191}, {127,255,223}, {0,165,124}, {82,165,145}, {0,127,95}, {63,127,111}, {0,76,57}, {38,76,66},
        {0,38,28}, {19,88,88}, {0,255,255}, {127,255,255}, {0,165,165}, {82,165,165}, {0,127,127}, {63,127,127},
        {0,76,76}, {38,76,76}, {0,38,38}, {19,88,88}, {0,191,255}, {127,223,255}, {0,124,165}, {82,145,165},
        {0,95,127}, {63,111,127}, {0,57,76}, {38,66,126}, {0,28,38}, {19,88,88}, {0,127,255}, {127,191,255},
        {0,82,165}, {82,124,165}, {0,63,127}, {63,95,127}, {0,38,76}, {38,57,126}, {0,19,38}, {19,28,88},
        {0,63,255}, {127,159,255}, {0,41,165}, {82,103,165}, {0,31,127}, {63,79,127}, {0,19,76}, {38,47,126},
        {0,9,38}, {19,23,88}, {0,0,255}, {127,127,255}, {0,0,165}, {82,82,165}, {0,0,127}, {63,63,127},
        {0,0,76}, {38,38,126}, {0,0,38}, {19,19,88}, {63,0,255}, {159,127,255}, {41,0,165}, {103,82,165},
        {31,0,127}, {79,63,127}, {19,0,76}, {47,38,126}, {9,0,38}, {23,19,88}, {127,0,255}, {191,127,255},
        {82,0,165}, {124,82,165}, {63,0,127}, {95,63,127}, {38,0,76}, {57,38,126}, {19,0,38}, {28,19,88},
        {191,0,255}, {223,127,255}, {124,0,165}, {145,82,165}, {95,0,127}, {111,63,127}, {57,0,76}, {66,38,76},
        {28,0,38}, {88,19,88}, {255,0,255}, {255,127,255}, {165,0,165}, {165,82,165}, {127,0,127}, {127,63,127},
        {76,0,76}, {76,38,76}, {38,0,38}, {88,19,88}, {255,0,191}, {255,127,223}, {165,0,124}, {165,82,145},
        {127,0,95}, {127,63,111}, {76,0,57}, {76,38,66}, {38,0,28}, {88,19,88}, {255,0,127}, {255,127,191},
        {165,0,82}, {165,82,124}, {127,0,63}, {127,63,95}, {76,0,38}, {76,38,57}, {38,0,19}, {88,19,28},
        {255,0,63}, {255,127,159}, {165,0,41}, {165,82,103}, {127,0,31}, {127,63,79}, {76,0,19}, {76,38,47},
        {38,0,9}, {88,19,23}, {0,0,0}, {101,101,101}, {102,102,102}, {153,153,153}, {204,204,204}, {255,255,255}
    };
    double r, g, b;
    color.Values(r, g, b, Quantity_TOC_sRGB);
    int best = 7;
    double bestDist = 1e9;
    for (int i = 1; i < 256; ++i) {
        const double dr = r * 255 - palette[i][0], dg = g * 255 - palette[i][1], db = b * 255 - palette[i][2];
        const double d = dr * dr + dg * dg + db * db;
        if (d < bestDist) {
            bestDist = d;
            best = i;
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

// Początek encji: typ, warstwa, kolor i typ linii jak warstwa (BYLAYER).
void entity(DxfStream& dxf, const char* type, const std::string& layer)
{
    dxf.code(0, type);
    dxf.code(8, layer);
    dxf.code(6, "BYLAYER");
    dxf.code(62, 256);
}

void writeLine(DxfStream& dxf, const std::string& layer, const gp_Pnt& a, const gp_Pnt& b)
{
    if (a.Distance(b) < 1e-9)
        return;
    entity(dxf, "LINE", layer);
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
            entity(dxf, "CIRCLE", layer);
            dxf.point(10, c);
            dxf.code(40, circ.Radius());
            return;
        }
        // ARC w DXF idzie zawsze przeciwnie do wskazówek zegara (patrząc z góry),
        // od kąta początkowego do końcowego. Łuk "od dołu" (oś w -Z) zamieniamy końcami.
        gp_Pnt a = curve.Value(first), b = curve.Value(last);
        if (nz < 0)
            std::swap(a, b);
        entity(dxf, "ARC", layer);
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
    dxf.code(2, "LTYPE");
    dxf.code(70, 1);
    dxf.code(0, "LTYPE");
    dxf.code(2, "CONTINUOUS");
    dxf.code(70, 0);
    dxf.code(3, "Solid line");
    dxf.code(72, 65);
    dxf.code(73, 0);
    dxf.code(40, 0.0);
    dxf.code(0, "ENDTAB");
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
