#pragma once
#include <gp_Trsf.hxx>
#include "ModelImport.h"

namespace camcore {

// Ustawienia auto-wyrównania – odpowiadają oknu Konfiguracja → Auto-wyrównanie.
struct AlignSettings
{
    enum class ZZero { Top, Middle, Bottom };     // gdzie ma być Z=0
    enum class LongEdge { X, Y };                 // wzdłuż której osi najdłuższa krawędź
    enum class BaseX { Left, Center, Right };     // punkt bazowy w X
    enum class BaseY { Bottom, Center, Top };     // punkt bazowy w Y ("Dół" = przód)

    bool alignAfterImport = true; // wyrównaj od razu po wczytaniu pliku
    bool lathe = false;           // wyrównanie dla toczenia – jeszcze nieobsługiwane
    bool panel = true;            // obróć jak płytę: płaszczyzna o największej sumie pól na stół
    bool minimalBox = false;      // orientację licz z najmniejszego obróconego prostopadłościanu

    ZZero zZero = ZZero::Bottom;
    LongEdge longEdge = LongEdge::Y;
    BaseX baseX = BaseX::Left;
    BaseY baseY = BaseY::Bottom;
};

// Liczy przekształcenie (obrót + przesunięcie), które wyrównuje bryłę.
// flipped = true kładzie bryłę na przeciwną stronę (wierzchem do stołu).
gp_Trsf computeAlignment(const TopoDS_Shape& shape, const AlignSettings& settings,
                         bool flipped = false);

// Zwraca kopię modelu przekształconą przez trsf (kolory jadą razem z bryłą).
ImportedModel transformed(const ImportedModel& model, const gp_Trsf& trsf);

} // namespace camcore
