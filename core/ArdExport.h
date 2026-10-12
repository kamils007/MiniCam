#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "Layers.h"

namespace camcore {

// Odcinek geometrii w płaszczyźnie XY [mm].
struct ArdSegment
{
    double x1, y1, x2, y2;
};

// Geometria Alphacama: ciąg odcinków po kolei wzdłuż konturu, na warstwie
// o danym identyfikatorze (zob. ardLayerId), z poziomami Z ścian.
struct ArdPath
{
    uint32_t layerId = 1; // bez warstw użytkownika 1 = GEOMETRIE APS
    double zBottom = 0, zTop = 0;
    std::vector<ArdSegment> segments;
};

// Identyfikator warstwy w pliku .ard, do którego odwołuje się geometria:
// warstwy użytkownika MiniCAM dostają 1…n (jak w Alphacamie), osiem warstw
// domyślnych Alphacama n + 1…n + 8; APS i nieznana nazwa → n + 1 (GEOMETRIE APS).
uint32_t ardLayerId(const LayerList& layers, const std::string& name);

// Zapisuje projekt w formacie Alphacam .ard ("Licom-APS File V1.94"): nagłówek,
// miniaturę JPEG, geometrie 2D (na razie same odcinki), warstwy i końcówkę pliku.
// Układ pliku odtworzony z plików zapisanych przez Alphacam (opis: FORMAT-ARD.md
// w plikach projektu). Bez bryły: Alphacam trzyma ją jako Parasolid XT, którego
// OCCT nie zapisuje.
// jpeg – gotowy plik JPEG miniatury (może być pusty).
// Zwraca zawartość pliku – zapis na dysk robi program (ścieżki z polskimi znakami).
std::string writeArd(const LayerList& layers, const std::vector<ArdPath>& paths,
                     const std::string& jpeg);

// Tekst UTF-8 → Windows-1250 (polskie litery), jak zapisuje go Alphacam.
// Znaki spoza strony kodowej zamienia na '?'.
std::string utf8ToCp1250(const std::string& text);

} // namespace camcore
