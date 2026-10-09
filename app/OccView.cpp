#include "OccView.h"

#include <AIS_Shape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>

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
    Handle(AIS_Shape) ais = new AIS_Shape(shape);
    m_context->Display(ais, AIS_Shaded, 0, Standard_False);

    m_view->SetProj(V3d_XposYnegZpos); // widok izometryczny
    fitAll();
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
