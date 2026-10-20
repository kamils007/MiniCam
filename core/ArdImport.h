#pragma once

#include "Geometry.h"
#include <cstdint>
#include <string>
#include <vector>

namespace camcore {
inline constexpr const char* kArdSolidConversionWarning =
    "Bryly Parasolid nie zostaly wczytane do widoku 3D. Wymagany jest konwerter Parasolid do STEP/BREP.";

// A read-only import. Retain the source and unconverted objects; an ARD read is
// not a promise that saving through MiniCam can reproduce the whole document.
struct ArdLayerInfo {
    uint32_t id = 0, flags = 0, number = 0, color = 0;
    std::string name;
};

struct ArdSolidData {
    std::string record;     // complete Feature v3 object, including its metadata
    std::string parasolid;  // bounded X_B payload; OCCT cannot read it directly
    uint32_t bodyCount = 0;
};

struct ArdDocument {
    std::string sourceBytes;
    std::string thumbnail;
    LayerList layers;
    std::vector<ArdLayerInfo> sourceLayers;
    std::vector<Geometry> geometries;
    std::vector<ArdSolidData> solids;
    std::vector<std::string> warnings;
    size_t lineCount = 0, arcCount = 0, circleCount = 0;
};

// Recognized Licom-APS V1.94 records only. Bounds and numeric values are
// checked; unsupported layouts fail instead of scanning for marker bytes.
// The caller reads the file (e.g. QFile, for Unicode Windows paths).
ArdDocument readArd(const std::string& bytes);

} // namespace camcore
