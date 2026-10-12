#pragma once
#include <string>

#include "ModelImport.h"

namespace camcore {

// Zapisuje bryłę (w bieżącym położeniu, z kolorami) do pliku STEP (.step/.stp)
// albo BREP (.brep/.brp, bez kolorów). W razie błędu rzuca std::runtime_error.
void exportModel(const ImportedModel& model, const std::string& path);

} // namespace camcore
