#include "OccView.h"

#include <AIS_Shape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <TopoDS.hxx>

#include <QKeyEvent>
#include <QMouseEvent>
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

    // Kolory podświetlenia ścian ("Local" = fragment bryły, nie cała bryła):
    // pod kursorem – jasnoniebieski, wybrana – pomarańczowy.
    const Handle(Prs3d_Drawer)& hover = m_context->HighlightStyle(Prs3d_TypeOfHighlight_LocalDynamic);
    hover->SetColor(Quantity_Color(0.35, 0.75, 1.0, Quantity_TOC_RGB));
    hover->SetDisplayMode(AIS_Shaded);
    const Handle(Prs3d_Drawer)& picked = m_context->HighlightStyle(Prs3d_TypeOfHighlight_LocalSelected);
    picked->SetColor(Quantity_NOC_ORANGE);
    picked->SetDisplayMode(AIS_Shaded);

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

    m_view->SetBgGradientColors(Quantity_Color(0.30, 0.34, 0.40, Quantity_TOC_RGB),
                                Quantity_Color(0.10, 0.11, 0.13, Quantity_TOC_RGB),
                                Aspect_GFM_VER);
    m_view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE, 0.08, V3d_ZBUFFER);
    m_view->MustBeResized();
}

void OccView::showShape(const TopoDS_Shape& shape)
{
    initViewer();

    m_context->RemoveAll(Standard_False);
    m_shape = new AIS_Shape(shape);
    // Neutralny szary materiał – na nim pomarańczowy wybór jest dobrze widoczny.
    m_shape->SetColor(Quantity_Color(0.72, 0.74, 0.77, Quantity_TOC_RGB));
    // SetColor przemalowuje też krawędzie – przywracamy im czarny kolor.
    m_shape->Attributes()->SetFaceBoundaryDraw(Standard_True);
    m_shape->Attributes()->SetFaceBoundaryAspect(
        new Prs3d_LineAspect(Quantity_NOC_BLACK, Aspect_TOL_SOLID, 1.0));
    // Tryb wyboru: pojedyncze ściany (TopAbs_FACE) zamiast całej bryły (tryb 0).
    // Dzięki temu kliknięcie wskazuje konkretną ścianę, np. dno kieszeni.
    m_context->Display(m_shape, AIS_Shaded, AIS_Shape::SelectionMode(TopAbs_FACE),
                       Standard_False);

    m_view->SetProj(V3d_XposYnegZpos); // widok izometryczny
    // Okno mogło właśnie zmienić rozmiar (np. plik podany przy starcie programu) –
    // bez tego FitAll liczyłby dopasowanie dla starego rozmiaru.
    m_view->MustBeResized();
    fitAll();
    emit selectionChanged(); // stary wybór zniknął razem ze starą bryłą
}

void OccView::clearSelection()
{
    if (m_context.IsNull())
        return;
    m_context->ClearSelected(Standard_True);
    emit selectionChanged();
}

std::vector<TopoDS_Face> OccView::selectedFaces() const
{
    std::vector<TopoDS_Face> faces;
    if (m_context.IsNull())
        return faces;
    // Kontekst przechowuje listę wybranych "właścicieli" (owner) – tu każdy to jedna ściana.
    for (m_context->InitSelected(); m_context->MoreSelected(); m_context->NextSelected()) {
        const TopoDS_Shape s = m_context->SelectedShape();
        if (!s.IsNull() && s.ShapeType() == TopAbs_FACE)
            faces.push_back(TopoDS::Face(s));
    }
    return faces;
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
    if (!m_view.IsNull())
        m_view->MustBeResized();
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
    if (e->button() == Qt::LeftButton) {
        // Jeszcze nie wiemy, czy to kliknięcie (wybór), czy przeciąganie (obrót).
        // Rozstrzygnie się w mouseMoveEvent / mouseReleaseEvent.
        m_pressPos = m_lastPos;
        m_rotating = false;
    }
}

void OccView::mouseReleaseEvent(QMouseEvent* e)
{
    if (m_view.IsNull() || e->button() != Qt::LeftButton)
        return;
    if (m_rotating) {
        m_rotating = false;
        return;
    }

    // Kliknięcie bez przeciągania = wybór ściany pod kursorem.
    // MoveTo ustala, co jest pod kursorem ("detected"), SelectDetected to zaznacza.
    const QPoint pos = toPixels(e->position());
    m_context->MoveTo(pos.x(), pos.y(), m_view, Standard_False);
    const AIS_SelectionScheme scheme = (e->modifiers() & Qt::ControlModifier)
                                           ? AIS_SelectionScheme_XOR      // dodaj/usuń
                                           : AIS_SelectionScheme_Replace; // tylko ta
    m_context->SelectDetected(scheme);
    m_context->UpdateCurrentViewer();
    emit selectionChanged();
}

void OccView::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        clearSelection();
        return;
    }
    QWidget::keyPressEvent(e);
}

void OccView::mouseMoveEvent(QMouseEvent* e)
{
    if (m_view.IsNull())
        return;
    const QPoint pos = toPixels(e->position());

    if (e->buttons() & Qt::LeftButton) {
        // Obrót startuje dopiero po przesunięciu o kilka pikseli –
        // inaczej każde kliknięcie lekko obracałoby widok.
        if (!m_rotating && (pos - m_pressPos).manhattanLength() > 4) {
            m_view->StartRotation(m_pressPos.x(), m_pressPos.y());
            m_rotating = true;
        }
        if (m_rotating)
            m_view->Rotation(pos.x(), pos.y());
    } else if (e->buttons() & (Qt::MiddleButton | Qt::RightButton)) {
        m_view->Pan(pos.x() - m_lastPos.x(), m_lastPos.y() - pos.y());
    } else {
        // Bez wciśniętych przycisków: podświetl ścianę pod kursorem.
        m_context->MoveTo(pos.x(), pos.y(), m_view, Standard_True);
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
