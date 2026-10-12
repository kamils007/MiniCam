#include "ArdExport.h"

#include <cstdint>
#include <cstring>

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
constexpr uint32_t kFirstLayerId = 4; // identyfikatory warstw w pliku zaczynają się od 4

// Identyfikator z rekordu 0x2E. Znaczenie nieznane; w plikach z Alphacama był
// taki sam w obu zapisach, więc zapisujemy go bez zmian.
const char* const kIdentifier = "pBIjARRGz2JjQAVHeYo2X9gE770a8RqPaovRhaQ9skkSAAAAAeYn";

void writeLayer(Writer& w, uint32_t number, const std::string& name, int32_t color)
{
    w.u8(kTagLayer);
    w.u32(kFirstLayerId + number - 1);
    w.u32(0);
    w.str16(name);
    w.u32(number);
    w.u32(number);
    w.i32(color);
    w.zeros(104);
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

std::string writeArd(const LayerList& layers, const std::string& jpeg)
{
    Writer w;
    w.bytes("Licom-APS File V1.94\r\n\x1A");

    w.u8(kTagThumbnail);
    w.u32(static_cast<uint32_t>(jpeg.size()));
    w.bytes(jpeg);

    w.u8(kTagVariables);
    int32_t value = 1;
    for (const char* name : {"LicomUKDMBTempElementDirX1", "LicomUKDMBTempElementDirX2",
                             "LicomUKDMBTempElementDirY1", "LicomUKDMBTempElementDirY2"}) {
        w.utf16(name);
        w.i32(value++);
    }
    w.u32(0); // koniec listy zmiennych

    w.u8(kTagIdentifier);
    const uint32_t identifierPos = static_cast<uint32_t>(w.size());
    w.str16(kIdentifier);

    // Najpierw warstwy domyślne Alphacama (APS to nasza warstwa APS), potem
    // warstwy użytkownika MiniCAM. Kolory MiniCAM (RGB) nie mają jeszcze
    // odpowiednika w indeksach Alphacama, więc dostają kolor domyślny.
    uint32_t number = 1;
    for (const DefaultLayer& l : kDefaultLayers)
        writeLayer(w, number++, l.name, l.color);
    for (size_t i = 1; i < layers.all().size(); ++i)
        writeLayer(w, number++, layers.all()[i].name, -1);

    w.u8(kTagTextStyle);
    w.u32(8);     // numer warstwy (ostatnia z domyślnych)
    w.f32(5.0f);  // prawdopodobnie wysokość tekstu
    w.f32(0.5f);
    w.zeros(56);

    w.u8(kTagDataStart);
    w.zeros(8);
    // Tu Alphacam zapisuje bloki danych (0x22, np. bryła Parasolid) i zaszyfrowany
    // Cache.xml (0x3E) – na razie ich nie mamy.

    // Wskaźnik zwrotny: offset rekordu identyfikatora (pole długości), potem koniec.
    w.u8(kTagBackPointer);
    w.u32(identifierPos);
    w.u8(kTagEnd);
    return w.take();
}

} // namespace camcore
