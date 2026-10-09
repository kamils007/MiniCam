#include "ModelImport.h"

#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <IGESControl_Reader.hxx>
#include <STEPControl_Reader.hxx>

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace {

std::string lowerExtension(const std::string& path)
{
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return {};
    std::string ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

TopoDS_Shape readStep(const std::string& path)
{
    STEPControl_Reader reader;
    if (reader.ReadFile(path.c_str()) != IFSelect_RetDone)
        throw std::runtime_error("Nie udało się odczytać pliku STEP.");
    reader.TransferRoots();
    return reader.OneShape();
}

TopoDS_Shape readIges(const std::string& path)
{
    IGESControl_Reader reader;
    if (reader.ReadFile(path.c_str()) != IFSelect_RetDone)
        throw std::runtime_error("Nie udało się odczytać pliku IGES.");
    reader.TransferRoots();
    return reader.OneShape();
}

TopoDS_Shape readBrep(const std::string& path)
{
    TopoDS_Shape shape;
    BRep_Builder builder;
    if (!BRepTools::Read(shape, path.c_str(), builder))
        throw std::runtime_error("Nie udało się odczytać pliku BREP.");
    return shape;
}

} // namespace

namespace camcore {

TopoDS_Shape importModel(const std::string& path)
{
    const std::string ext = lowerExtension(path);

    TopoDS_Shape shape;
    if (ext == "step" || ext == "stp")
        shape = readStep(path);
    else if (ext == "iges" || ext == "igs")
        shape = readIges(path);
    else if (ext == "brep" || ext == "brp")
        shape = readBrep(path);
    else
        throw std::runtime_error("Nieobsługiwany format pliku: ." + ext);

    if (shape.IsNull())
        throw std::runtime_error("Plik nie zawiera żadnej geometrii.");
    return shape;
}

} // namespace camcore
