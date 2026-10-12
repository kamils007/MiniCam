#include "ModelExport.h"

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepTools.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPControl_StepModelType.hxx>
#include <TDF_Label.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>

namespace camcore {

void exportModel(const ImportedModel& model, const std::string& path)
{
    if (model.shape.IsNull())
        throw std::runtime_error("Brak bryły do zapisania.");

    std::string ext = path.substr(std::min(path.size(), path.find_last_of('.') + 1));
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (ext == "brep" || ext == "brp") {
        if (!BRepTools::Write(model.shape, path.c_str()))
            throw std::runtime_error("Nie udało się zapisać pliku BREP: " + path);
        return;
    }
    if (ext != "step" && ext != "stp")
        throw std::runtime_error("Nieobsługiwany format zapisu: ." + ext);

    // Dokument XCAF: bryła + kolory jej fragmentów (jak przy wczytywaniu).
    Handle(TDocStd_Document) doc;
    XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", doc);
    Handle(XCAFDoc_ShapeTool) shapes = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    Handle(XCAFDoc_ColorTool) colors = XCAFDoc_DocumentTool::ColorTool(doc->Main());
    // Położenie bryły (po wyrównaniu/przesunięciu) jest zapisane jako "location";
    // writer STEP z kolorami gubi je, więc wpisujemy je na stałe w geometrię.
    // Fragmenty z kolorami przenosimy na odpowiadające im fragmenty kopii.
    const TopLoc_Location loc = model.shape.Location();
    TopoDS_Shape shape = model.shape;
    std::unique_ptr<BRepBuilderAPI_Transform> baked;
    if (!loc.IsIdentity()) {
        baked = std::make_unique<BRepBuilderAPI_Transform>(model.shape.Located(TopLoc_Location()),
                                                           loc.Transformation(), Standard_True);
        shape = baked->Shape();
    }
    auto inFile = [&](const TopoDS_Shape& s) {
        return baked ? baked->ModifiedShape(s.Moved(loc.Inverted())) : s;
    };

    const TDF_Label main = shapes->AddShape(shape, Standard_False);
    for (const ShapeColor& c : model.colors) {
        const TopoDS_Shape sub = c.shape.IsSame(model.shape) ? shape : inFile(c.shape);
        TDF_Label label;
        if (sub.IsSame(shape))
            label = main;
        else if (!shapes->FindSubShape(main, sub, label))
            label = shapes->AddSubShape(main, sub);
        if (!label.IsNull())
            colors->SetColor(label, c.color, XCAFDoc_ColorSurf);
    }

    STEPCAFControl_Writer writer;
    writer.SetColorMode(Standard_True);
    writer.SetNameMode(Standard_True);
    const bool ok = writer.Transfer(doc, STEPControl_AsIs)
                    && writer.Write(path.c_str()) == IFSelect_RetDone;
    XCAFApp_Application::GetApplication()->Close(doc);
    if (!ok)
        throw std::runtime_error("Nie udało się zapisać pliku STEP: " + path);
}

} // namespace camcore
