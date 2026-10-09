#pragma once

#include <QPoint>
#include <QWidget>

#include <AIS_InteractiveContext.hxx>
#include <TopoDS_Shape.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

// Widżet Qt, w którym Open CASCADE rysuje scenę 3D.
// Sterowanie: LPM – obrót, ŚPM/PPM – przesuwanie, kółko – zoom.
class OccView : public QWidget
{
    Q_OBJECT

public:
    explicit OccView(QWidget* parent = nullptr);

    void showShape(const TopoDS_Shape& shape);
    void fitAll();

    // OCCT rysuje sam, Qt nie może malować po tym widżecie.
    QPaintEngine* paintEngine() const override { return nullptr; }

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private:
    void initViewer();
    QPoint toPixels(const QPointF& p) const;

    Handle(V3d_Viewer) m_viewer;
    Handle(V3d_View) m_view;
    Handle(AIS_InteractiveContext) m_context;
    QPoint m_lastPos;
};
