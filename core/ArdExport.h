#pragma once
#include <string>

#include "Layers.h"

namespace camcore {

// Zapisuje projekt w formacie Alphacam .ard ("Licom-APS File V1.94"), na razie
// bez bryły i bez geometrii: nagłówek, miniatura JPEG, warstwy i końcówka pliku.
// Układ pliku odtworzony z plików zapisanych przez Alphacam (opis: FORMAT-ARD.md
// w plikach projektu). Bryła siedzi w Alphacam jako Parasolid XT, którego OCCT
// nie zapisuje, a rekordów geometrii 2D jeszcze nie znamy.
// jpeg – gotowy plik JPEG miniatury (może być pusty).
// Zwraca zawartość pliku – zapis na dysk robi program (ścieżki z polskimi znakami).
std::string writeArd(const LayerList& layers, const std::string& jpeg);

// Tekst UTF-8 → Windows-1250 (polskie litery), jak zapisuje go Alphacam.
// Znaki spoza strony kodowej zamienia na '?'.
std::string utf8ToCp1250(const std::string& text);

} // namespace camcore
