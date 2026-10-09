#pragma once
#include <string>
#include <TopoDS_Shape.hxx>

namespace camcore {

// Wczytuje bryłę z pliku STEP (.step/.stp), IGES (.iges/.igs) lub BREP (.brep).
// W razie błędu rzuca std::runtime_error.
TopoDS_Shape importModel(const std::string& path);

} // namespace camcore
