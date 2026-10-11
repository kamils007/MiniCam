#pragma once

#include <QString>

#include <vector>

// Czcionka "z jednej linii" jak na starych komputerach i ploterach: każda litera to kilka
// odcinków (Hershey Simplex, 1967), bez wypełnień. Do napisów rysowanych liniami w widoku 3D.
namespace retrofont {

struct Segment
{
    double x1, y1, x2, y2;
};

// Wysokość wiersza i linia bazowa w jednostkach czcionki (wielka litera ma 21 jednostek).
constexpr double kTop = -16;    // góra wiersza
constexpr double kBottom = 16;  // dół wiersza
constexpr double kCapTop = -12; // góra wielkiej litery
constexpr double kBaseline = 9; // linia bazowa

// Odcinki napisu od x = 0 w prawo, y w dół (jak na ekranie). Zwraca szerokość napisu.
// Znaki spoza ASCII i polskich liter rysuje jako "?".
double layout(const QString& text, std::vector<Segment>& out);
// Sama szerokość napisu.
double width(const QString& text);

} // namespace retrofont
