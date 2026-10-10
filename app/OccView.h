#pragma once

#include <QPoint>
#include <QWidget>

#include <vector>

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <Quantity_Color.hxx>
#include <TopoDS_Shape.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

#include "ModelImport.h"

// Widżet Qt, w którym Open CASCADE rysuje scenę 3D.
// Sterowanie: LPM – obrót, ŚPM/PPM – przesuwanie, kółko – zoom.
class OccView : public QWidget
{
    Q_OBJECT

public:
    explicit OccView(QWidget* parent = nullptr);

    // Pokazuje model w położeniu z pliku, z kolorami z pliku.
    void showModel(const camcore::ImportedModel& model);
    void fitAll();

    // Kontury rysowane grubymi liniami na bryle – wynik rozpoznania cech. Kontur
    // z wysokością dostaje pod sobą przezroczystą ściankę aż do dołu swoich ścian.
    struct Contour
    {
        TopoDS_Shape shape;   // kontur na górze ścian
        Quantity_Color color;
        double height = 0;    // wysokość ścian pod konturem [mm]
    };
    void showGeometry(const std::vector<Contour>& contours); // pusta lista = usuń geometrię
    // Wskazane kontury (indeksy z showGeometry) rysuje na pomarańczowo i grubiej.
    void highlightGeometry(const std::vector<int>& indices);

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
    void showOriginAxes();
    QPoint toPixels(const QPointF& p) const;

    Handle(V3d_Viewer) m_viewer;
    Handle(V3d_View) m_view;
    Handle(AIS_InteractiveContext) m_context;
    Handle(AIS_InteractiveObject) m_model; // wczytany detal
    std::vector<Handle(AIS_Shape)> m_geometry; // narysowane kontury
    std::vector<Quantity_Color> m_geometryColors; // ich kolory bez podświetlenia
    QPoint m_lastPos;
};
