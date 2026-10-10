// OccView – wskazywanie punktu: krzyż linii przy kursorze, uchwyty (przyciąganie
// do końców, środków, centrów i ćwiartek), znacznik uchwytu i podgląd przesuwania.
// Reszta widoku (rysowanie, wybór, mysz) jest w OccView.cpp.
#include "OccView.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <ElCLib.hxx>
#include <Graphic3d_TransformPers.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <QCursor>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>

#include <cmath>

namespace {
constexpr double kHalfPi = 1.57079632679489661923;
}

void OccView::showCrosshair(const QPoint& pos)
{
    // Krzyż linii przy kursorze (wskazywanie punktu): trzy długie białe linie
    // wzdłuż osi X, Y i Z przez punkt spod kursora na płaszczyźnie Z = 0.
    if (m_crosshair.IsNull()) {
        const double len = 1.0e5;
        BRep_Builder builder;
        TopoDS_Compound lines;
        builder.MakeCompound(lines);
        builder.Add(lines, BRepBuilderAPI_MakeEdge(gp_Pnt(-len, 0, 0), gp_Pnt(len, 0, 0)).Edge());
        builder.Add(lines, BRepBuilderAPI_MakeEdge(gp_Pnt(0, -len, 0), gp_Pnt(0, len, 0)).Edge());
        builder.Add(lines, BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, -len), gp_Pnt(0, 0, len)).Edge());
        m_crosshair = new AIS_Shape(lines);
        m_crosshair->SetColor(Quantity_NOC_WHITE);
        m_crosshair->SetWidth(1.0);
        // Nie liczy się do "dopasuj do okna" ani do zakresu głębi widoku.
        m_crosshair->SetInfiniteState(Standard_True);
    }
    const gp_Pnt p = pointOnTable(pos);
    gp_Trsf move;
    move.SetTranslation(gp_Vec(p.X(), p.Y(), 0.0));
    if (!m_context->IsDisplayed(m_crosshair))
        m_context->Display(m_crosshair, AIS_WireFrame, -1, Standard_False); // -1: nie do zaznaczania
    m_context->SetLocation(m_crosshair, TopLoc_Location(move));
    m_view->Redraw();
}

void OccView::hideCrosshair()
{
    if (!m_crosshair.IsNull() && m_context->IsDisplayed(m_crosshair))
        m_context->Erase(m_crosshair, Standard_False);
}

void OccView::addSnapPoints(const TopoDS_Shape& shape, double height, std::vector<SnapPoint>& out)
{
    // Końce i środki krawędzi, środki i ćwiartki łuków/okręgów. height > 0: te same
    // punkty także niżej o height (dół ścianki konturu).
    auto add = [&](Snap kind, const gp_Pnt& p) {
        out.push_back({kind, p});
        if (height > 1e-6)
            out.push_back({kind, gp_Pnt(p.X(), p.Y(), p.Z() - height)});
    };
    TopTools_IndexedMapOfShape edges; // każda krawędź raz (w bryle krawędź należy do dwóch ścian)
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    for (int i = 1; i <= edges.Extent(); ++i) {
        const TopoDS_Edge& edge = TopoDS::Edge(edges(i));
        if (BRep_Tool::Degenerated(edge))
            continue;
        const BRepAdaptor_Curve curve(edge);
        const double t0 = curve.FirstParameter(), t1 = curve.LastParameter();
        add(Snap::End, curve.Value(t0));
        add(Snap::End, curve.Value(t1));
        add(Snap::Mid, curve.Value((t0 + t1) / 2));
        if (curve.GetType() == GeomAbs_Circle) {
            const gp_Circ circle = curve.Circle();
            add(Snap::Centre, circle.Location());
            // Ćwiartki: 0°, 90°, 180°, 270° w układzie XY – o ile leżą na łuku.
            for (int q = 0; q < 4; ++q) {
                const gp_Pnt p(circle.Location().X() + circle.Radius() * std::cos(q * kHalfPi),
                               circle.Location().Y() + circle.Radius() * std::sin(q * kHalfPi),
                               circle.Location().Z());
                if (p.Distance(ElCLib::Value(ElCLib::Parameter(circle, p), circle)) > 1e-6)
                    continue; // okrąg nie leży poziomo – ćwiartki XY nie są na nim
                double t = ElCLib::Parameter(circle, p);
                while (t < t0 - 1e-9)
                    t += 4 * kHalfPi;
                if (t <= t1 + 1e-9)
                    add(Snap::Quadrant, p);
            }
        }
    }
}

void OccView::startMovePreview(const gp_Pnt& base, bool model, const std::vector<int>& geometries)
{
    stopMovePreview();
    m_movePreviewHasModel = false;
    m_moveBase = base;
    if (model && !m_modelData.shape.IsNull()) {
        // Bryła: półprzezroczysta kopia.
        Handle(AIS_Shape) solid = new AIS_Shape(m_modelData.shape);
        solid->SetColor(Quantity_Color(0.72, 0.72, 1.00, Quantity_TOC_sRGB));
        solid->SetTransparency(0.55);
        solid->Attributes()->SetFaceBoundaryDraw(Standard_True);
        solid->Attributes()->SetFaceBoundaryAspect(
            new Prs3d_LineAspect(Quantity_Color(0.85, 0.85, 1.0, Quantity_TOC_sRGB), Aspect_TOL_SOLID, 1.0));
        // Kopia leży dokładnie na płaszczyznach oryginału – bez tego ściany migotałyby
        // (z-fighting). Przesuwamy ją minimalnie w stronę kamery i rysujemy po bryle.
        solid->Attributes()->ShadingAspect()->Aspect()->SetPolygonOffsets(Aspect_POM_Fill, -2.0f, -2.0f);
        solid->SetZLayer(Graphic3d_ZLayerId_Top);
        m_context->Display(solid, AIS_Shaded, -1, Standard_False); // -1: nie do zaznaczania
        m_movePreview.push_back(solid);
        m_movePreviewHasModel = true;
    }
    for (int i : geometries) {
        if (i < 0 || i >= static_cast<int>(m_geometry.size()))
            continue;
        // Geometria: same cienkie linie w kolorze warstwy, bez wypełnienia.
        Handle(AIS_Shape) lines = new AIS_Shape(m_geometry[static_cast<size_t>(i)]->Shape());
        lines->SetColor(m_geometryColors[static_cast<size_t>(i)]);
        lines->SetWidth(1.0);
        lines->SetZLayer(Graphic3d_ZLayerId_Top);
        m_context->Display(lines, AIS_WireFrame, -1, Standard_False);
        m_movePreview.push_back(lines);
    }
    updatePickFeedback(toPixels(mapFromGlobal(QCursor::pos())));
}

void OccView::stopMovePreview()
{
    for (const Handle(AIS_Shape)& p : m_movePreview)
        m_context->Remove(p, Standard_False);
    m_movePreview.clear();
}

void OccView::setSnap(Snap snap, const QCursor& cursor)
{
    m_snap = snap;
    m_snapCursor = cursor;
    if (!m_context.IsNull() && m_interaction == Interaction::PickPoint)
        updatePickFeedback(toPixels(mapFromGlobal(QCursor::pos())));
}

bool OccView::snapAt(const QPoint& pos, gp_Pnt& out) const
{
    // Najbliższy na ekranie punkt wybranego rodzaju (Auto = końce, środki i ćwiartki).
    const double maxDist = 20.0 * devicePixelRatioF();
    double best = maxDist;
    bool found = false;
    // Punkty widocznych geometrii i – gdy jest widoczna – bryły.
    std::vector<const std::vector<SnapPoint>*> sources;
    for (size_t i = 0; i < m_snapPoints.size(); ++i)
        if (m_geometryVisible[i])
            sources.push_back(&m_snapPoints[i]);
    if (!m_model.IsNull() && m_modelVisible)
        sources.push_back(&m_modelSnapPoints);
    for (const std::vector<SnapPoint>* points : sources) {
        for (const SnapPoint& sp : *points) {
            const bool wanted = m_snap == Snap::Auto
                                    ? (sp.kind == Snap::End || sp.kind == Snap::Mid || sp.kind == Snap::Quadrant)
                                    : sp.kind == m_snap;
            if (!wanted)
                continue;
            Standard_Integer x, y;
            m_view->Convert(sp.point.X(), sp.point.Y(), sp.point.Z(), x, y);
            const double d = std::hypot(pos.x() - x, pos.y() - y);
            if (d < best) {
                best = d;
                out = sp.point;
                found = true;
            }
        }
    }
    return found;
}

void OccView::updatePickFeedback(const QPoint& pos)
{
    if (!m_movePreview.empty()) {
        // Kopia jedzie tam, gdzie trafiłby punkt: na uchwyt (z jego Z) albo pod kursor
        // na płaszczyźnie Z = 0. Bryła przesuwa się też w Z, geometrie tylko w X i Y.
        gp_Pnt p;
        if (m_snap == Snap::None || !snapAt(pos, p))
            p = pointOnTable(pos);
        const gp_Vec offset(p.X() - m_moveBase.X(), p.Y() - m_moveBase.Y(), p.Z() - m_moveBase.Z());
        gp_Trsf move, flat;
        move.SetTranslation(offset);
        flat.SetTranslation(gp_Vec(offset.X(), offset.Y(), 0.0));
        for (size_t i = 0; i < m_movePreview.size(); ++i)
            m_context->SetLocation(m_movePreview[i],
                                   TopLoc_Location(i == 0 && m_movePreviewHasModel ? move : flat));
    }
    if (m_snap == Snap::None) {
        if (!m_snapMarker.IsNull())
            m_context->Erase(m_snapMarker, Standard_False);
        setCursor(Qt::CrossCursor);
        showCrosshair(pos);
        return;
    }
    // Uchwyt włączony: bez krzyża, zielona piłeczka na punkcie, do którego klei się kursor,
    // a przy strzałce kursora ikonka uchwytu.
    hideCrosshair();
    setCursor(m_snapCursor);
    gp_Pnt p;
    if (snapAt(pos, p)) {
        if (m_snapMarker.IsNull()) {
            // Pomocnicza zielona, półprzezroczysta piłeczka o stałej wielkości na ekranie
            // (ok. 22 px) – kula w (0,0,0), przesuwana do punktu przez TransformPers.
            Handle(AIS_Shape) ball = new AIS_Shape(BRepPrimAPI_MakeSphere(11.0).Shape());
            ball->SetColor(Quantity_Color(0.20, 0.85, 0.20, Quantity_TOC_sRGB));
            ball->SetTransparency(0.25);
            ball->Attributes()->SetFaceBoundaryDraw(Standard_False);
            ball->SetZLayer(Graphic3d_ZLayerId_Topmost);
            m_snapMarker = ball;
        }
        m_snapMarker->SetTransformPersistence(new Graphic3d_TransformPers(Graphic3d_TMF_ZoomPers, p));
        if (m_context->IsDisplayed(m_snapMarker))
            m_context->Redisplay(m_snapMarker, Standard_False);
        else
            m_context->Display(m_snapMarker, AIS_Shaded, -1, Standard_False); // -1: nie do zaznaczania
    } else if (!m_snapMarker.IsNull()) {
        m_context->Erase(m_snapMarker, Standard_False);
    }
    m_view->Redraw();
}
