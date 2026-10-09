#pragma once

#include <QPoint>
#include <QWidget>

#include <vector>

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

// Widżet Qt, w którym Open CASCADE rysuje scenę 3D.
// Sterowanie: LPM – obrót (przeciągnięcie) lub wybór ściany (kliknięcie),
// Ctrl+LPM – dodaj/usuń ścianę z wyboru, Esc – wyczyść wybór,
// ŚPM/PPM – przesuwanie, kółko – zoom.
class OccView : public QWidget
{
    Q_OBJECT

public:
    explicit OccView(QWidget* parent = nullptr);

    void showShape(const TopoDS_Shape& shape);
    void fitAll();
    void clearSelection();

    // Ściany aktualnie wybrane myszką (w kolejności klikania).
    std::vector<TopoDS_Face> selectedFaces() const;

    // OCCT rysuje sam, Qt nie może malować po tym widżecie.
    QPaintEngine* paintEngine() const override { return nullptr; }

signals:
    // Wysyłany po każdej zmianie wyboru (kliknięcie, Esc, nowy plik).
    void selectionChanged();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private:
    void initViewer();
    QPoint toPixels(const QPointF& p) const;

    Handle(V3d_Viewer) m_viewer;
    Handle(V3d_View) m_view;
    Handle(AIS_InteractiveContext) m_context;
    Handle(AIS_Shape) m_shape;

    QPoint m_lastPos;
    QPoint m_pressPos;     // gdzie wciśnięto LPM
    bool m_rotating = false; // czy LPM przeszło już w tryb obrotu
};
