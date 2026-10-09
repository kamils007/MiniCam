#pragma once
#include <gp_Vec.hxx>
#include "ModelImport.h"

namespace camcore {

// Gdzie ma wylądować bryła względem punktu 0,0,0.
enum class AlignMode
{
    BottomCorner, // lewy przedni dolny róg w 0,0,0 – bryła "leży na stole"
    TopCorner,    // lewy przedni róg w 0,0, wierzch na Z=0
    CenterBottom, // środek w X/Y w 0,0, spód na Z=0
};

// Przesuwa bryłę (bez obracania) według prostopadłościanu, który ją otacza.
// Zwraca wektor przesunięcia – przydaje się do pokazania, o ile przesunięto.
gp_Vec alignModel(ImportedModel& model, AlignMode mode);

} // namespace camcore
