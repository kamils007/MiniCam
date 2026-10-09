#include "ModelImport.h"

#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <IGESCAFControl_Reader.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDocStd_Document.hxx>
#include <TopoDS_Compound.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFPrs.hxx>
#include <XCAFPrs_IndexedDataMapOfShapeStyle.hxx>
#include <XCAFPrs_Style.hxx>

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

// Pusty dokument XCAF ("XDE") – pojemnik, w którym oprócz geometrii
// przechowywane są atrybuty z pliku: kolory, nazwy, struktura złożenia.
Handle(TDocStd_Document) newXcafDocument()
{
    Handle(TDocStd_Document) doc;
    XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", doc);
    return doc;
}

// Wyciąga z dokumentu geometrię i kolory.
camcore::ImportedModel modelFromDocument(const Handle(TDocStd_Document)& doc)
{
    const Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());

    // "Wolne" kształty to najwyższy poziom pliku: pojedyncze części lub całe złożenia.
    TDF_LabelSequence roots;
    shapeTool->GetFreeShapes(roots);

    camcore::ImportedModel model;
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);

    for (const TDF_Label& label : roots) {
        // GetShape zwraca kształt już ustawiony tam, gdzie jest w pliku –
        // niczego nie przesuwamy.
        const TopoDS_Shape shape = XCAFDoc_ShapeTool::GetShape(label);
        if (shape.IsNull())
            continue;
        builder.Add(compound, shape);

        // CollectStyleSettings zbiera kolory z całej struktury pod tą etykietą
        // (kolor części, bryły i pojedynczych ścian) razem z ich położeniem.
        XCAFPrs_IndexedDataMapOfShapeStyle styles;
        XCAFPrs::CollectStyleSettings(label, TopLoc_Location(), styles);
        for (XCAFPrs_IndexedDataMapOfShapeStyle::Iterator it(styles); it.More(); it.Next()) {
            if (it.Value().IsSetColorSurf())
                model.colors.push_back({it.Key(), it.Value().GetColorSurf()});
        }
    }

    // Jedna część – bez zbędnego opakowania w kompaund.
    model.shape = (roots.Length() == 1) ? XCAFDoc_ShapeTool::GetShape(roots.First())
                                        : TopoDS_Shape(compound);
    return model;
}

camcore::ImportedModel readStep(const std::string& path)
{
    STEPCAFControl_Reader reader;
    reader.SetColorMode(true);
    reader.SetNameMode(true);
    if (reader.ReadFile(path.c_str()) != IFSelect_RetDone)
        throw std::runtime_error("Nie udało się odczytać pliku STEP.");
    Handle(TDocStd_Document) doc = newXcafDocument();
    if (!reader.Transfer(doc))
        throw std::runtime_error("Nie udało się przetworzyć geometrii z pliku STEP.");
    return modelFromDocument(doc);
}

camcore::ImportedModel readIges(const std::string& path)
{
    IGESCAFControl_Reader reader;
    reader.SetColorMode(true);
    reader.SetNameMode(true);
    if (reader.ReadFile(path.c_str()) != IFSelect_RetDone)
        throw std::runtime_error("Nie udało się odczytać pliku IGES.");
    Handle(TDocStd_Document) doc = newXcafDocument();
    if (!reader.Transfer(doc))
        throw std::runtime_error("Nie udało się przetworzyć geometrii z pliku IGES.");
    return modelFromDocument(doc);
}

camcore::ImportedModel readBrep(const std::string& path)
{
    // BREP to "goła" geometria OCCT – nie zapisuje kolorów.
    camcore::ImportedModel model;
    BRep_Builder builder;
    if (!BRepTools::Read(model.shape, path.c_str(), builder))
        throw std::runtime_error("Nie udało się odczytać pliku BREP.");
    return model;
}

} // namespace

namespace camcore {

ImportedModel importModel(const std::string& path)
{
    const std::string ext = lowerExtension(path);

    ImportedModel model;
    if (ext == "step" || ext == "stp")
        model = readStep(path);
    else if (ext == "iges" || ext == "igs")
        model = readIges(path);
    else if (ext == "brep" || ext == "brp")
        model = readBrep(path);
    else
        throw std::runtime_error("Nieobsługiwany format pliku: ." + ext);

    if (model.shape.IsNull())
        throw std::runtime_error("Plik nie zawiera żadnej geometrii.");
    return model;
}

} // namespace camcore
