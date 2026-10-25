// Reconstruction of the supported analytic Feature snapshot subset for GUI and tests.
// No vendor runtime, approximation of curved surfaces, or export worker changes.
#include "FeatureGeometry.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Plane.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_ConicalSurface.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Geom2d_Line.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Dir2d.hxx>
#include <gp_Vec2d.hxx>
#include <GeomAPI_IntSS.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <ShapeFix_Face.hxx>
#include <ShapeFix_Edge.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Iterator.hxx>
#include <Standard_Failure.hxx>
#include <gp_Ax3.hxx>
#include <gp_Ax2.hxx>
#include <gp_Vec.hxx>
#include <map>
#include <set>
#include <vector>
#include <cmath>
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

bool semicircleInsideTrim(const QJsonObject& edge,const std::map<int,QJsonObject>& faces,
                          const gp_Vec& radial,const gp_Dir& normal,double angle)
{
    const gp_Vec midpoint=radial*std::cos(angle/2)
        +gp_Vec(normal).Crossed(radial)*std::sin(angle/2);
    bool constrained=false;
    for(auto ref:array(edge["faces"])) {
        const auto face=faces.at(id(ref));if(!kind(face,"FeatureCylindricalFace"))continue;
        const auto conic=face["conic"].toObject();
        const double start=num(conic["startangle"]),span=num(conic["endangle"])-start;
        if(span>=2*pi-1e-7 || span<=0)continue;
        const gp_Dir axis(vec(conic["axis"].toObject()["vector"]));
        if(std::abs(axis.Dot(normal))<1-1e-7)continue;
        gp_Vec x=vec(conic["xaxis"]);x-=gp_Vec(axis)*x.Dot(gp_Vec(axis));x.Normalize();
        const gp_Vec y=gp_Vec(axis).Crossed(x);
        const double phi=std::atan2(midpoint.Dot(y),midpoint.Dot(x));
        double delta=std::fmod(phi-start,2*pi);if(delta<0)delta+=2*pi;
        constrained=true;if(delta>span+1e-7)return false;
    }
    return constrained;
}

Handle(Geom_Surface) intersectionSurface(const QJsonObject& face)
{
    if (kind(face, "FeaturePlanarFace")) {
        const auto plane = face["plane"].toObject();
        return new Geom_Plane(point(plane["point"]), gp_Dir(vec(plane["vector"])));
    }
    require(kind(face, "FeatureCylindricalFace"), "Spline edge requires a supported plane/cylinder intersection");
    const auto conic = face["conic"].toObject(), axis = conic["axis"].toObject();
    const double radius = num(conic["radius"]);
    require(radius > tolerance, "Invalid intersection cylinder radius");
    return new Geom_CylindricalSurface(gp_Ax3(point(axis["point"]), gp_Dir(vec(axis["vector"]))), radius);
}

TopoDS_Edge intersectionEdge(const QJsonObject& edge, const std::map<int,QJsonObject>& faces,
                            const TopoDS_Vertex& a, const TopoDS_Vertex& b,
                            const gp_Pnt& pa, const gp_Pnt& pb, double expected)
{
    const auto refs = array(edge["faces"]);
    require(refs.size()==2, "Spline edge requires two adjacent surfaces");
    const auto fa = faces.at(id(refs[0])), fb = faces.at(id(refs[1]));
    require((kind(fa,"FeaturePlanarFace") && kind(fb,"FeatureCylindricalFace"))
            || (kind(fb,"FeaturePlanarFace") && kind(fa,"FeatureCylindricalFace")),
            "Only analytic plane/cylinder spline intersections are supported");
    GeomAPI_IntSS intersection(intersectionSurface(fa),intersectionSurface(fb),tolerance/10);
    require(intersection.IsDone(), "Cannot intersect spline edge surfaces");
    TopoDS_Edge result; int matches = 0;
    for (int i=1; i<=intersection.NbLines(); ++i) {
        auto curve = intersection.Line(i);
        // IntSS may return an ellipse trimmed at its parameter seam. Its
        // analytic basis is still the complete plane/cylinder intersection.
        while (const auto trimmed=Handle(Geom_TrimmedCurve)::DownCast(curve))
            curve=trimmed->BasisCurve();
        GeomAdaptor_Curve adaptor(curve);
        // No approximation of an arbitrary spline from endpoints or a sampled polyline.
        if (adaptor.GetType()!=GeomAbs_Ellipse && adaptor.GetType()!=GeomAbs_Circle
            && adaptor.GetType()!=GeomAbs_Line) continue;
        GeomAPI_ProjectPointOnCurve projectA(pa,curve),projectB(pb,curve);
        if (!projectA.NbPoints() || !projectB.NbPoints()
            || projectA.LowerDistance()>tolerance || projectB.LowerDistance()>tolerance) continue;
        const double ua=projectA.LowerDistanceParameter(), ub=projectB.LowerDistanceParameter();
        auto candidate = [&](double first, double last, bool reverse) {
            if (!(last>first)) return;
            const double length=GCPnts_AbscissaPoint::Length(adaptor,first,last,tolerance/100);
            if (std::abs(length-expected)>tolerance) return;
            BRepBuilderAPI_MakeEdge make(curve,reverse?b:a,reverse?a:b,first,last);
            if (!make.IsDone()) return;
            result=make.Edge();if(reverse)result.Reverse();++matches;
        };
        if (curve->IsPeriodic()) {
            const double period=curve->Period();double delta=std::fmod(ub-ua+period,period);
            if (delta==0) delta=period;
            candidate(ua,ua+delta,false);
            if (delta<period) candidate(ub,ub+period-delta,true);
        } else if (ua<ub) candidate(ua,ub,false);
        else candidate(ub,ua,true);
    }
    require(matches==1, "Cannot unambiguously reconstruct analytic spline edge from adjacent surfaces");
    return result;
}

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
  std::map<int,QJsonObject> faceData;
  for (auto value:array(root["faces"])) {
   const auto face=value.toObject();
   require(faceData.emplace(id(face["id"]),face).second,"Duplicate face ID");
  }
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
    double angle;
    if(a==b) {
     require(std::abs(2*pi*r-expected)<=tolerance,"Closed circular edge length does not match its circumference");
     angle=2*pi;
    } else {
     angle=std::atan2(gp_Vec(normal).Dot(va.Crossed(vb)),va.Dot(vb));
     if(angle<0)angle+=2*pi;
     if(std::abs(angle*r-expected)<=tolerance && std::abs((2*pi-angle)*r-expected)<=tolerance) {
      const bool forward=semicircleInsideTrim(o,faceData,va,normal,angle);
      const bool backward=semicircleInsideTrim(o,faceData,va,normal.Reversed(),2*pi-angle);
      require(forward!=backward,"Semicircle direction is not determined by adjacent surface trims");
      if(backward){normal.Reverse();angle=2*pi-angle;}
     } else if(std::abs(angle*r-expected)>tolerance) {normal.Reverse();angle=2*pi-angle;}
     require(angle>0 && angle<2*pi && std::abs(angle*r-expected)<=tolerance,"Ambiguous circular arc");
    }
    Handle(Geom_Circle) circle=new Geom_Circle(gp_Ax2(center,normal,gp_Dir(va)),r);
    BRepBuilderAPI_MakeEdge mk(circle,vertices.at(a),vertices.at(b),0,angle);require(mk.IsDone(),"Cannot build arc");shape=mk.Edge();
   } else if(kind(o,"FeatureSplineEdge")) {
    shape=intersectionEdge(o,faceData,vertices.at(a),vertices.at(b),pa,pb,expected);
   } else throw std::runtime_error("Unsupported edge type");
   require(edges.emplace(n,Edge{shape,a,b,o}).second,"Duplicate edge ID");
  }
  auto wire=[&](QJsonObject loop, const QJsonObject& face) {
   std::vector<int> pending;std::set<int> seen;
   for(auto v:array(loop["edges"])) {int n=id(v);require(seen.insert(n).second,"Repeated edge in loop unsupported");pending.push_back(n);}
   require(!pending.empty(),"Empty loop");
   // Feature exposes unique edges of a loop; the seam of a complete cylinder
   // occurs once there, but OCCT needs its two opposite coedge uses.
   if (kind(face,"FeatureCylindricalFace") && pending.size()==3) {
    const auto conic=face["conic"].toObject();
    if (std::abs(num(conic["endangle"])-num(conic["startangle"])-2*pi)<1e-7) {
     int seam=0;std::vector<int> rings;
     for(int n:pending) {
      const auto& e=edges.at(n);const auto adjacent=array(e.data["faces"]);
      if(e.a==e.b && kind(e.data,"FeatureCircularEdge"))rings.push_back(e.a);
      else if(kind(e.data,"FeatureLinearEdge") && adjacent.size()==1
              && id(adjacent[0])==id(face["id"]))seam=n;
     }
     if(seam && rings.size()==2) {
      const auto& e=edges.at(seam);
      require((rings[0]==e.a && rings[1]==e.b) || (rings[0]==e.b && rings[1]==e.a),
              "Cylinder seam does not connect its circular boundaries");
      const auto axis=conic["axis"].toObject();
      const gp_Dir direction(vec(axis["vector"]));
      int low=0,high=0;
      const bool ascending=gp_Vec(points.at(e.a),points.at(e.b)).Dot(gp_Vec(direction))>0;
      const int lowerVertex=ascending?e.a:e.b;
      for(int n:pending)if(edges.at(n).a==edges.at(n).b) {
       if(edges.at(n).a==lowerVertex)low=n;else high=n;
      }
      require(low && high,"Missing cylinder circular boundary");
      auto bottom=edges.at(low).shape,top=edges.at(high).shape,rising=e.shape;
      if(vec(edges.at(low).data["plane"].toObject()["vector"]).Dot(gp_Vec(direction))<0)bottom.Reverse();
      if(vec(edges.at(high).data["plane"].toObject()["vector"]).Dot(gp_Vec(direction))>0)top.Reverse();
      if(!ascending)rising.Reverse();
      BRep_Builder builder;TopoDS_Wire closed;builder.MakeWire(closed);
      builder.Add(closed,bottom);builder.Add(closed,rising);builder.Add(closed,top);
      builder.Add(closed,rising.Reversed());
      return closed;
     }
    }
   }
   if(kind(face,"FeatureCylindricalFace") && pending.size()!=3) {
    const auto conic=face["conic"].toObject();
    if(std::abs(num(conic["endangle"])-num(conic["startangle"])-2*pi)<1e-7) {
     const auto original=pending;
     for(int n:original) {
      const auto& e=edges.at(n);const auto adjacent=array(e.data["faces"]);
      if(e.a!=e.b && kind(e.data,"FeatureLinearEdge") && adjacent.size()==1
         && id(adjacent[0])==id(face["id"]))pending.push_back(n);
     }
    }
   }
   // Euler traversal: unique-edge collections with a seam can contain several
   // closed subcycles. Greedily taking the next edge can leave one stranded.
   struct Step { int occurrence; bool reverse; };
   // Where available, Feature's vertex walk records the coedge order even
   // though its edge collection omits the second occurrence of a seam.
   const auto walk=loop["vertices"].toArray();
   if(walk.size()==int(pending.size())) {
    std::vector<bool> taken(pending.size(),false);std::vector<Step> ordered;
    bool unique=true;
    for(int i=0;i<walk.size() && unique;++i) {
     const int from=id(walk[i]),to=id(walk[(i+1)%walk.size()]);int found=-1;
     for(size_t j=0;j<pending.size();++j)if(!taken[j]) {
      const auto& e=edges.at(pending[j]);
      if((e.a==from && e.b==to) || (e.b==from && e.a==to)) {
       if(found>=0 && pending[found]!=pending[j]){unique=false;break;}
       if(found<0)found=int(j);
      }
     }
     if(found<0){unique=false;break;}
     taken[found]=true;ordered.push_back({found,edges.at(pending[found]).a!=from});
    }
    if(unique) {
     BRep_Builder builder;TopoDS_Wire result;builder.MakeWire(result);
     for(const auto& step:ordered) {
      auto shape=edges.at(pending[step.occurrence]).shape;
      if(step.reverse)shape.Reverse();builder.Add(result,shape);
     }
     return result;
    }
   }
   std::vector<bool> used(pending.size(),false);
   std::vector<int> stack{edges.at(pending.front()).a};
   std::vector<Step> incoming{{-1,false}}, circuit;
   while(!stack.empty()) {
    const int current=stack.back();int found=-1;
    for(size_t i=0;i<pending.size();++i)if(!used[i]) {
     const auto& e=edges.at(pending[i]);if(e.a==current || e.b==current){found=int(i);break;}
    }
    if(found>=0) {
     used[found]=true;const auto& e=edges.at(pending[found]);const bool reverse=e.a!=current;
     stack.push_back(reverse?e.a:e.b);incoming.push_back({found,reverse});
    } else {
     if(incoming.back().occurrence>=0)circuit.push_back(incoming.back());
     incoming.pop_back();stack.pop_back();
    }
   }
   require(circuit.size()==pending.size(),"Disconnected loop");std::reverse(circuit.begin(),circuit.end());
   int current=edges.at(pending.front()).a;const int start=current;
   BRep_Builder builder;TopoDS_Wire result;builder.MakeWire(result);
   for(const auto& step:circuit) {
    const auto& e=edges.at(pending[step.occurrence]);
    require(current==(step.reverse?e.b:e.a),"Disconnected coedges in loop");
    auto shape=e.shape;if(step.reverse)shape.Reverse();builder.Add(result,shape);
    current=step.reverse?e.a:e.b;
   }
   require(current==start,"Open loop");return result;
  };
  BRepBuilderAPI_Sewing sewing(tolerance);double referenceArea=0;int faceCount=0;
  for(auto v:array(root["faces"])) {
   auto f=v.toObject();Handle(Geom_Surface) surface;
   if(kind(f,"FeaturePlanarFace")) {
    auto p=f["plane"].toObject();surface=new Geom_Plane(point(p["point"]),gp_Dir(vec(p["vector"])));
   } else if(kind(f,"FeatureCylindricalFace") || kind(f,"FeatureConicalFace")) {
    auto c=f["conic"].toObject(),axis=c["axis"].toObject();auto origin=point(axis["point"]);gp_Dir direction(vec(axis["vector"]));
    if(kind(f,"FeatureCylindricalFace")) {
     double r=num(c["radius"]);require(r>tolerance,"Invalid cylinder");gp_Ax3 placement(origin,direction);
     // Put the parameter seam outside a bounded circular patch. Otherwise two
     // endpoints at u=0 and u=2pi can look disconnected in the face's UV wire.
     for(auto ev:array(f["edges"])) {
      const auto& e=edges.at(id(ev));
      if(e.a!=e.b && kind(e.data,"FeatureCircularEdge")) {
       double first,last;const auto curve=BRep_Tool::Curve(e.shape,first,last);
       auto radial=gp_Vec(origin,curve->Value((first+last)/2));
       radial-=gp_Vec(direction)*radial.Dot(gp_Vec(direction));
       placement=gp_Ax3(origin,direction,gp_Dir(-radial));break;
      }
     }
     for(auto ev:array(f["edges"])) {
      const auto& e=edges.at(id(ev));const auto adjacent=array(e.data["faces"]);
      if(e.a!=e.b && kind(e.data,"FeatureLinearEdge") && adjacent.size()==1
         && id(adjacent[0])==id(f["id"])) {
       auto radial=gp_Vec(origin,points.at(e.a));radial-=gp_Vec(direction)*radial.Dot(gp_Vec(direction));
       require(std::abs(radial.Magnitude()-r)<=tolerance,"Cylinder seam is off its surface");
       placement=gp_Ax3(origin,direction,gp_Dir(radial));
      }
     }
     surface=new Geom_CylindricalSurface(placement,r);
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
   builder.Add(unhealed,wire(loops[outer].toObject(),f));
   for(int i=0;i<loops.size();++i)if(i!=outer){require(kind(loops[i].toObject(),"FeatureInnerLoop"),"Unknown loop type");builder.Add(unhealed,wire(loops[i].toObject(),f));}
   ShapeFix_Edge edgeFix;
   for(TopExp_Explorer ex(unhealed,TopAbs_EDGE);ex.More();ex.Next()) {
    bool seam=false;
    if(kind(f,"FeatureCylindricalFace"))
     for(auto ev:array(f["edges"])) {
      const auto& e=edges.at(id(ev));
      if(e.a!=e.b && array(e.data["faces"]).size()==1 && ex.Current().IsSame(e.shape))seam=true;
     }
    if(!seam)edgeFix.FixAddPCurve(TopoDS::Edge(ex.Current()),unhealed,false,tolerance);
   }
   std::vector<TopoDS_Edge> preparedSeams;
   for(TopExp_Explorer ex(unhealed,TopAbs_WIRE);ex.More();ex.Next()) {
    const auto original=TopoDS::Wire(ex.Current());
    if(kind(f,"FeatureCylindricalFace")) {
     std::vector<TopoDS_Edge> coedges;
     for(TopoDS_Iterator it(original);it.More();it.Next())coedges.push_back(TopoDS::Edge(it.Value()));
     for(size_t i=0;i<coedges.size();++i) {
      const auto edge=coedges[i];bool seam=false;
      for(auto ev:array(f["edges"])) {
       const auto& e=edges.at(id(ev));
       if(e.a!=e.b && array(e.data["faces"]).size()==1 && edge.IsSame(e.shape))seam=true;
      }
      if(!seam || std::any_of(preparedSeams.begin(),preparedSeams.end(),[&](const auto& e){return e.IsSame(edge);}))continue;
      preparedSeams.push_back(edge);
      const auto previous=coedges[(i+coedges.size()-1)%coedges.size()];double first,last;
      const auto pc=BRep_Tool::CurveOnSurface(previous,unhealed,first,last);
      require(!pc.IsNull(),"Missing pcurve before cylinder seam");
      const double endU=pc->Value(previous.Orientation()==TopAbs_REVERSED?first:last).X();
      double nearU=std::round(endU/(2*pi))*2*pi;
      require(std::abs(endU-nearU)<1e-7,"Cylinder seam does not meet the preceding pcurve");
      nearU=nearU>pi?2*pi:0;
      const double u=edge.Orientation()==TopAbs_REVERSED?2*pi-nearU:nearU;
      const auto cylinder=Handle(Geom_CylindricalSurface)::DownCast(surface);
      const auto curve=BRep_Tool::Curve(edge,first,last);const auto position=cylinder->Position();
      const double va=gp_Vec(position.Location(),curve->Value(first)).Dot(gp_Vec(position.Direction()));
      const double vb=gp_Vec(position.Location(),curve->Value(last)).Dot(gp_Vec(position.Direction()));
      const double slope=(vb-va)/(last-first);
      require(std::abs(std::abs(slope)-1)<=1e-7,"Non-axial cylinder seam");
      Handle(Geom2d_Curve) forward=new Geom2d_Line(gp_Pnt2d(u,va-slope*first),gp_Dir2d(0,slope));
      Handle(Geom2d_Curve) backward=new Geom2d_Line(gp_Pnt2d(2*pi-u,va-slope*first),gp_Dir2d(0,slope));
      builder.UpdateEdge(edge,forward,backward,unhealed,tolerance);builder.Range(edge,unhealed,first,last);
     }
    }
    if(kind(f,"FeatureCylindricalFace")) {
     gp_Pnt2d previous,begin;bool firstEdge=true;
     for(TopoDS_Iterator it(original);it.More();it.Next()) {
      const auto edge=TopoDS::Edge(it.Value());double first,last;
      auto pc=BRep_Tool::CurveOnSurface(edge,unhealed,first,last);
      require(!pc.IsNull(),"Missing cylinder boundary pcurve");
      const bool reversed=edge.Orientation()==TopAbs_REVERSED;
      auto from=pc->Value(reversed?last:first),to=pc->Value(reversed?first:last);
      if(!firstEdge) {
       const double shift=std::round((previous.X()-from.X())/(2*pi))*2*pi;
       if(std::abs(shift)>1e-7) {
        require(!BRep_Tool::IsClosed(edge,unhealed),"Inconsistent cylinder seam pcurve order");
        pc=Handle(Geom2d_Curve)::DownCast(pc->Copy());pc->Translate(gp_Vec2d(shift,0));
        builder.UpdateEdge(edge,pc,unhealed,tolerance);builder.Range(edge,unhealed,first,last);
        from=pc->Value(reversed?last:first);to=pc->Value(reversed?first:last);
       }
       require(from.Distance(previous)<=tolerance,"Disconnected cylinder UV boundaries");
      } else {begin=from;firstEdge=false;}
      previous=to;
     }
     require(previous.Distance(begin)<=tolerance,"Open cylinder UV boundary");
    }
   }
   ShapeFix_Face fix(unhealed);fix.SetPrecision(tolerance);
   if(kind(f,"FeatureCylindricalFace")) {
    // Its coedge walk and periodic pcurves were constructed explicitly. A
    // generic reorder can discard a subcycle adjacent to a shared seam.
    fix.FixWireMode()=0;fix.FixMissingSeamMode()=0;fix.FixAddNaturalBoundMode()=0;
   } else fix.Perform();
   fix.FixOrientation();auto face=fix.Face();
   BRepCheck_Analyzer check(face);
   if(!check.IsValid())throw std::runtime_error("Invalid reconstructed face " + std::to_string(id(f["id"])));
   GProp_GProps area;BRepGProp::SurfaceProperties(face,area);
   double expected=num(f["area"]);
   if(!(expected>0 && std::abs(area.Mass()-expected)<=std::max(1e-5,expected*1e-7)))
    throw std::runtime_error("Face area mismatch, face " + std::to_string(id(f["id"]))
                             + ", actual " + std::to_string(area.Mass()) + ", expected " + std::to_string(expected));
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
  Bnd_Box expectedBox,actualBox;for(const auto& p:points)expectedBox.Add(p.second);
  for(const auto& e:edges)BRepBndLib::AddOptimal(e.second.shape,expectedBox,false,false);
  BRepBndLib::AddOptimal(solid,actualBox,false,false);
  double ea[6],aa[6];expectedBox.Get(ea[0],ea[1],ea[2],ea[3],ea[4],ea[5]);actualBox.Get(aa[0],aa[1],aa[2],aa[3],aa[4],aa[5]);
  // Include the exact source boundary curves, including complete circles.
  for(int i=0;i<6;++i)require(std::abs(ea[i]-aa[i])<=tolerance,"Solid bounds differ from snapshot boundary bounds");
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
