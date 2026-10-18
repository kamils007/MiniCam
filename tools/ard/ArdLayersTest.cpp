#include "ArdExport.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
uint32_t word(const std::string& data, size_t at)
{
    if (at > data.size() || data.size() - at < 4)
        throw std::runtime_error("Truncated ARD word");
    uint32_t result = 0;
    for (int i = 0; i < 4; ++i)
        result |= uint32_t(uint8_t(data[at + i])) << (8 * i);
    return result;
}

size_t textLength(const std::string& data, size_t at)
{
    return uint8_t(data.at(at)) | (uint32_t(uint8_t(data.at(at + 1))) << 8);
}

void require(bool condition, const char* detail)
{
    if (!condition)
        throw std::runtime_error(detail);
}

void check(size_t users)
{
    camcore::LayerList layers;
    if (users) {
        layers.createLayer("userKonturZew");
        layers.createLayer("userOtwory");
    }
    // Document evidence: two user layer IDs are 1 and 2, APS is 3.
    // Use >10 geometries, repeated/non-sequential layers and an empty path;
    // confusing a geometry number with its layer used to hit default layers
    // and eventually non-existent IDs.
    const std::vector<uint32_t> expected = users
        ? std::vector<uint32_t>{3, 1, 2, 2, 1, 3, 1, 2, 1, 3, 2, 1}
        : std::vector<uint32_t>(12, 1);
    require(camcore::ardLayerId(layers, "Geometrie APS") == (users ? 3u : 1u), "APS mapping changed");
    require(camcore::ardLayerId(layers, "missing") == (users ? 3u : 1u), "Unknown layer must fall back to APS");
    if (users) {
        require(camcore::ardLayerId(layers, "userKonturZew") == 1, "Outer layer mapping changed");
        require(camcore::ardLayerId(layers, "userOtwory") == 2, "Hole layer mapping changed");
    }
    std::vector<camcore::ArdPath> paths(1); // this empty path must not consume an ID
    for (size_t i = 0; i < expected.size(); ++i) {
        camcore::ArdPath path;
        path.layerId = expected[i];
        path.zTop = 18;
        path.segments = {{double(i), 0, double(i) + 1, 0}};
        paths.push_back(path);
    }
    const std::string data = camcore::writeArd(layers, paths, {});
    size_t at = 23;
    require(uint8_t(data.at(at)) == 0x30, "Missing thumbnail");
    at += 5 + word(data, at + 1);
    require(uint8_t(data.at(at++)) == 0x1d, "Missing attribute names");
    while (const uint32_t count = word(data, at))
        at += 4 + size_t(count) * 2 + 4;
    at += 4;
    require(uint8_t(data.at(at++)) == 0x02, "Missing geometries");
    for (size_t i = 0; i < expected.size(); ++i) {
        require(word(data, at) == i + 1, "Geometry IDs must be unique and sequential");
        require(word(data, at + 221) == expected[i], "Geometry refers to the wrong layer ID");
        require(word(data, at + 52) == (i + 1 < expected.size() ? 1u : 0u), "Broken geometry list");
        at += 237;
        while (word(data, at)) {
            require(word(data, at + 4) == 5, "Unexpected attribute type in test");
            at += 16;
        }
        at += 4 + 48;
        require(uint8_t(data.at(at)) == 1, "Missing line");
        double x;
        const uint64_t bits = uint64_t(word(data, at + 13)) | (uint64_t(word(data, at + 17)) << 32);
        std::memcpy(&x, &bits, sizeof x);
        require(x == double(i), "Geometry coordinates changed");
        at += 49;
        require(data.at(at++) == 0, "Missing element terminator");
    }
    require(uint8_t(data.at(at++)) == 0x2e, "Missing document identifier");
    at += 2 + textLength(data, at);
    std::map<uint32_t, std::string> names;
    while (uint8_t(data.at(at)) == 0x10) {
        const uint32_t id = word(data, at + 1);
        const size_t count = textLength(data, at + 9);
        require(count > 0, "Invalid layer name");
        require(names.emplace(id, data.substr(at + 11, count - 1)).second, "Duplicate layer ID");
        at += 11 + count + 116;
    }
    require(names.at(users ? 3 : 1) == "GEOMETRIE APS", "APS record ID mismatch");
    if (users) {
        require(names.at(1) == "userKonturZew", "Outer layer record ID mismatch");
        require(names.at(2) == "userOtwory", "Hole layer record ID mismatch");
    }
    for (uint32_t id : expected)
        require(names.count(id) == 1, "Unresolved geometry layer");
    for (uint32_t invalid : {0u, static_cast<uint32_t>(users + 9)}) {
        camcore::ArdPath bad;
        bad.layerId = invalid;
        bad.segments = {{0, 0, 1, 1}};
        bool rejected = false;
        try {
            camcore::writeArd(layers, {bad}, {});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, "Non-existent geometry layer was accepted");
    }
}
}

int main()
{
    check(0);
    check(2);
    std::cout << "Geometry IDs and layer references verified independently against layer records\n";
}
