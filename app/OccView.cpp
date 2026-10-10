#include "OccView.h"

#include <AIS_ColoredShape.hxx>
#include <AIS_Shape.hxx>
#include <AIS_Trihedron.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Geom_Axis2Placement.hxx>
#include <Graphic3d_TransformPers.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Prs3d_DatumAspect.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>

#include <QMouseEvent>

#include <algorithm>
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

void OccView::showModel(const camcore::ImportedModel& model)
{
    initViewer();

    if (!m_model.IsNull())
        m_context->Remove(m_model, Standard_False); // osie zostają
    showGeometry({}); // geometria dotyczyła starej bryły

    // AIS_ColoredShape = bryła, której fragmenty mogą mieć różne kolory.
    // Geometria trafia na ekran dokładnie tam, gdzie leży w pliku – nic nie przesuwamy.
    Handle(AIS_ColoredShape) ais = new AIS_ColoredShape(model.shape);
    // Fragmenty bez koloru w pliku: neutralny jasnoszary.
    ais->SetColor(Quantity_Color(0.70, 0.70, 0.70, Quantity_TOC_sRGB));
    // SetColor przemalowuje też krawędzie – przywracamy im czarny kolor.
    const Handle(Prs3d_LineAspect) blackEdges =
        new Prs3d_LineAspect(Quantity_NOC_BLACK, Aspect_TOL_SOLID, 1.0);
    ais->Attributes()->SetFaceBoundaryDraw(Standard_True);
    ais->Attributes()->SetFaceBoundaryAspect(blackEdges);

    // Kolory z pliku. Każdy kolorowany fragment dostaje własny zestaw ustawień
    // (CustomAspects), więc czarne krawędzie trzeba mu ustawić osobno.
    for (const camcore::ShapeColor& c : model.colors) {
        ais->SetCustomColor(c.shape, c.color);
        const Handle(AIS_ColoredDrawer)& aspects = ais->CustomAspects(c.shape);
        aspects->SetFaceBoundaryDraw(Standard_True);
        aspects->SetFaceBoundaryAspect(blackEdges);
    }

    m_model = ais;
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

    for (const Contour& c : contours) {
        // Sam kontur (krawędzie) – AIS_Shape rysuje go jako linie.
        Handle(AIS_Shape) line = new AIS_Shape(c.shape);
        line->SetColor(c.color);
        line->SetWidth(kLineWidth);
        // Warstwa "Top": rysowana po bryle, więc linie leżące na ścianach
        // nie giną pod czarnymi krawędziami. Bryła dalej zasłania to, co za nią.
        line->SetZLayer(Graphic3d_ZLayerId_Top);
        m_context->Display(line, AIS_WireFrame, -1, Standard_False); // -1: nie do zaznaczania
        m_geometry.push_back(line);
        m_geometryColors.push_back(c.color);
    }
    m_view->Redraw();
}

void OccView::highlightGeometry(const std::vector<int>& indices)
{
    for (size_t i = 0; i < m_geometry.size(); ++i) {
        const bool on = std::find(indices.begin(), indices.end(), static_cast<int>(i)) != indices.end();
        m_geometry[i]->SetColor(on ? Quantity_Color(Quantity_NOC_ORANGE) : m_geometryColors[i]);
        m_geometry[i]->SetWidth(on ? kHighlightWidth : kLineWidth);
        m_context->Redisplay(m_geometry[i], Standard_False);
    }
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
    if (e->button() == Qt::LeftButton)
        m_view->StartRotation(m_lastPos.x(), m_lastPos.y());
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
