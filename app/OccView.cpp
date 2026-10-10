#include "OccView.h"

#include <AIS_ColoredShape.hxx>
#include <SelectMgr_EntityOwner.hxx>
#include <StdSelect_ViewerSelector3d.hxx>
#include <AIS_Shape.hxx>
#include <AIS_Trihedron.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Geom_Axis2Placement.hxx>
#include <Graphic3d_TransformPers.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Prs3d_DatumAspect.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Prs3d_ShadingAspect.hxx>

#include <QKeyEvent>
#include <QMouseEvent>

#include <algorithm>
#include <cmath>
#include <QWheelEvent>

#if defined(_WIN32)
#include <WNT_Window.hxx>
#elif defined(__APPLE__)
#include <Cocoa_Window.hxx>
#else
#include <Xw_Window.hxx>
#endif

OccView::OccView(QWidget* parent)
    : QWidget(parent)
{
    // Widżet musi mieć własne natywne okno, do którego podepnie się OpenGL z OCCT.
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_NativeWindow);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(200, 200);
}

void OccView::initViewer()
{
    if (!m_view.IsNull())
        return;

    Handle(Aspect_DisplayConnection) display = new Aspect_DisplayConnection();
    Handle(OpenGl_GraphicDriver) driver = new OpenGl_GraphicDriver(display);

    m_viewer = new V3d_Viewer(driver);
    m_viewer->SetDefaultLights();
    m_viewer->SetLightOn();

    m_context = new AIS_InteractiveContext(m_viewer);
    m_context->SetDisplayMode(AIS_Shaded, Standard_False);
    // Czarne krawędzie na ścianach – bryła czytelniejsza przy obróbce.
    m_context->DefaultDrawer()->SetFaceBoundaryDraw(Standard_True);
    m_context->DefaultDrawer()->SetFaceBoundaryAspect(
        new Prs3d_LineAspect(Quantity_NOC_BLACK, Aspect_TOL_SOLID, 1.0));

    // Zaznaczanie i podświetlanie robimy sami (kolorami), więc wyłączamy
    // automatyczne podświetlenie OCCT pod kursorem.
    m_context->SetAutomaticHilight(Standard_False);
    m_view = m_viewer->CreateView();

#if defined(_WIN32)
    Handle(WNT_Window) window = new WNT_Window(reinterpret_cast<Aspect_Handle>(winId()));
#elif defined(__APPLE__)
    Handle(Cocoa_Window) window = new Cocoa_Window(reinterpret_cast<NSView*>(winId()));
#else
    Handle(Xw_Window) window = new Xw_Window(display, static_cast<Aspect_Drawable>(winId()));
#endif

    m_view->SetWindow(window);
    if (!window->IsMapped())
        window->Map();

    // Jednolite ciemnoszare tło (#1E1E1E). sRGB = kolor tak, jak podaje go grafik/edytor.
    m_view->SetBackgroundColor(Quantity_Color(30 / 255.0, 30 / 255.0, 30 / 255.0, Quantity_TOC_sRGB));
    m_view->MustBeResized();

    showOriginAxes();
}

void OccView::showOriginAxes()
{
    // Osie układu współrzędnych w punkcie 0,0,0 – widać, gdzie leży detal względem zera.
    Handle(AIS_Trihedron) axes = new AIS_Trihedron(new Geom_Axis2Placement(gp::XOY()));
    axes->SetDatumDisplayMode(Prs3d_DM_Shaded); // pełne strzałki zamiast linii
    axes->SetSize(80.0);                        // długość osi w pikselach (patrz niżej)
    // Grubość osi i wielkość grotów – jako ułamek długości osi.
    const Handle(Prs3d_DatumAspect)& look = axes->Attributes()->DatumAspect();
    look->SetAttribute(Prs3d_DatumAttribute_ShadingTubeRadiusPercent, 0.025);
    look->SetAttribute(Prs3d_DatumAttribute_ShadingConeRadiusPercent, 0.07);
    look->SetAttribute(Prs3d_DatumAttribute_ShadingConeLengthPercent, 0.20);
    look->SetAttribute(Prs3d_DatumAttribute_ShadingOriginRadiusPercent, 0.05);
    // Kolory jak w programach CAD: X czerwony, Y zielony, Z niebieski (oś + grot).
    axes->SetDatumPartColor(Prs3d_DatumParts_XAxis, Quantity_NOC_RED);
    axes->SetDatumPartColor(Prs3d_DatumParts_XArrow, Quantity_NOC_RED);
    axes->SetDatumPartColor(Prs3d_DatumParts_YAxis, Quantity_NOC_GREEN);
    axes->SetDatumPartColor(Prs3d_DatumParts_YArrow, Quantity_NOC_GREEN);
    axes->SetDatumPartColor(Prs3d_DatumParts_ZAxis, Quantity_NOC_BLUE1);
    axes->SetDatumPartColor(Prs3d_DatumParts_ZArrow, Quantity_NOC_BLUE1);
    axes->SetDatumPartColor(Prs3d_DatumParts_Origin, Quantity_NOC_GREEN);
    axes->SetTextColor(Prs3d_DatumParts_XAxis, Quantity_NOC_RED);
    axes->SetTextColor(Prs3d_DatumParts_YAxis, Quantity_NOC_GREEN);
    axes->SetTextColor(Prs3d_DatumParts_ZAxis, Quantity_NOC_BLUE1);

    // ZoomPers: osie stoją w punkcie 0,0,0, ale mają stały rozmiar na ekranie,
    // niezależnie od przybliżenia – jak w programach CAD/CAM.
    axes->SetTransformPersistence(new Graphic3d_TransformPers(Graphic3d_TMF_ZoomPers, gp::Origin()));
    // Osie nie liczą się do "Dopasuj do okna" (F) – dopasowujemy tylko detal.
    axes->SetInfiniteState(Standard_True);

    // -1 = obiektu nie da się zaznaczyć myszką.
    m_context->Display(axes, 0, -1, Standard_False);
}

namespace {
// Wybrana bryła: kolory lekko przesunięte w stronę jasnoniebieskiego ("filtr").
Quantity_Color tint(const Quantity_Color& c, bool on)
{
    if (!on)
        return c;
    double r, g, b;
    c.Values(r, g, b, Quantity_TOC_sRGB);
    return Quantity_Color(0.55 * r + 0.45 * 0.55, 0.55 * g + 0.45 * 0.80, 0.55 * b + 0.45 * 1.00,
                          Quantity_TOC_sRGB);
}
}

Handle(AIS_InteractiveObject) OccView::makeModelPresentation(const camcore::ImportedModel& model, bool tinted) const
{
    // AIS_ColoredShape = bryła, której fragmenty mogą mieć różne kolory.
    // Geometria trafia na ekran dokładnie tam, gdzie leży w pliku – nic nie przesuwamy.
    Handle(AIS_ColoredShape) ais = new AIS_ColoredShape(model.shape);
    // Fragmenty bez koloru w pliku: neutralny jasnoszary.
    ais->SetColor(tint(Quantity_Color(0.70, 0.70, 0.70, Quantity_TOC_sRGB), tinted));
    // SetColor przemalowuje też krawędzie – przywracamy im czarny kolor.
    const Handle(Prs3d_LineAspect) blackEdges =
        new Prs3d_LineAspect(Quantity_NOC_BLACK, Aspect_TOL_SOLID, 1.0);
    ais->Attributes()->SetFaceBoundaryDraw(Standard_True);
    ais->Attributes()->SetFaceBoundaryAspect(blackEdges);

    // Kolory z pliku. Każdy kolorowany fragment dostaje własny zestaw ustawień
    // (CustomAspects), więc czarne krawędzie trzeba mu ustawić osobno.
    for (const camcore::ShapeColor& c : model.colors) {
        ais->SetCustomColor(c.shape, tint(c.color, tinted));
        const Handle(AIS_ColoredDrawer)& aspects = ais->CustomAspects(c.shape);
        aspects->SetFaceBoundaryDraw(Standard_True);
        aspects->SetFaceBoundaryAspect(blackEdges);
    }

    return ais;
}

void OccView::showModel(const camcore::ImportedModel& model)
{
    initViewer();

    if (!m_model.IsNull())
        m_context->Remove(m_model, Standard_False); // osie zostają
    showGeometry({}); // geometria dotyczyła starej bryły

    m_modelData = model;
    m_modelSelected = false;
    m_modelVisible = true;
    m_model = makeModelPresentation(model, false);
    m_context->Display(m_model, AIS_Shaded, 0, Standard_False);

    m_view->SetProj(V3d_XposYnegZpos); // widok izometryczny
    // Okno mogło właśnie zmienić rozmiar (np. plik podany przy starcie programu) –
    // bez tego FitAll liczyłby dopasowanie dla starego rozmiaru.
    m_view->MustBeResized();
    fitAll();
}

namespace {
constexpr double kLineWidth = 2.5;
constexpr double kHighlightWidth = 4.0;
}

void OccView::showGeometry(const std::vector<Contour>& contours)
{
    if (m_context.IsNull())
        return;
    for (const Handle(AIS_Shape)& g : m_geometry)
        m_context->Remove(g, Standard_False);
    m_geometry.clear();
    m_geometryColors.clear();
    m_geometryLines.clear();
    m_geometryVisible.clear();
    m_highlighted.clear();
    m_selected.clear();
    m_hovered = -1;

    for (const Contour& c : contours) {
        Handle(AIS_Shape) item;
        if (c.height > 1e-6) {
            // Kontur "wyciągnięty" w dół o wysokość ścian (pryzmat): ścianka
            // od górnego do dolnego konturu, z krawędziami u góry, u dołu i w narożach.
            const TopoDS_Shape walls =
                BRepPrimAPI_MakePrism(c.shape, gp_Vec(0, 0, -c.height)).Shape();
            item = new AIS_Shape(walls);
            item->SetColor(c.color);
            item->SetTransparency(0.35);
            item->Attributes()->SetFaceBoundaryDraw(Standard_True);
            item->Attributes()->SetFaceBoundaryAspect(
                new Prs3d_LineAspect(c.color, Aspect_TOL_SOLID, kLineWidth));
            // Ścianka leży dokładnie na ścianach bryły – przesuwamy ją minimalnie
            // w stronę kamery, żeby nie migotała (z-fighting).
            item->Attributes()->ShadingAspect()->Aspect()->SetPolygonOffsets(Aspect_POM_Fill, -1.0f, -1.0f);
        } else {
            // Kontur bez wysokości – same linie.
            item = new AIS_Shape(c.shape);
            item->SetColor(c.color);
            item->SetWidth(kLineWidth);
        }
        // Warstwa "Top": rysowana po bryle, więc linie leżące na ścianach
        // nie giną pod czarnymi krawędziami. Bryła dalej zasłania to, co za nią.
        item->SetZLayer(Graphic3d_ZLayerId_Top);
        m_context->Display(item, c.height > 1e-6 ? AIS_Shaded : AIS_WireFrame, -1,
                           Standard_False); // -1: nie do zaznaczania
        m_geometry.push_back(item);
        m_geometryColors.push_back(c.color);
        m_geometryVisible.push_back(true);

        // Punkty wzdłuż konturu (u góry i u dołu ścianki) – po nich trafiamy myszką.
        std::vector<std::vector<gp_Pnt>> lines;
        for (TopExp_Explorer ex(c.shape, TopAbs_EDGE); ex.More(); ex.Next()) {
            const BRepAdaptor_Curve curve(TopoDS::Edge(ex.Current()));
            const int n = curve.GetType() == GeomAbs_Line ? 1 : 32;
            std::vector<gp_Pnt> top, bottom;
            for (int k = 0; k <= n; ++k) {
                const double t = curve.FirstParameter() + (curve.LastParameter() - curve.FirstParameter()) * k / n;
                const gp_Pnt p = curve.Value(t);
                top.push_back(p);
                bottom.push_back(gp_Pnt(p.X(), p.Y(), p.Z() - c.height));
            }
            lines.push_back(top);
            if (c.height > 1e-6)
                lines.push_back(bottom);
        }
        m_geometryLines.push_back(lines);
    }
    m_view->Redraw();
}

void OccView::highlightGeometry(const std::vector<int>& indices)
{
    m_highlighted = indices;
    refreshGeometryLook();
}

// Kolor i grubość każdej geometrii według stanu: wybrana (jasnoniebieska),
// pod kursorem (jasnożółta), podświetlona z panelu (pomarańczowa), zwykła.
void OccView::refreshGeometryLook()
{
    auto has = [](const std::vector<int>& v, int i) { return std::find(v.begin(), v.end(), i) != v.end(); };
    for (size_t i = 0; i < m_geometry.size(); ++i) {
        const int idx = static_cast<int>(i);
        Quantity_Color color = m_geometryColors[i];
        double width = kLineWidth;
        if (has(m_selected, idx)) {
            color = Quantity_Color(0.62, 0.82, 1.00, Quantity_TOC_sRGB);
            width = kHighlightWidth;
        } else if (idx == m_hovered) {
            color = Quantity_Color(1.00, 0.93, 0.62, Quantity_TOC_sRGB);
            width = kHighlightWidth;
        } else if (has(m_highlighted, idx)) {
            color = Quantity_Color(Quantity_NOC_ORANGE);
            width = kHighlightWidth;
        }
        const Handle(AIS_Shape)& g = m_geometry[i];
        const double transparency = g->Transparency();
        g->SetColor(color);
        g->SetWidth(width);
        if (g->Attributes()->FaceBoundaryDraw()) {
            g->SetTransparency(transparency); // SetColor przywraca nieprzezroczystość
            g->Attributes()->SetFaceBoundaryAspect(new Prs3d_LineAspect(color, Aspect_TOL_SOLID, width));
        }
        m_context->Redisplay(g, Standard_False);
    }
    m_view->Redraw();
}

int OccView::geometryAt(const QPoint& pos) const
{
    // Geometrię wskazujemy po jej liniach: najbliższa w promieniu kilku pikseli.
    const double maxDist = 6.0 * devicePixelRatioF();
    int best = -1;
    double bestDist = maxDist;
    for (size_t i = 0; i < m_geometryLines.size(); ++i) {
        if (!m_geometryVisible[i])
            continue;
        for (const std::vector<gp_Pnt>& line : m_geometryLines[i]) {
            for (size_t k = 0; k + 1 < line.size(); ++k) {
                Standard_Integer ax, ay, bx, by;
                m_view->Convert(line[k].X(), line[k].Y(), line[k].Z(), ax, ay);
                m_view->Convert(line[k + 1].X(), line[k + 1].Y(), line[k + 1].Z(), bx, by);
                // Odległość kursora od odcinka na ekranie.
                const double dx = bx - ax, dy = by - ay;
                const double len2 = dx * dx + dy * dy;
                const double t = std::clamp(len2 > 0 ? ((pos.x() - ax) * dx + (pos.y() - ay) * dy) / len2 : 0.0,
                                            0.0, 1.0);
                const double d = std::hypot(pos.x() - (ax + t * dx), pos.y() - (ay + t * dy));
                if (d < bestDist) {
                    bestDist = d;
                    best = static_cast<int>(i);
                }
            }
        }
    }
    return best;
}

bool OccView::modelAt(const QPoint& pos) const
{
    if (m_model.IsNull() || !m_modelVisible)
        return false;
    // Pytamy selektor OCCT o wszystko pod kursorem i szukamy wśród trafień bryły
    // (pierwsze trafienie może być np. przezroczystą ścianką konturu).
    const Handle(StdSelect_ViewerSelector3d)& selector = m_context->MainSelector();
    selector->Pick(pos.x(), pos.y(), m_view);
    for (Standard_Integer i = 1; i <= selector->NbPicked(); ++i) {
        const Handle(SelectMgr_EntityOwner)& owner = selector->Picked(i);
        if (!owner.IsNull() && owner->Selectable() == m_model)
            return true;
    }
    return false;
}

void OccView::updateModel(const camcore::ImportedModel& model)
{
    if (m_context.IsNull())
        return;
    m_modelData = model;
    rebuildModel();
}

void OccView::rebuildModel()
{
    if (!m_model.IsNull())
        m_context->Remove(m_model, Standard_False);
    m_model = makeModelPresentation(m_modelData, m_modelSelected);
    if (m_modelVisible)
        m_context->Display(m_model, AIS_Shaded, 0, Standard_False);
    m_view->Redraw();
}

void OccView::setGeometryVisible(int index, bool visible)
{
    if (index < 0 || index >= static_cast<int>(m_geometry.size()))
        return;
    const Handle(AIS_Shape)& g = m_geometry[static_cast<size_t>(index)];
    m_geometryVisible[static_cast<size_t>(index)] = visible;
    if (visible)
        m_context->Display(g, Standard_False); // wraca z tym samym trybem i warstwą
    else
        m_context->Erase(g, Standard_False);
    m_view->Redraw();
}

void OccView::setModelVisible(bool visible)
{
    if (m_model.IsNull())
        return;
    m_modelVisible = visible;
    if (visible)
        m_context->Display(m_model, Standard_False);
    else
        m_context->Erase(m_model, Standard_False);
    m_view->Redraw();
}

void OccView::fitAll()
{
    if (m_view.IsNull())
        return;
    m_view->FitAll(0.05, Standard_False);
    m_view->ZFitAll();
    m_view->Redraw();
}

void OccView::paintEvent(QPaintEvent*)
{
    initViewer();
    m_view->Redraw();
}

void OccView::resizeEvent(QResizeEvent*)
{
    if (m_view.IsNull())
        return;
    // MustBeResized odczytuje nowy rozmiar okna. Na Windows po maksymalizacji
    // (albo zmniejszeniu okna) Qt nie zawsze wysyła potem paintEvent dla widżetu,
    // który maluje sam (WA_PaintOnScreen) – wtedy obraz zostawał w starym rozmiarze.
    // Dlatego od razu przerysowujemy scenę.
    m_view->MustBeResized();
    m_view->Redraw();
}

QPoint OccView::toPixels(const QPointF& p) const
{
    // Qt podaje współrzędne logiczne, OCCT oczekuje fizycznych pikseli (ekrany HiDPI).
    const qreal r = devicePixelRatioF();
    return QPoint(qRound(p.x() * r), qRound(p.y() * r));
}

void OccView::mousePressEvent(QMouseEvent* e)
{
    if (m_view.IsNull())
        return;
    m_lastPos = toPixels(e->position());
    m_pressPos = m_lastPos;
    if (e->button() == Qt::LeftButton)
        m_view->StartRotation(m_lastPos.x(), m_lastPos.y());
}

void OccView::mouseReleaseEvent(QMouseEvent* e)
{
    if (m_view.IsNull())
        return;
    const QPoint pos = toPixels(e->position());
    // Kliknięcie = puszczenie przycisku prawie w miejscu wciśnięcia (bez przeciągania).
    if ((pos - m_pressPos).manhattanLength() <= 4)
        onClick(e->button(), pos);
}

void OccView::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape)
        emit cancelRequested();
    else
        QWidget::keyPressEvent(e);
}

bool OccView::isSelectable(const Handle(AIS_InteractiveObject)& obj) const
{
    if (obj.IsNull())
        return false;
    if (obj == m_model)
        return true;
    for (const Handle(AIS_Shape)& g : m_geometry)
        if (obj == g)
            return true;
    return false; // np. osie układu współrzędnych
}

void OccView::onClick(Qt::MouseButton button, const QPoint& pos)
{
    if (m_interaction == Interaction::Select) {
        if (button == Qt::LeftButton) {
            // Kliknięty element dokładamy do wyboru albo – gdy już był – odejmujemy.
            // Najpierw geometrie (wskazywane po liniach), potem bryła.
            const int g = geometryAt(pos);
            if (g >= 0) {
                auto it = std::find(m_selected.begin(), m_selected.end(), g);
                if (it == m_selected.end())
                    m_selected.push_back(g); // dokładamy do wyboru
                else
                    m_selected.erase(it);    // drugie kliknięcie odejmuje
                refreshGeometryLook();
                emit selectionChanged(selectionCount());
            } else {
                if (modelAt(pos)) {
                    m_modelSelected = !m_modelSelected; // drugie kliknięcie odznacza
                    rebuildModel();
                    emit selectionChanged(selectionCount());
                }
            }
            m_view->Redraw();
        } else if (button == Qt::RightButton) {
            emit selectionConfirmed();
        }
    } else if (m_interaction == Interaction::PickPoint && button == Qt::LeftButton) {
        // Promień spod kursora przecinamy z płaszczyzną Z = 0 (płaszczyzna stołu).
        double x, y, z, vx, vy, vz;
        m_view->ConvertWithProj(pos.x(), pos.y(), x, y, z, vx, vy, vz);
        if (std::abs(vz) > 1e-9) {
            const double t = -z / vz;
            x += t * vx;
            y += t * vy;
        }
        emit pointPicked(x, y, 0.0);
    }
}

void OccView::setInteraction(Interaction mode)
{
    if (m_context.IsNull())
        return;
    m_interaction = mode;
    if (mode == Interaction::Select) {
        // Bryłę wskazuje OCCT (tryb 0 = cała bryła), geometrie – geometryAt.
        if (!m_model.IsNull())
            m_context->Activate(m_model, 0);
    } else if (mode == Interaction::Navigate) {
        clearSelection();
    }
    m_context->ClearDetected(Standard_False);
    m_view->Redraw();
    setCursor(mode == Interaction::Navigate ? Qt::ArrowCursor : Qt::CrossCursor);
}

std::vector<int> OccView::selectedGeometries() const
{
    return m_selected;
}

bool OccView::isModelSelected() const
{
    return !m_model.IsNull() && m_modelSelected;
}

int OccView::selectionCount() const
{
    return static_cast<int>(selectedGeometries().size()) + (isModelSelected() ? 1 : 0);
}

void OccView::clearSelection()
{
    if (m_context.IsNull())
        return;
    if (m_modelSelected) {
        m_modelSelected = false;
        rebuildModel();
    }
    m_selected.clear();
    m_hovered = -1;
    refreshGeometryLook();
}

void OccView::mouseMoveEvent(QMouseEvent* e)
{
    if (m_view.IsNull())
        return;
    const QPoint pos = toPixels(e->position());

    if (e->buttons() & Qt::LeftButton) {
        m_view->Rotation(pos.x(), pos.y());
    } else if (e->buttons() & (Qt::MiddleButton | Qt::RightButton)) {
        m_view->Pan(pos.x() - m_lastPos.x(), m_lastPos.y() - pos.y());
    }
    m_lastPos = pos;
}

void OccView::wheelEvent(QWheelEvent* e)
{
    if (m_view.IsNull())
        return;
    const QPoint pos = toPixels(e->position());
    const int step = e->angleDelta().y() > 0 ? 15 : -15;

    // Zoom w stronę kursora, jak w typowych programach CAD.
    m_view->StartZoomAtPoint(pos.x(), pos.y());
    m_view->ZoomAtPoint(pos.x(), pos.y(), pos.x() + step, pos.y() + step);
}
