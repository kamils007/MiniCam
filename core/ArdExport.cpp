#include "ArdExport.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <utility>
#include <limits>
#include <stdexcept>

namespace camcore {

namespace {

// Bajty zapisujemy po kolei, liczby little-endian – jak Alphacam na Windows.
class Writer
{
public:
    void u8(uint8_t v) { m_data.push_back(static_cast<char>(v)); }
    void u16(uint16_t v)
    {
        u8(static_cast<uint8_t>(v));
        u8(static_cast<uint8_t>(v >> 8));
    }
    void u32(uint32_t v)
    {
        u16(static_cast<uint16_t>(v));
        u16(static_cast<uint16_t>(v >> 16));
    }
    void i32(int32_t v) { u32(static_cast<uint32_t>(v)); }
    void f64(double v)
    {
        uint64_t bits;
        std::memcpy(&bits, &v, sizeof bits);
        u32(static_cast<uint32_t>(bits));
        u32(static_cast<uint32_t>(bits >> 32));
    }
    void f32(float v)
    {
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof bits);
        u32(bits);
    }
    void bytes(const std::string& s) { m_data += s; }
    void zeros(size_t n) { m_data.append(n, '\0'); }
    // str16: u16 długość razem z kończącym zerem + tekst cp1250 + zero.
    void str16(const std::string& utf8)
    {
        const std::string s = utf8ToCp1250(utf8);
        u16(static_cast<uint16_t>(s.size() + 1));
        bytes(s);
        u8(0);
    }
    // Nazwa zmiennej w rekordzie 0x1D: u32 liczba znaków + UTF-16LE (tylko ASCII).
    void utf16(const std::string& ascii)
    {
        u32(static_cast<uint32_t>(ascii.size()));
        for (char c : ascii)
            u16(static_cast<uint8_t>(c));
    }

    size_t size() const { return m_data.size(); }
    std::string take() { return std::move(m_data); }

private:
    std::string m_data;
};

// Znaczniki rekordów (pierwszy bajt rekordu).
enum Tag : uint8_t {
    kTagEnd = 0x00,
    kTagPath = 0x02,
    kTagLayer = 0x10,
    kTagTextStyle = 0x15,
    kTagDataStart = 0x21,
    kTagVariables = 0x1D,
    kTagIdentifier = 0x2E,
    kTagBackPointer = 0x2F,
    kTagThumbnail = 0x30,
};

// Warstwy, które Alphacam (wersja polska) zapisuje w każdym pliku: nazwa i kolor
// (-1 = domyślny, inaczej indeks koloru Alphacam). Ostatnia jest bez nazwy;
// rekord stylu tekstu (0x15) wskazuje jej numer.
struct DefaultLayer
{
    const char* name;
    int32_t color;
};
const DefaultLayer kDefaultLayers[] = {
    {"GEOMETRIE APS", -1},
    {"KONSTRUKCJE", -1},
    {"DROGI NARZ\xC4\x98" "DZIA", -1}, // "DROGI NARZĘDZIA" w UTF-8
    {"WYMIARY", 31},
    {"SPLAJNY", 28},
    {"POWIERZCHNIE", -1},
    {"TEXT", 32},
    {"", -1},
};
// Identyfikatory warstw jak w pliku Alphacama z warstwami TEST1/TEST2: warstwy
// użytkownika mają 1…n, warstwy domyślne zaraz po nich (GEOMETRIE APS = n + 1).
// Warstwy użytkownika mają rodzaj 0x38 (jak utworzone w Alphacamie) i kolor RGB.
constexpr uint32_t kUserLayerKind = 0x38;

uint32_t userLayerCount(const LayerList& layers)
{
    return static_cast<uint32_t>(layers.all().size() - 1); // bez APS
}

// Identyfikator z rekordu 0x2E. Znaczenie nieznane; w plikach z Alphacama był
// taki sam w obu zapisach, więc zapisujemy go bez zmian.
const char* const kIdentifier = "pBIjARRGz2JjQAVHeYo2X9gE770a8RqPaovRhaQ9skkSAAAAAeYn";

// Atrybuty zapisywane w pliku: Alphacam numeruje nazwy w rekordzie 0x1D
// po kolei od 1, a geometrie odwołują się do tych numerów.
const char* const kAttributeNames[] = {
    "LicomUKDMBGeoZLevelBottom",  // 1: dół ścian geometrii
    "LicomUKDMBGeoZLevelTop",     // 2: góra ścian geometrii
    "LicomUKDMBTempElementDirX1", // 3–6: zapisuje je Alphacam, znaczenie nieznane
    "LicomUKDMBTempElementDirX2",
    "LicomUKDMBTempElementDirY1",
    "LicomUKDMBTempElementDirY2",
    "_LicomUKDMBTempPickZ",       // 7: Alphacam wpisuje tu górę ścian
};
constexpr uint32_t kAttrZBottom = 1, kAttrZTop = 2, kAttrPickZ = 7;
constexpr uint32_t kAttrTypeDouble = 5;
constexpr uint8_t kElementLine = 0x01;

// Jedna geometria z listy 0x02. Pola, których znaczenia nie znamy, mają wartości
// z plików Alphacama (kwadraty 100×100 i 40×40).
void writePath(Writer& w, const ArdPath& path, uint32_t number, bool hasNext)
{
    double xmin = 0, ymin = 0, xmax = 0, ymax = 0;
    for (size_t i = 0; i < path.segments.size(); ++i) {
        const ArdSegment& s = path.segments[i];
        if (i == 0) {
            xmin = xmax = s.x1;
            ymin = ymax = s.y1;
        }
        for (const auto& [x, y] : {std::pair{s.x1, s.y1}, std::pair{s.x2, s.y2}}) {
            xmin = std::min(xmin, x);
            xmax = std::max(xmax, x);
            ymin = std::min(ymin, y);
            ymax = std::max(ymax, y);
        }
    }

    // The first word identifies this geometry, not its layer. ALPHACAM
    // renumbers it on save; the layer reference lives at byte offset 221.
    w.u32(number);
    w.u32(0x08000002);
    w.zeros(12);
    w.f64(xmin); // obrys geometrii
    w.f64(ymin);
    w.f64(xmax);
    w.f64(ymax);
    w.u32(hasNext ? 1 : 0); // po tej geometrii jest następna
    w.u32(1);
    w.zeros(161);
    w.u32(path.layerId); // odniesienie do ID rekordu warstwy 0x10
    w.zeros(12);
    for (const auto& [id, z] : {std::pair{kAttrZBottom, path.zBottom}, std::pair{kAttrZTop, path.zTop},
                                std::pair{kAttrPickZ, path.zTop}}) {
        w.u32(id);
        w.u32(kAttrTypeDouble);
        w.f64(z);
    }
    w.u32(0); // koniec atrybutów
    w.zeros(48);
    for (const ArdSegment& s : path.segments) {
        w.u8(kElementLine);
        w.u32(0x400);
        w.f64(0); // dla odcinka 0 (u łuku zapewne coś innego)
        w.f64(s.x1);
        w.f64(s.y1);
        w.f64(s.x2);
        w.f64(s.y2);
        w.zeros(4);
    }
    w.u8(0); // koniec elementów
}

// Warstwa domyślna Alphacama: identyfikator, rodzaj 0, nazwa, numer (dwa razy), kolor.
void writeDefaultLayer(Writer& w, uint32_t id, uint32_t number, const std::string& name, int32_t color)
{
    w.u8(kTagLayer);
    w.u32(id);
    w.u32(0);
    w.str16(name);
    w.u32(number);
    w.u32(number);
    w.i32(color);
    w.zeros(104);
}

// Warstwa użytkownika, jak TEST1/TEST2 z Alphacama: numer, kolor jako bajty
// R, G, B, 0, a pole 7 na pozycji 16 – znaczenie nieznane, przepisane.
void writeUserLayer(Writer& w, uint32_t id, uint32_t number, const Layer& layer)
{
    double r, g, b;
    layer.color.Values(r, g, b, Quantity_TOC_sRGB);
    auto byte = [](double v) { return static_cast<uint8_t>(std::clamp(v, 0.0, 1.0) * 255.0 + 0.5); };

    w.u8(kTagLayer);
    w.u32(id);
    w.u32(kUserLayerKind);
    w.str16(layer.name);
    w.u32(number);
    w.u32(0);
    w.u8(byte(r));
    w.u8(byte(g));
    w.u8(byte(b));
    w.u8(0);
    w.u32(0);
    w.u32(7);
    w.zeros(96);
}

} // namespace

std::string utf8ToCp1250(const std::string& text)
{
    struct Letter
    {
        uint32_t code;
        char cp1250;
    };
    static const Letter letters[] = {
        {0x104, '\xA5'}, {0x105, '\xB9'}, // Ą ą
        {0x106, '\xC6'}, {0x107, '\xE6'}, // Ć ć
        {0x118, '\xCA'}, {0x119, '\xEA'}, // Ę ę
        {0x141, '\xA3'}, {0x142, '\xB3'}, // Ł ł
        {0x143, '\xD1'}, {0x144, '\xF1'}, // Ń ń
        {0x0D3, '\xD3'}, {0x0F3, '\xF3'}, // Ó ó
        {0x15A, '\x8C'}, {0x15B, '\x9C'}, // Ś ś
        {0x179, '\x8F'}, {0x17A, '\x9F'}, // Ź ź
        {0x17B, '\xAF'}, {0x17C, '\xBF'}, // Ż ż
    };

    std::string out;
    for (size_t i = 0; i < text.size();) {
        const auto c = static_cast<uint8_t>(text[i]);
        if (c < 0x80) {
            out += static_cast<char>(c);
            ++i;
            continue;
        }
        // Wielobajtowy znak UTF-8: odczytujemy kod i szukamy go w tabeli.
        const size_t len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
        uint32_t code = c & (0xFF >> (len + 1));
        for (size_t k = 1; k < len && i + k < text.size(); ++k)
            code = (code << 6) | (static_cast<uint8_t>(text[i + k]) & 0x3F);
        i += len;
        char mapped = '?';
        for (const Letter& l : letters)
            if (l.code == code)
                mapped = l.cp1250;
        out += mapped;
    }
    return out;
}

uint32_t ardLayerId(const LayerList& layers, const std::string& name)
{
    const auto& all = layers.all();
    for (size_t i = 1; i < all.size(); ++i)
        if (all[i].name == name)
            return static_cast<uint32_t>(i);
    return userLayerCount(layers) + 1; // GEOMETRIE APS
}

std::string writeArd(const LayerList& layers, const std::vector<ArdPath>& paths,
                     const std::string& jpeg, const std::string& solidRecord)
{
    if (!solidRecord.empty() && (solidRecord.size() < 8
        || static_cast<unsigned char>(solidRecord[0]) != 3
        || solidRecord[1] != 0 || solidRecord[2] != 0 || solidRecord[3] != 0))
        throw std::invalid_argument("Nieprawidlowy rekord bryly Feature.dll.");
    if (solidRecord.size() > std::numeric_limits<uint32_t>::max())
        throw std::length_error("Rekord bryly jest za duzy dla ARD.");
    Writer w;
    w.bytes("Licom-APS File V1.94\r\n\x1A");

    w.u8(kTagThumbnail);
    w.u32(static_cast<uint32_t>(jpeg.size()));
    w.bytes(jpeg);

    w.u8(kTagVariables);
    int32_t number = 1;
    for (const char* name : kAttributeNames) {
        w.utf16(name);
        w.i32(number++);
    }
    w.u32(0); // koniec listy nazw

    // Lista geometrii: znacznik 0x02 raz, potem geometrie jedna po drugiej;
    // każda mówi, czy po niej jest następna.
    std::vector<const ArdPath*> written;
    const uint32_t maxLayerId = userLayerCount(layers) + static_cast<uint32_t>(std::size(kDefaultLayers));
    for (const ArdPath& path : paths) {
        if (!path.segments.empty()) {
            if (path.layerId == 0 || path.layerId > maxLayerId)
                throw std::invalid_argument("Geometria ARD wskazuje nieistniejaca warstwe.");
            written.push_back(&path);
        }
    }
    if (!written.empty()) {
        w.u8(kTagPath);
        for (size_t i = 0; i < written.size(); ++i)
            writePath(w, *written[i], static_cast<uint32_t>(i + 1), i + 1 < written.size());
    }

    w.u8(kTagIdentifier);
    const uint32_t identifierPos = static_cast<uint32_t>(w.size());
    w.str16(kIdentifier);

    // Najpierw warstwy domyślne Alphacama (APS to nasza warstwa APS), potem
    // warstwy użytkownika MiniCAM z ich kolorami.
    uint32_t layerNumber = 1;
    for (const DefaultLayer& l : kDefaultLayers) {
        writeDefaultLayer(w, userLayerCount(layers) + layerNumber, layerNumber, l.name, l.color);
        ++layerNumber;
    }
    for (size_t i = 1; i < layers.all().size(); ++i)
        writeUserLayer(w, ardLayerId(layers, layers.all()[i].name), layerNumber++, layers.all()[i]);

    w.u8(kTagTextStyle);
    w.u32(8);     // numer warstwy (ostatnia z domyślnych)
    w.f32(5.0f);  // prawdopodobnie wysokość tekstu
    w.f32(0.5f);
    w.zeros(56);

    w.u8(kTagDataStart);
    w.zeros(8);
    // Obwiednia 0x22 zgodna ze sprawdzonym plikiem TEST_6_ARD:
    // nazwa w str16, liczba obiektow, dlugosc rekordu, pole zarezerwowane.
    // Sam rekord pochodzi bez zmian z ObjectDoWrite2, lacznie z metadanymi bryly.
    if (!solidRecord.empty()) {
        w.u8(0x22);
        w.str16("LicomUKRJMParaSolid");
        w.u32(1);
        w.u32(static_cast<uint32_t>(solidRecord.size()));
        w.u32(0);
        w.bytes(solidRecord);
    }

    // Wskaźnik zwrotny: offset rekordu identyfikatora (pole długości), potem koniec.
    w.u8(kTagBackPointer);
    w.u32(identifierPos);
    w.u8(kTagEnd);
    return w.take();
}

} // namespace camcore

