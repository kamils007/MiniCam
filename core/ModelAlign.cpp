#include "ModelAlign.h"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Trsf.hxx>

namespace camcore {

gp_Vec alignModel(ImportedModel& model, AlignMode mode)
{
    // Prostopadłościan otaczający (bounding box) liczony z dokładnej geometrii,
    // a nie z siatki trójkątów – inaczej róg mógłby wyjść przesunięty o ułamek mm.
    Bnd_Box box;
    BRepBndLib::AddOptimal(model.shape, box, Standard_False, Standard_False);
    if (box.IsVoid())
        return gp_Vec();

    double xMin, yMin, zMin, xMax, yMax, zMax;
    box.Get(xMin, yMin, zMin, xMax, yMax, zMax);

    gp_Vec shift;
    switch (mode) {
    case AlignMode::BottomCorner:
        shift = gp_Vec(-xMin, -yMin, -zMin);
        break;
    case AlignMode::TopCorner:
        shift = gp_Vec(-xMin, -yMin, -zMax);
        break;
    case AlignMode::CenterBottom:
        shift = gp_Vec(-(xMin + xMax) / 2, -(yMin + yMax) / 2, -zMin);
        break;
    }

    // Przesunięcie zapisujemy jako "położenie" (location) kształtu – geometria
    // nie jest kopiowana, OCCT tylko pamięta, że całość jest przesunięta.
    gp_Trsf trsf;
    trsf.SetTranslation(shift);
    const TopLoc_Location loc(trsf);
    model.shape.Move(loc);
    // Kolory są przypięte do fragmentów bryły, więc przesuwamy je tak samo,
    // żeby dalej pasowały do przesuniętych ścian.
    for (ShapeColor& c : model.colors)
        c.shape.Move(loc);
    return shift;
}

} // namespace camcore
