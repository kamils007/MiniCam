#include "ArdImport.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <Standard_Failure.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace camcore {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr size_t maxFileSize = 128 * 1024 * 1024;

class Reader {
public:
    explicit Reader(const std::string& data) : data(data) {}
    size_t pos = 0;
    const std::string& data;
    [[noreturn]] void fail(const std::string& what) const {
        throw std::runtime_error("ARD: " + what + " (bajt " + std::to_string(pos) + ").");
    }
    void need(size_t n) const { if (n > data.size() - pos) fail("Niepelny plik"); }
    void skip(size_t n) { need(n); pos += n; }
    uint8_t u8() { need(1); return static_cast<uint8_t>(data[pos++]); }
    uint16_t u16() { const auto lo = u8(); return lo | uint16_t(u8()) << 8; }
    uint32_t u32() { const auto lo = u16(); return lo | uint32_t(u16()) << 16; }
    double f64() {
        uint64_t v = u32(); v |= uint64_t(u32()) << 32;
        double d; std::memcpy(&d, &v, sizeof d);
        if (!std::isfinite(d) || std::abs(d) > 1e12) fail("Nieprawidlowa liczba");
        return d;
    }
    std::string take(size_t n) { need(n); auto s = data.substr(pos, n); pos += n; return s; }
    std::string terminated(size_t n) {
        if (!n || n > 65536) fail("Nieprawidlowa dlugosc tekstu");
        auto s = take(n);
        if (s.back() != 0 || s.find('\0') != n - 1) fail("Nieprawidlowe zakonczenie tekstu");
        s.pop_back(); return s;
    }
    bool flag() { auto v = u32(); if (v > 1) fail("Nieznany znacznik listy"); return v != 0; }
};

void utf8(std::string& s, uint32_t c) {
    if (c < 0x80) s += char(c);
    else if (c < 0x800) { s += char(0xc0 | (c >> 6)); s += char(0x80 | (c & 63)); }
    else { s += char(0xe0 | (c >> 12)); s += char(0x80 | ((c >> 6) & 63)); s += char(0x80 | (c & 63)); }
}

std::string cp1250(const std::string& s) {
    // The sample mixes legacy ANSI names and UTF-8 names in the same table.
    // Prefer UTF-8 only when the complete byte string is a valid encoding.
    bool validUtf8 = true;
    for (size_t i = 0; i < s.size() && validUtf8;) {
        const auto c = static_cast<unsigned char>(s[i++]);
        if (c < 128) continue;
        const size_t n = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2
            : c >= 0xf0 && c <= 0xf4 ? 3 : 0;
        if (!n || n > s.size() - i) { validUtf8 = false; break; }
        uint32_t code = c & (0x7f >> n);
        for (size_t j = 0; j < n; ++j) {
            const auto next = static_cast<unsigned char>(s[i++]);
            if ((next & 0xc0) != 0x80) validUtf8 = false;
            code = (code << 6) | (next & 63);
        }
        if (code < (n == 1 ? 0x80u : n == 2 ? 0x800u : 0x10000u)
            || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) validUtf8 = false;
    }
    if (validUtf8) return s;
    // Full Windows-1250 table; undefined bytes are represented by U+FFFD.
    static const uint16_t table[] = {
        0x20ac,0xfffd,0x201a,0xfffd,0x201e,0x2026,0x2020,0x2021,0xfffd,0x2030,0x160,0x2039,0x15a,0x164,0x17d,0x179,
        0xfffd,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0xfffd,0x2122,0x161,0x203a,0x15b,0x165,0x17e,0x17a,
        0xa0,0x2c7,0x2d8,0x141,0xa4,0x104,0xa6,0xa7,0xa8,0xa9,0x15e,0xab,0xac,0xad,0xae,0x17b,
        0xb0,0xb1,0x2db,0x142,0xb4,0xb5,0xb6,0xb7,0xb8,0x105,0x15f,0xbb,0x13d,0x2dd,0x13e,0x17c,
        0x154,0xc1,0xc2,0x102,0xc4,0x139,0x106,0xc7,0x10c,0xc9,0x118,0xcb,0x11a,0xcd,0xce,0x10e,
        0x110,0x143,0x147,0xd3,0xd4,0x150,0xd6,0xd7,0x158,0x16e,0xda,0x170,0xdc,0xdd,0x162,0xdf,
        0x155,0xe1,0xe2,0x103,0xe4,0x13a,0x107,0xe7,0x10d,0xe9,0x119,0xeb,0x11b,0xed,0xee,0x10f,
        0x111,0x144,0x148,0xf3,0xf4,0x151,0xf6,0xf7,0x159,0x16f,0xfa,0x171,0xfc,0xfd,0x163,0x2d9
    };
    std::string result;
    for (unsigned char c : s) utf8(result, c < 128 ? c : table[c - 128]);
    return result;
}

using Attributes = std::map<uint32_t, double>;
Attributes attributes(Reader& r) {
    Attributes result;
    for (size_t count = 0; ; ++count) {
        const auto id = r.u32();
        if (!id) return result;
        if (count >= 4096 || result.count(id)) r.fail("Nieprawidlowa lista atrybutow");
        const auto type = r.u32();
        if (type == 5) result[id] = r.f64();
        else if (type == 3) result[id] = static_cast<int32_t>(r.u32());
        else r.fail("Nieobslugiwany typ atrybutu " + std::to_string(type));
    }
}

struct Path {
    uint32_t id = 0, layer = 0, flags = 0;
    Geometry geometry;
};

double attribute(const Attributes& attrs, const std::map<uint32_t, std::string>& names,
                 const std::string& name) {
    for (const auto& [id, value] : attrs) {
        const auto it = names.find(id);
        if (it != names.end() && it->second == name) return value;
    }
    return 0;
}

void readPaths(Reader& r, ArdDocument& doc, std::vector<Path>& paths,
               const std::map<uint32_t, std::string>& names, bool& degenerate) {
    std::set<uint32_t> ids;
    bool more;
    do {
        if (paths.size() >= 100000) r.fail("Za duzo geometrii");
        const size_t start = r.pos;
        Path path; path.id = r.u32(); path.flags = r.u32();
        if (!path.id || !ids.insert(path.id).second) r.fail("Powtorzony identyfikator geometrii");
        r.skip(12);
        for (int i = 0; i < 4; ++i) r.f64(); // source bounding box, not a substitute for geometry
        more = r.flag(); r.skip(4 + 161);
        path.layer = r.u32(); r.skip(12);
        const auto attrs = attributes(r);
        auto& contour = path.geometry.contour;
        contour.zBottom = attribute(attrs, names, "LicomUKDMBGeoZLevelBottom");
        contour.zTop = attribute(attrs, names, "LicomUKDMBGeoZLevelTop");
        r.skip(48);
        BRepBuilderAPI_MakeWire wire;
        size_t count = 0;
        while (const auto tag = r.u8()) {
            if (++count > 1000000) r.fail("Za duzo elementow");
            const auto flags = r.u32();
            const double elementZ = r.f64();
            if (std::abs(elementZ) > 1e-9) r.fail("Nieobslugiwane polozenie elementu 3D");
            const double x1 = r.f64(), y1 = r.f64(), x2 = r.f64(), y2 = r.f64();
            const gp_Pnt a(x1, y1, contour.zTop), b(x2, y2, contour.zTop);
            TopoDS_Edge edge;
            if (tag == 1) {
                ++doc.lineCount;
                if (a.Distance(b) > 1e-7) edge = BRepBuilderAPI_MakeEdge(a, b).Edge();
                else degenerate = true;
            } else if (tag == 2) {
                const double cx = r.f64(), cy = r.f64(), radius = r.f64();
                for (int i = 0; i < 4; ++i) r.f64(); // cached tangents
                const double angle = r.f64();
                if (radius <= 1e-7 || angle <= 0 || angle > 360.000001) r.fail("Nieprawidlowy luk");
                const gp_Pnt center(cx, cy, contour.zTop);
                if (std::abs(center.Distance(a) - radius) > 1e-5 * std::max(1.0, radius)
                    || std::abs(center.Distance(b) - radius) > 1e-5 * std::max(1.0, radius))
                    r.fail("Promien luku nie zgadza sie z koncami");
                const bool clockwise = (flags & 0x10) != 0;
                const double sweep = std::min(angle, 360.0) * pi / 180;
                const double startAngle = std::atan2(y1 - cy, x1 - cx);
                const double endAngle = startAngle + (clockwise ? -sweep : sweep);
                if (gp_Pnt(cx + radius * std::cos(endAngle), cy + radius * std::sin(endAngle), contour.zTop)
                        .Distance(b) > 1e-5 * std::max(1.0, radius))
                    r.fail("Nieobslugiwany kierunek luku");
                const gp_Ax2 axis(center, gp_Dir(0, 0, clockwise ? -1 : 1), gp_Dir(gp_Vec(center, a)));
                edge = BRepBuilderAPI_MakeEdge(gp_Circ(axis, radius), 0, sweep).Edge();
                if (angle >= 359.999999) { ++doc.circleCount; contour.diameter = 2 * radius; }
                else ++doc.arcCount;
            } else r.fail("Nieobslugiwany element " + std::to_string(tag));
            attributes(r); // retained verbatim in sourceBytes, not applied as CAM settings
            if (!edge.IsNull()) {
                wire.Add(edge);
                if (!wire.IsDone()) r.fail("Rozlaczone elementy konturu");
                contour.geometry.push_back(edge);
            }
        }
        if (contour.geometry.empty()) r.fail("Geometria bez poprawnych elementow");
        contour.wire = wire.Wire();
        if (contour.geometry.size() != 1) contour.diameter = 0;
        Bnd_Box bounds; BRepBndLib::Add(contour.wire, bounds);
        double x0,y0,z0,x3,y3,z3; bounds.Get(x0,y0,z0,x3,y3,z3);
        contour.x = (x0+x3)/2; contour.y = (y0+y3)/2;
        contour.sizeX = x3-x0; contour.sizeY = y3-y0;
        // Tool direction/side fields are not decoded. Centre avoids inventing
        // a cutter compensation side for these imported drawing geometries.
        path.geometry.toolSide = ToolSide::Centre;
        path.geometry.toolDirectionKnown = false;
        paths.push_back(std::move(path));
        if (r.pos <= start) r.fail("Brak postepu odczytu");
    } while (more);
}

ArdSolidData solid(const std::string& bytes) {
    Reader r(bytes);
    if (r.u32() != 3) r.fail("Nieobslugiwana wersja rekordu bryly");
    const auto sources = r.u32();
    if (sources > 1024) r.fail("Za duzo zrodel bryly");
    for (uint32_t i = 0; i < sources; ++i) {
        r.u32(); auto n = r.u32(); r.terminated(n); n = r.u32(); r.terminated(n); r.skip(9);
    }
    const auto length = r.u32();
    ArdSolidData result; result.record = bytes; result.parasolid = r.take(length);
    if (result.parasolid.compare(0, 2, "**") != 0
        || result.parasolid.substr(0, 4096).find("FORMAT=binary") == std::string::npos)
        r.fail("Nieprawidlowy naglowek Parasolid");
    result.bodyCount = r.u32();
    return result;
}

// Reserved numbers are corroborated by the second number and source name.
// A user layer named KONSTRUKCJE has systemNumber == 0 and stays a user layer.
unsigned reservedLayer(const ArdLayerInfo& layer) {
    static const char* names[] = {"GEOMETRIE APS", "KONSTRUKCJE", "DROGI NARZĘDZIA",
                                  "WYMIARY", "SPLAJNY", "POWIERZCHNIE", "TEXT", ""};
    return layer.number >= 1 && layer.number <= 8
        && layer.systemNumber == layer.number
        && (layer.name == names[layer.number - 1] || (layer.number == 7 && layer.name == "GEOTEXT"))
        && !(layer.flags & 0x08) ? layer.number : 0;
}

} // namespace

ArdDocument readArd(const std::string& bytes) {
    if (bytes.size() > maxFileSize) throw std::runtime_error("ARD: Plik przekracza limit 128 MiB.");
    try {
        Reader r(bytes); ArdDocument doc; doc.sourceBytes = bytes;
        const std::string header = "Licom-APS File V1.94\r\n\x1a";
        if (r.take(header.size()) != header) r.fail("Nieobslugiwana wersja pliku");
        std::map<uint32_t, std::string> names;
        std::vector<Path> paths;
        std::map<uint32_t, std::string> layerNames;
        bool hadPaths = false, hadIdentifier = false, ended = false, degenerate = false;
        bool indexedColors = false, machiningAttributes = false;
        uint32_t identifierPos = 0;
        std::set<uint8_t> singleton;
        while (r.pos < bytes.size()) {
            const auto tag = r.u8();
            if (tag != 0x10 && tag != 0x22 && !singleton.insert(tag).second)
                r.fail("Powtorzony rekord " + std::to_string(tag));
            switch (tag) {
            case 0x30: { auto n = r.u32(); doc.thumbnail = r.take(n); break; }
            case 0x1d:
                for (size_t count = 0; ; ++count) {
                    auto n = r.u32(); if (!n) break;
                    if (n > 4096 || count > 4096) r.fail("Za duzo nazw atrybutow");
                    std::string name;
                    for (uint32_t i = 0; i < n; ++i) utf8(name, r.u16());
                    const auto id = r.u32();
                    if (!id || !names.emplace(id, name).second) r.fail("Powtorzona nazwa atrybutu");
                }
                break;
            case 0x01: {
                // Observed V1.94 auxiliary list: fixed header followed by
                // linked 92-byte records. No marker scanning/resynchronizing.
                r.u32(); r.u32(); r.u32();
                for (int i = 0; i < 4; ++i) r.f64();
                r.flag(); attributes(r);
                size_t count = 0; bool more;
                do {
                    if (++count > 100000) r.fail("Za duzo rekordow pomocniczych");
                    auto flags = r.u32();
                    if (flags != 0x31 && flags != 0x431) r.fail("Nieznany rekord pomocniczy 0x01");
                    r.skip(76); more = r.flag(); r.skip(8);
                } while (more);
                doc.warnings.push_back("Rekord pomocniczy 0x01: " + std::to_string(count)
                    + " obiekty zachowane w danych zrodlowych, bez odwzorowania w widoku.");
                break;
            }
            case 0x02:
                if (hadPaths) r.fail("Powtorzona lista geometrii");
                readPaths(r, doc, paths, names, degenerate); hadPaths = true; break;
            case 0x2e: {
                identifierPos = static_cast<uint32_t>(r.pos);
                auto n = r.u16(); r.terminated(n); hadIdentifier = true; break;
            }
            case 0x10: {
                ArdLayerInfo info; info.id = r.u32(); info.flags = r.u32();
                auto n = r.u16(); info.name = cp1250(r.terminated(n));
                info.number = r.u32(); info.systemNumber = r.u32(); info.color = r.u32(); r.skip(8);
                attributes(r); r.skip(92);
                if (!info.id || layerNames.count(info.id) || doc.sourceLayers.size() >= 10000)
                    r.fail("Nieprawidlowa lista warstw");
                layerNames[info.id] = {}; // validate uniqueness before resolving names
                doc.sourceLayers.push_back(std::move(info)); break;
            }
            case 0x15: r.skip(68); break; // text style, not text entities
            case 0x21: r.skip(8); break;
            case 0x22: {
                auto n = r.u16(); const auto name = r.terminated(n);
                const auto count = r.u32();
                if (count > 10000) r.fail("Za duzo obiektow dodatku");
                for (uint32_t i = 0; i < count; ++i) {
                    auto length = r.u32(); r.u32(); const auto record = r.take(length);
                    if (name == "LicomUKRJMParaSolid") doc.solids.push_back(solid(record));
                    else doc.warnings.push_back("Nieodwzorowany dodatek ARD: " + cp1250(name));
                }
                break;
            }
            case 0x3e: {
                auto n = r.u32(); r.skip(n);
                doc.warnings.push_back("Osadzona pamiec podreczna (0x3e) zachowana bez interpretacji."); break;
            }
            case 0x2f:
                if (!hadIdentifier || r.u32() != identifierPos || r.u8() != 0 || r.pos != bytes.size())
                    r.fail("Nieprawidlowa koncowka dokumentu");
                ended = true; break;
            default: r.fail("Nieobslugiwany rekord " + std::to_string(tag));
            }
        }
        if (!ended) r.fail("Brak koncowki dokumentu");
        std::set<std::string> usedNames{kApsLayer};
        auto color = [&](Layer& layer, const ArdLayerInfo& info) {
            if ((info.flags & 0x10) && info.color != 0xffffffff)
                layer.color = Quantity_Color((info.color & 255)/255.,
                    ((info.color >> 8) & 255)/255., ((info.color >> 16) & 255)/255., Quantity_TOC_sRGB);
            else if (info.color != 0xffffffff) indexedColors = true;
        };
        // System layers first, independent of the order of records in ARD.
        std::set<unsigned> reservedNumbers;
        for (const auto& info : doc.sourceLayers) {
            const auto number = reservedLayer(info);
            if (!number) continue;
            if (!reservedNumbers.insert(number).second) r.fail("Powtorzona warstwa systemowa");
            if (number == 8) continue; // unnamed text-style record, not a user layer
            auto& layer = doc.layers.createSystemLayer(static_cast<SystemLayer>(number - 1));
            usedNames.insert(layer.name); layerNames[info.id] = layer.name;
            color(layer, info);
        }
        for (const auto& info : doc.sourceLayers) {
            if (reservedLayer(info)) continue;
            std::string name = info.name.empty() ? "Warstwa ARD " + std::to_string(info.id) : info.name;
            if (usedNames.count(name)) name += " [ARD " + std::to_string(info.id) + "]";
            while (usedNames.count(name)) name += " [ARD " + std::to_string(info.id) + "]";
            usedNames.insert(name); layerNames[info.id] = name;
            color(doc.layers.createLayer(name), info);
        }
        for (auto& path : paths) {
            const auto layer = layerNames.find(path.layer);
            if (layer == layerNames.end()) r.fail("Geometria wskazuje nieistniejaca warstwe");
            if (layer->second.empty()) r.fail("Geometria wskazuje techniczny rekord stylu tekstu");
            path.geometry.layer = layer->second;
            machiningAttributes |= path.flags != 0x08000002;
            doc.geometries.push_back(std::move(path.geometry));
        }
        if (degenerate) doc.warnings.push_back("Zerowe odcinki pozostawiono w danych zrodlowych; nie tworza krawedzi widoku.");
        if (indexedColors) doc.warnings.push_back("Kolory z palety ALPHACAM nie sa odwzorowane; zastosowano kolory MiniCam.");
        if (machiningAttributes) doc.warnings.push_back("Ustawienia obrobki, kierunki, strona narzedzia i widocznosc ARD nie sa odtworzone.");
        if (!doc.solids.empty()) doc.warnings.push_back(kArdSolidConversionWarning);
        return doc;
    } catch (const Standard_Failure& e) {
        throw std::runtime_error(std::string("ARD: Nieprawidlowa geometria: ") + e.GetMessageString());
    }
}
} // namespace camcore
