#include "ArdImport.h"
#include "ArdExport.h"
#include "ToolSides.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void word(std::string& s, size_t p, uint32_t v) {
    for (int i = 0; i < 4; ++i) s.at(p+i) = char(v >> (i*8));
}
uint32_t word(const std::string& s, size_t p) {
    uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= uint32_t(uint8_t(s.at(p+i))) << (i*8); return v;
}
void real(std::string& s, size_t p, double v) {
    uint64_t bits; std::memcpy(&bits, &v, 8);
    word(s,p,uint32_t(bits)); word(s,p+4,uint32_t(bits >> 32));
}
void rejects(const std::string& data) {
    bool failed = false;
    try { camcore::readArd(data); } catch (const std::runtime_error&) { failed = true; }
    check(failed, "invalid ARD accepted");
}
size_t pathStart(const std::string& s) {
    size_t p = 29; // header + empty thumbnail + variables tag
    while (auto n = word(s,p)) p += 4 + 2*n + 4;
    p += 4;
    check(uint8_t(s.at(p)) == 2, "test path location"); return p+1;
}
std::string curved(std::string s, bool clockwise, bool fullCircle) {
    const size_t p = pathStart(s), e = p+337;
    s.at(e) = 2;
    word(s,e+1,clockwise ? 0x610 : 0x600);
    const double endX = fullCircle ? 1 : 0;
    const double endY = fullCircle ? 0 : (clockwise ? -1 : 1);
    real(s,e+29,endX); real(s,e+37,endY);
    s.insert(e+45,64,'\0');
    real(s,e+45,0); real(s,e+53,0); real(s,e+61,1);
    real(s,e+101,fullCircle ? 360 : 90);
    word(s,s.size()-5,word(s,s.size()-5)+64);
    return s;
}
}

int main(int argc, char** argv) {
    try {
        camcore::LayerList layers;
        layers.createLayer("Zażółć gęślą jaźń", Quantity_Color(.2,.4,.6,Quantity_TOC_sRGB));
        camcore::ArdPath path; path.layerId = 1; path.zBottom = -3; path.zTop = 7;
        path.segments.push_back({1,0,0,1});
        auto data = camcore::writeArd(layers,{path},{ });
        auto doc = camcore::readArd(data);
        check(doc.sourceBytes == data, "original bytes not retained");
        check(doc.geometries.size() == 1 && doc.lineCount == 1, "line not imported");
        check(!doc.geometries[0].toolDirectionKnown, "invented CAM direction");
        auto edited = doc.geometries;
        camcore::DirectionChange change;
        camcore::applyDirectionChange(edited, {}, change, {});
        check(!edited[0].toolDirectionKnown, "no-op direction change reveals unknown direction");
        change.openSide = camcore::DirectionChange::OpenSide::Right;
        camcore::applyDirectionChange(edited, {}, change, {});
        check(edited[0].toolDirectionKnown, "explicit direction change hidden");
        check(doc.sourceLayers.size() == 9, "layers not read");
        check(doc.geometries[0].layer == "Zażółć gęślą jaźń", "CP1250 layer name");
        check(doc.layers.find(doc.geometries[0].layer)->color.Distance(layers.all()[1].color) < .001,
              "RGB color not retained");
        const auto& c = doc.geometries[0].contour;
        check(c.zTop == 7 && c.zBottom == -3, "Z attributes not imported");
        BRepAdaptor_Curve line(c.geometry[0]);
        check(line.Value(line.FirstParameter()).Distance(gp_Pnt(1,0,7)) < 1e-7, "3D edge position");
        check(BRepCheck_Analyzer(c.wire).IsValid(), "invalid wire");
        for (bool clockwise : {false,true}) {
            auto curve = camcore::readArd(curved(data,clockwise,false));
            check(curve.arcCount == 1 && curve.circleCount == 0, "arc counts");
            BRepAdaptor_Curve arc(curve.geometries[0].contour.geometry[0]);
            const double q = std::sqrt(.5);
            check(arc.Value((arc.FirstParameter()+arc.LastParameter())/2)
                .Distance(gp_Pnt(q,clockwise ? -q : q,7)) < 1e-6, "wrong arc orientation");
        }
        auto circle = camcore::readArd(curved(data,true,true));
        check(circle.circleCount == 1 && circle.geometries[0].contour.diameter == 2, "circle not imported");
        check(camcore::readArd(camcore::writeArd(layers,{},{})).geometries.empty(), "empty drawing");
        for (size_t n = 0; n < data.size(); n += 13) rejects(data.substr(0,n));
        auto bad = data; bad[23] = char(0xfe); rejects(bad);
        bad = data; word(bad,pathStart(bad)+221,9999); rejects(bad);
        bad = data; real(bad,pathStart(bad)+337+13,std::numeric_limits<double>::quiet_NaN()); rejects(bad);
        bad = data; bad.back() = 1; rejects(bad);
        bad = data + "junk"; rejects(bad);
        auto arcBad = curved(data,false,false); real(arcBad,pathStart(arcBad)+337+101,180); rejects(arcBad);
        // Auxiliary list and embedded cache: retained, explicitly not rendered.
        std::string auxiliary(145,'\0'); auxiliary[0] = 1;
        word(auxiliary,1,2); word(auxiliary,5,1); word(auxiliary,9,1);
        word(auxiliary,45,1); word(auxiliary,53,0x31);
        auto withAuxiliary = data;
        withAuxiliary.insert(pathStart(data)-1,auxiliary);
        word(withAuxiliary,withAuxiliary.size()-5,word(data,data.size()-5)+uint32_t(auxiliary.size()));
        check(!camcore::readArd(withAuxiliary).warnings.empty(), "auxiliary objects not reported");
        bad = withAuxiliary; word(bad,pathStart(data)-1+53,0xbad); rejects(bad);
        auto withCache = data;
        std::string cache(8,'\0'); cache[0] = 0x3e; word(cache,1,3);
        withCache.insert(withCache.size()-6,cache);
        check(!camcore::readArd(withCache).warnings.empty(), "cache not reported");
        bad = withCache; word(bad,bad.size()-6-8+1,0xffffffff); rejects(bad);
        std::string fragment(12,'\0'); word(fragment,0,3); // v3, no source descriptions
        const std::string parasolid = "**PARASOLID FORMAT=binary;";
        word(fragment,8,uint32_t(parasolid.size())); fragment += parasolid;
        fragment.append(12,'\0'); word(fragment,fragment.size()-12,1);
        auto withSolid = camcore::writeArd(layers,{path},{},fragment);
        auto solid = camcore::readArd(withSolid);
        check(solid.solids.size() == 1 && solid.solids[0].bodyCount == 1, "solid not detected");
        check(solid.solids[0].record == fragment && solid.solids[0].parasolid == parasolid,
              "solid bytes changed");
        check(!solid.warnings.empty(), "unconverted solid not reported");
        word(fragment,8,0xffffffff); rejects(camcore::writeArd(layers,{path},{},fragment));
        std::cout << "PASS: lines, layers, CP1250, RGB, Z, CW/CCW arcs, circle, solid extraction and malformed files\n";
        if (argc > 1) {
            std::ifstream file(argv[1],std::ios::binary);
            check(bool(file), "sample not readable");
            std::string bytes((std::istreambuf_iterator<char>(file)),{});
            auto sample = camcore::readArd(bytes);
            check(sample.geometries.size()==35 && sample.lineCount==46 && sample.arcCount==25
                && sample.circleCount==1 && sample.solids.size()==1, "unexpected sample counts");
            for (const auto& g : sample.geometries)
                check(BRepCheck_Analyzer(g.contour.wire).IsValid(), "sample has an invalid wire");
            check(sample.layers.find("Zamknięte kontury") != nullptr, "mixed UTF-8 layer name");
            std::cout << "PASS sample: " << sample.geometries.size() << " paths, "
                      << sample.lineCount << " lines, " << sample.arcCount << " arcs, "
                      << sample.circleCount << " circle, " << sample.sourceLayers.size()
                      << " layers, " << sample.solids[0].bodyCount << " unconverted solid(s)\n";
            for (const auto& warning : sample.warnings) std::cout << "LIMITATION: " << warning << '\n';
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
