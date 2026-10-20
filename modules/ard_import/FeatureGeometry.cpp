// Offline reconstruction of the supported analytic Feature snapshot subset.
// No vendor runtime, approximation of curved surfaces, or export worker changes.
#include "FeatureGeometry.h"
#include <QCryptographicHash>
#include <QCoreApplication>
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

namespace {
constexpr double tolerance = 1e-6;
const double pi = std::acos(-1.0);
void require(bool b, const char* message) { if (!b) throw std::runtime_error(message); }
double num(QJsonValue v) { require(v.isDouble() && std::isfinite(v.toDouble()), "Missing/nonfinite number"); return v.toDouble(); }
int id(QJsonValue v) { double d=num(v); require(d>=1 && d<=300000 && std::floor(d)==d,"Invalid ID"); return int(d); }
QJsonArray array(QJsonValue v) { require(v.isArray(),"Missing array"); return v.toArray(); }
gp_Vec vec(QJsonValue v) { auto a=array(v); require(a.size()==3,"Invalid triple"); return {num(a[0]),num(a[1]),num(a[2])}; }
gp_Pnt point(QJsonValue v) { auto a=vec(v); return {a.X(),a.Y(),a.Z()}; }
bool kind(QJsonObject o, const char* name) { return o["type"].toObject()["names"].toArray().contains(QString::fromLatin1(name)); }
struct Edge { TopoDS_Edge shape; int a,b; QJsonObject data; };

}

TopoDS_Shape ardimport::shapeFromSnapshot(const QByteArray& snapshot)
{
 try {
 require(!snapshot.isEmpty() && snapshot.size()<32*1024*1024,"Snapshot size outside limit");
  QJsonParseError parse; auto document=QJsonDocument::fromJson(snapshot,&parse);
  require(parse.error==QJsonParseError::NoError && document.isObject(),"Invalid JSON");
  auto root=document.object();
  require(root["schema"]=="minicam-feature-geometry-v1" && root["units"]=="mm" && root["topology_read_complete"].toBool(),"Unsupported/incomplete snapshot");
  std::map<int,gp_Pnt> points; std::map<int,TopoDS_Vertex> vertices; std::map<int,Edge> edges;
  for(auto v:array(root["vertices"])) {
   auto o=v.toObject();int n=id(o["id"]);auto p=point(o["point"]);
   require(points.emplace(n,p).second,"Duplicate vertex ID");vertices.emplace(n,BRepBuilderAPI_MakeVertex(p).Vertex());
  }
  require(!points.empty() && points.size()<=100000,"Invalid vertex count");
  for(auto v:array(root["edges"])) {
   auto o=v.toObject();auto refs=array(o["vertices"]);require(refs.size()==2,"Only two-ended edges supported");
   int a=id(refs[0]),b=id(refs[1]),n=id(o["id"]);auto pa=points.at(a),pb=points.at(b);
   double expected=num(o["length"]);require(expected>tolerance,"Degenerate edge");TopoDS_Edge shape;
   if(kind(o,"FeatureLinearEdge")) {
    require(std::abs(pa.Distance(pb)-expected)<=tolerance,"Line length mismatch");
    BRepBuilderAPI_MakeEdge mk(vertices.at(a),vertices.at(b));require(mk.IsDone(),"Cannot build line");shape=mk.Edge();
   } else if(kind(o,"FeatureCircularEdge")) {
    double r=num(o["radius"]);require(r>tolerance,"Invalid circle radius");auto center=point(o["center"]);
    gp_Dir normal(vec(o["plane"].toObject()["vector"]));gp_Vec va(center,pa),vb(center,pb);
    require(std::abs(va.Magnitude()-r)<=tolerance && std::abs(vb.Magnitude()-r)<=tolerance,"Circle radius/endpoints mismatch");
    require(std::abs(va.Dot(gp_Vec(normal)))<=tolerance && std::abs(vb.Dot(gp_Vec(normal)))<=tolerance,"Circle endpoints off plane");
    double angle=std::atan2(gp_Vec(normal).Dot(va.Crossed(vb)),va.Dot(vb));
    if(angle<0)angle+=2*pi;
    if(std::abs(angle*r-expected)>tolerance) {normal.Reverse();angle=2*pi-angle;}
    require(angle>0 && angle<2*pi && std::abs(angle*r-expected)<=tolerance,"Ambiguous circular arc");
    Handle(Geom_Circle) circle=new Geom_Circle(gp_Ax2(center,normal,gp_Dir(va)),r);
    BRepBuilderAPI_MakeEdge mk(circle,vertices.at(a),vertices.at(b),0,angle);require(mk.IsDone(),"Cannot build arc");shape=mk.Edge();
   } else throw std::runtime_error("Unsupported edge type");
   require(edges.emplace(n,Edge{shape,a,b,o}).second,"Duplicate edge ID");
  }
  auto wire=[&](QJsonObject loop) {
   std::vector<int> pending;std::set<int> seen;
   for(auto v:array(loop["edges"])) {int n=id(v);require(seen.insert(n).second,"Repeated edge in loop unsupported");pending.push_back(n);}
   require(!pending.empty(),"Empty loop");
   BRepBuilderAPI_MakeWire mk;auto first=edges.at(pending.front());int start=first.a,current=first.b;
   mk.Add(first.shape);pending.erase(pending.begin());
   while(!pending.empty()) {
    auto found=std::find_if(pending.begin(),pending.end(),[&](int n){return edges.at(n).a==current || edges.at(n).b==current;});
    require(found!=pending.end(),"Disconnected loop");const auto& e=edges.at(*found);auto shape=e.shape;
    if(e.a!=current)shape.Reverse(); current=e.a==current?e.b:e.a;
    mk.Add(shape);pending.erase(found);
   }
   require(current==start && mk.IsDone(),"Open/invalid wire");return mk.Wire();
  };
  BRepBuilderAPI_Sewing sewing(tolerance);double referenceArea=0;int faceCount=0;
  for(auto v:array(root["faces"])) {
   auto f=v.toObject();Handle(Geom_Surface) surface;
   if(kind(f,"FeaturePlanarFace")) {
    auto p=f["plane"].toObject();surface=new Geom_Plane(point(p["point"]),gp_Dir(vec(p["vector"])));
   } else if(kind(f,"FeatureCylindricalFace") || kind(f,"FeatureConicalFace")) {
    auto c=f["conic"].toObject(),axis=c["axis"].toObject();auto origin=point(axis["point"]);gp_Dir direction(vec(axis["vector"]));
    if(kind(f,"FeatureCylindricalFace")) {
     double r=num(c["radius"]);require(r>tolerance,"Invalid cylinder");surface=new Geom_CylindricalSurface(gp_Ax3(origin,direction),r);
    } else {
     // Recover the cone reference radius from its actual boundary circles.
     // Feature's Radius/Radius2 do not specify the reference plane unambiguously.
     std::vector<std::pair<double,double>> rings;
     for(auto ev:array(f["edges"])) {const auto& e=edges.at(id(ev));if(kind(e.data,"FeatureCircularEdge"))
      rings.emplace_back(gp_Vec(origin,point(e.data["center"])).Dot(gp_Vec(direction)),num(e.data["radius"]));}
     require(rings.size()>=2,"Cone has insufficient circular boundaries");std::sort(rings.begin(),rings.end());
     double height=rings.back().first-rings.front().first;require(height>tolerance,"Degenerate cone");
     double slope=(rings.back().second-rings.front().second)/height,angle=std::atan(slope);
     require(std::abs(std::abs(angle)-std::abs(num(c["semiangle"])))<1e-7,"Cone semiangle mismatch");
     for(auto ring:rings) require(std::abs(ring.second-(rings.front().second+slope*(ring.first-rings.front().first)))<=tolerance,"Cone rings disagree");
     surface=new Geom_ConicalSurface(gp_Ax3(origin.Translated(gp_Vec(direction)*rings.front().first),direction),angle,rings.front().second);
    }
   } else throw std::runtime_error("Unsupported surface type");
   auto loops=array(f["loops"]);int outer=-1;
   for(int i=0;i<loops.size();++i)if(kind(loops[i].toObject(),"FeatureOuterLoop")){require(outer==-1,"Multiple outer loops unsupported");outer=i;}
   require(outer>=0,"Missing outer loop");BRep_Builder builder;TopoDS_Face unhealed;builder.MakeFace(unhealed,surface,tolerance);
   builder.Add(unhealed,wire(loops[outer].toObject()));
   for(int i=0;i<loops.size();++i)if(i!=outer){require(kind(loops[i].toObject(),"FeatureInnerLoop"),"Unknown loop type");builder.Add(unhealed,wire(loops[i].toObject()));}
   ShapeFix_Edge edgeFix;
   for(TopExp_Explorer ex(unhealed,TopAbs_EDGE);ex.More();ex.Next())
    edgeFix.FixAddPCurve(TopoDS::Edge(ex.Current()),unhealed,false,tolerance);
   ShapeFix_Face fix(unhealed);fix.SetPrecision(tolerance);fix.Perform();fix.FixOrientation();auto face=fix.Face();
   BRepCheck_Analyzer check(face);
   if(!check.IsValid()) {
    for(TopExp_Explorer ex(face,TopAbs_WIRE);ex.More();ex.Next()) {
     auto result=check.Result(ex.Current());
     for(BRepCheck_ListIteratorOfListOfStatus it(result->Status());it.More();it.Next())std::cerr<<"wire status "<<it.Value()<<'\n';
     result->InitContextIterator();while(result->MoreShapeInContext()) {
      for(BRepCheck_ListIteratorOfListOfStatus it(result->StatusOnShape());it.More();it.Next())std::cerr<<"wire context status "<<it.Value()<<'\n';
      result->NextShapeInContext();
     }
    }
    for(BRepCheck_ListIteratorOfListOfStatus it(check.Result(face)->Status());it.More();it.Next())std::cerr<<"face status "<<it.Value()<<'\n';
   }
   require(check.IsValid(),"Invalid reconstructed face");GProp_GProps area;BRepGProp::SurfaceProperties(face,area);
   double expected=num(f["area"]);require(expected>0 && std::abs(area.Mass()-expected)<=std::max(1e-5,expected*1e-7),"Face area mismatch");
   referenceArea+=expected;sewing.Add(face);++faceCount;
  }
  require(faceCount>0,"No faces");sewing.Perform();auto sewn=sewing.SewedShape();
  require(!sewn.IsNull() && sewn.ShapeType()==TopAbs_SHELL && sewing.NbFreeEdges()==0,"Reconstruction is not one closed shell");
  BRepBuilderAPI_MakeSolid mkSolid(TopoDS::Shell(sewn));require(mkSolid.IsDone(),"Cannot make solid");auto solid=mkSolid.Solid();
  require(BRepLib::OrientClosedSolid(solid) && BRepCheck_Analyzer(solid).IsValid(),"Invalid OCCT solid");
  GProp_GProps volume,area;BRepGProp::VolumeProperties(solid,volume);BRepGProp::SurfaceProperties(solid,area);
  require(volume.Mass()>tolerance && std::abs(area.Mass()-referenceArea)<=std::max(1e-5,referenceArea*1e-7),"Invalid volume or total area");
  TopTools_IndexedMapOfShape fm,em,vm;TopExp::MapShapes(solid,TopAbs_FACE,fm);TopExp::MapShapes(solid,TopAbs_EDGE,em);TopExp::MapShapes(solid,TopAbs_VERTEX,vm);
  require(fm.Extent()==faceCount && em.Extent()==int(edges.size()) && vm.Extent()==int(vertices.size()),"Sewing changed topology counts");
  Bnd_Box expectedBox,actualBox;for(const auto& p:points)expectedBox.Add(p.second);BRepBndLib::AddOptimal(solid,actualBox,false,false);
  double ea[6],aa[6];expectedBox.Get(ea[0],ea[1],ea[2],ea[3],ea[4],ea[5]);actualBox.Get(aa[0],aa[1],aa[2],aa[3],aa[4],aa[5]);
  // Vertex bounds can miss curve extrema; this subset requires agreement.
  for(int i=0;i<6;++i)require(std::abs(ea[i]-aa[i])<=tolerance,"Solid bounds differ from snapshot vertex bounds");
 return solid;
 } catch(const Standard_Failure& failure) {
  throw std::runtime_error(std::string("Nieprawidlowa geometria ARD: ")+failure.GetMessageString());
 }
}

TopoDS_Shape ardimport::shapeFromReadResult(const QByteArray& reportBytes, const QByteArray& snapshot,
                                          const QString& sourceSha, const QString& payloadSha)
{
 QJsonParseError parse;
 const auto response=QJsonDocument::fromJson(reportBytes,&parse);
 const auto report=response.object();
 require(parse.error==QJsonParseError::NoError && response.isObject()
         && report["schema"]=="minicam-ard-feature-read-v1", "Niepoprawna odpowiedz konwertera ARD");
 require(report["read_confirmed"].toBool() && report["geometry_snapshot_available"].toBool()
         && report["installed_runtime_unchanged"].toBool() && report["source_ard_unchanged"].toBool()
         && report["save_requested"].isBool() && !report["save_requested"].toBool(), "Odczyt bryly ARD niepotwierdzony");
 require(sourceSha.size()==64 && payloadSha.size()==64
         && report["input"].toObject()["sha256"]==sourceSha
         && report["extraction"].toObject()["parasolid_sha256"]==payloadSha, "Raport nie odpowiada otwieranemu ARD");
 const auto hash=QString::fromLatin1(QCryptographicHash::hash(snapshot,QCryptographicHash::Sha256).toHex());
 const auto descriptor=report["geometry_snapshot"].toObject();
 require(descriptor["sha256"]==hash && descriptor["topology_read_complete"].toBool(), "Dane geometrii ARD sa niekompletne");
 const auto geometry=QJsonDocument::fromJson(snapshot,&parse).object();
 const auto measurement=report["measurement"].toObject();const auto counts=measurement["topology_counts"].toObject();
 require(parse.error==QJsonParseError::NoError && geometry["faces"].toArray().size()==counts["Faces"].toInt(-1)
         && geometry["edges"].toArray().size()==counts["Edges"].toInt(-1)
         && geometry["vertices"].toArray().size()==counts["Vertices"].toInt(-1), "Liczba elementow bryly nie zgadza sie z raportem ARD");
 auto shape=shapeFromSnapshot(snapshot);Bnd_Box box;BRepBndLib::AddOptimal(shape,box,false,false);
 double bounds[6];box.Get(bounds[0],bounds[1],bounds[2],bounds[3],bounds[4],bounds[5]);
 const auto measured=measurement["bounds"].toObject();
 const QStringList keys{"MinX","MinY","MinZ","MaxX","MaxY","MaxZ"};
 for(int i=0;i<6;++i)require(measured[keys[i]].isDouble() && std::isfinite(measured[keys[i]].toDouble())
                           && std::abs(bounds[i]-measured[keys[i]].toDouble())<=tolerance,
                           "Wymiary bryly roznia sie od odczytu ARD");
 return shape;
}
