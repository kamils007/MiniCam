#include "ModelExport.h"
#include "ModelImport.h"
#include <BRepBndLib.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <Bnd_Box.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <cmath>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    camcore::ImportedModel current;
    current.shape = BRepPrimAPI_MakeBox(12, 23, 4).Shape();
    gp_Trsf placement;
    placement.SetTranslation(gp_Vec(100, 200, 30));
    current.shape.Location(TopLoc_Location(placement));
    camcore::exportModel(current, argv[1]);
    const auto restored = camcore::importModel(argv[1]);
    Bnd_Box box;
    BRepBndLib::Add(restored.shape, box);
    double values[6];
    box.Get(values[0], values[1], values[2], values[3], values[4], values[5]);
    const double expected[] = {100, 200, 30, 112, 223, 34};
    for (int i = 0; i < 6; ++i)
        if (std::abs(values[i] - expected[i]) > 1e-5)
            return 1;
    std::cout << "Current edited placement survives STEP snapshot and reimport\n";
}
