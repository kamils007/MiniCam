// Offline reconstruction of the supported analytic Feature snapshot subset.
// No vendor runtime, approximation of curved surfaces, or export worker changes.
#include "FeatureGeometry.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepCheck_Result.hxx>
#include <BRepCheck_ListIteratorOfListOfStatus.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Plane.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_ConicalSurface.hxx>
#include <ShapeFix_Face.hxx>
#include <ShapeFix_Edge.hxx>
#include <STEPControl_Writer.hxx>
#include <STEPControl_Reader.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Shell.hxx>
#include <Standard_Failure.hxx>
#include <gp_Ax3.hxx>
#include <gp_Ax2.hxx>
#include <gp_Vec.hxx>
#include <map>
#include <set>
#include <vector>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <algorithm>

constexpr double tolerance=1e-6;
void require(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
int main(int argc, char** argv) {
 QCoreApplication app(argc,argv);
 try {
  QCommandLineParser parser;
  parser.addHelpOption();
  parser.addPositionalArgument("snapshot", "Feature geometry JSON");
  parser.addPositionalArgument("output", "New STEP output");
  parser.addOption({"report", "Validate native read report", "path"});
  parser.addOption({"source-sha256", "Expected source ARD hash", "hash"});
  parser.addOption({"payload-sha256", "Expected Parasolid hash", "hash"});
  parser.process(app);
  const auto args=parser.positionalArguments();
  require(args.size()==2,"Usage: ard_snapshot_to_occt snapshot.json NEW_output.step");
  QString output=args[1];
  require(!QFile::exists(output) && !QFile::exists(output+".brep"),"Output already exists");
  QFile input(args[0]); require(input.open(QIODevice::ReadOnly),"Cannot read snapshot");
  require(input.size()>0 && input.size()<32*1024*1024,"Snapshot size outside limit");
  const auto snapshot=input.readAll();TopoDS_Shape solid;
  if(parser.isSet("report")) {
   QFile report(parser.value("report"));require(report.open(QIODevice::ReadOnly) && report.size()<8*1024*1024,"Cannot read report");
   solid=ardimport::shapeFromReadResult(report.readAll(),snapshot,parser.value("source-sha256"),parser.value("payload-sha256"));
  } else solid=ardimport::shapeFromSnapshot(snapshot);
  GProp_GProps volume,area;BRepGProp::VolumeProperties(solid,volume);BRepGProp::SurfaceProperties(solid,area);
  TopTools_IndexedMapOfShape fm,em,vm;TopExp::MapShapes(solid,TopAbs_FACE,fm);TopExp::MapShapes(solid,TopAbs_EDGE,em);TopExp::MapShapes(solid,TopAbs_VERTEX,vm);
  Bnd_Box actualBox;BRepBndLib::AddOptimal(solid,actualBox,false,false);double aa[6];actualBox.Get(aa[0],aa[1],aa[2],aa[3],aa[4],aa[5]);
  STEPControl_Writer writer;require(writer.Transfer(solid,STEPControl_ManifoldSolidBrep)==IFSelect_RetDone,"STEP transfer failed");
  require(writer.Write(output.toLocal8Bit().constData())==IFSelect_RetDone,"STEP write failed");
  require(BRepTools::Write(solid,(output+".brep").toLocal8Bit().constData()),"BREP write failed");
  STEPControl_Reader reader;
  require(reader.ReadFile(output.toLocal8Bit().constData())==IFSelect_RetDone && reader.TransferRoots()==1,"STEP readback failed");
  auto restored=reader.OneShape();int solidCount=0;
  for(TopExp_Explorer ex(restored,TopAbs_SOLID);ex.More();ex.Next())++solidCount;
  require(solidCount==1 && BRepCheck_Analyzer(restored).IsValid(),"Invalid STEP readback solid");
  TopTools_IndexedMapOfShape rf,re,rv;TopExp::MapShapes(restored,TopAbs_FACE,rf);TopExp::MapShapes(restored,TopAbs_EDGE,re);TopExp::MapShapes(restored,TopAbs_VERTEX,rv);
  require(rf.Extent()==fm.Extent() && re.Extent()==em.Extent() && rv.Extent()==vm.Extent(),"STEP readback topology mismatch");
  GProp_GProps restoredVolume;BRepGProp::VolumeProperties(restored,restoredVolume);
  require(std::abs(restoredVolume.Mass()-volume.Mass())<=std::max(1e-5,volume.Mass()*1e-8),"STEP readback volume mismatch");
  QJsonObject report{{"occt_solid_valid",true},{"faces",fm.Extent()},{"edges",em.Extent()},{"vertices",vm.Extent()},
   {"area_mm2",area.Mass()},{"volume_mm3",volume.Mass()},{"volume_matches_original_verified",false},
   {"step_readback_valid",true},{"step_readback_volume_mm3",restoredVolume.Mass()},
   {"bounds_mm",QJsonArray{aa[0],aa[1],aa[2],aa[3],aa[4],aa[5]}}};
  std::cout<<QJsonDocument(report).toJson().constData();return 0;
 } catch(const Standard_Failure& e){std::cerr<<"OCCT: "<<e.GetMessageString()<<'\n';}
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';}return 1;
}
