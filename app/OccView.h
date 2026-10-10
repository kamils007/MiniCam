#pragma once

#include <QCursor>
#include <QPoint>
#include <QWidget>

#include <vector>

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <Quantity_Color.hxx>
#include <TopoDS_Shape.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <gp_Pnt.hxx>

#include "ModelImport.h"

// Widżet Qt, w którym Open CASCADE rysuje scenę 3D.
// Sterowanie: LPM – obrót, ŚPM/PPM – przesuwanie, kółko – zoom.
// W trybie wyboru kliknięcie LPM zaznacza element, kliknięcie PPM zatwierdza wybór;
// w trybie wskazywania punktu kliknięcie LPM podaje punkt na płaszczyźnie Z = 0.
class OccView : public QWidget
{
    Q_OBJECT

public:
    explicit OccView(QWidget* parent = nullptr);

    // Co robi kliknięcie myszą (przeciąganie zawsze obraca/przesuwa widok).
    enum class Interaction { Navigate, Select, PickPoint };
    void setInteraction(Interaction mode);

    // Uchwyty (przyciąganie) przy wskazywaniu punktu: kursor "klei się" do wybranych
    // punktów geometrii (zamiast krzyża pokazuje znacznik), kliknięcie daje ten punkt.
    enum class Snap { None, Auto, End, Mid, Centre, Quadrant };
    void setSnap(Snap snap, const QCursor& cursor = QCursor());

    // Zaznaczone elementy (tryb wyboru): numery geometrii i czy bryła.
    std::vector<int> selectedGeometries() const;
    bool isModelSelected() const;
    int selectionCount() const;
    void clearSelection();

    // Podmienia bryłę (np. po przesunięciu) bez zmiany widoku i bez kasowania geometrii.
    void updateModel(const camcore::ImportedModel& model);

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

    // Widoczność (checkboxy w panelu Warstwy).
    void setGeometryVisible(int index, bool visible);
    void setModelVisible(bool visible);

    // OCCT rysuje sam, Qt nie może malować po tym widżecie.
    QPaintEngine* paintEngine() const override { return nullptr; }

signals:
    void selectionChanged(int count);
    void selectionConfirmed();                     // PPM w trybie wyboru
    void pointPicked(double x, double y, double z); // LPM w trybie wskazywania punktu
    void cancelRequested();                        // Esc
    void cursorMoved(double x, double y);          // kursor na płaszczyźnie Z = 0

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    void initViewer();
    void showOriginAxes();
    Handle(AIS_InteractiveObject) makeModelPresentation(const camcore::ImportedModel& model, bool selected,
                                                        bool hovered) const;
    void rebuildModel(); // przerysowuje bryłę (np. po zmianie zaznaczenia)
    bool isSelectable(const Handle(AIS_InteractiveObject)& obj) const;
    void onClick(Qt::MouseButton button, const QPoint& pos);
    int geometryAt(const QPoint& pos) const; // -1 = żadna
    bool modelAt(const QPoint& pos) const;   // czy pod kursorem jest bryła
    gp_Pnt pointOnTable(const QPoint& pos) const; // punkt spod kursora na płaszczyźnie Z = 0
    void refreshGeometryLook();
    void updateHover(const QPoint& pos); // podświetla to, co jest pod kursorem (tryb wyboru)
    void clearHover();
    void showCrosshair(const QPoint& pos); // krzyż linii X/Y/Z przy kursorze (wskazywanie punktu)
    void hideCrosshair();
    void updatePickFeedback(const QPoint& pos);  // krzyż albo znacznik uchwytu pod kursorem
    bool snapAt(const QPoint& pos, gp_Pnt& out) const; // najbliższy punkt uchwytu przy kursorze
    QPoint toPixels(const QPointF& p) const;

    Handle(V3d_Viewer) m_viewer;
    Handle(V3d_View) m_view;
    Handle(AIS_InteractiveContext) m_context;
    Handle(AIS_InteractiveObject) m_model; // wczytany detal
    camcore::ImportedModel m_modelData;    // jego dane – do przerysowania
    bool m_modelSelected = false;          // bryła wybrana (lekko podbarwiona)
    bool m_modelHovered = false;           // bryła pod kursorem przy wyborze (wyszarzona)
    bool m_modelVisible = true;
    std::vector<Handle(AIS_Shape)> m_geometry; // narysowane kontury
    std::vector<Quantity_Color> m_geometryColors; // ich kolory bez podświetlenia
    // Punkty konturów (góra i dół ścianki) do wskazywania geometrii myszką.
    std::vector<std::vector<std::vector<gp_Pnt>>> m_geometryLines; // [geometria][łamana][punkt]
    std::vector<bool> m_geometryVisible;
    std::vector<int> m_highlighted;    // podświetlone z panelu Warstwy (pomarańczowe)
    std::vector<int> m_selected;       // wybrane w trybie wyboru
    Handle(AIS_Shape) m_crosshair;     // krzyż linii przy kursorze
    Handle(AIS_InteractiveObject) m_snapMarker; // znacznik punktu uchwytu (biała kulka)
    Snap m_snap = Snap::None;
    QCursor m_snapCursor; // strzałka z ikonką aktywnego uchwytu
    struct SnapPoint
    {
        Snap kind; // End, Mid, Centre albo Quadrant
        gp_Pnt point;
    };
    std::vector<std::vector<SnapPoint>> m_snapPoints; // [geometria] – punkty do przyciągania
    std::vector<SnapPoint> m_modelSnapPoints;         // punkty krawędzi bryły
    static void addSnapPoints(const TopoDS_Shape& shape, double height, std::vector<SnapPoint>& out);
    int m_hovered = -1;                // geometria pod kursorem w trybie wyboru (przerywana linia)
    QPoint m_lastPos;
    QPoint m_pressPos; // gdzie wciśnięto przycisk – odróżnia kliknięcie od przeciągania
    Interaction m_interaction = Interaction::Navigate;
};
