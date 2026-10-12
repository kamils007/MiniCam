#pragma once

#include <QList>
#include <QPoint>
#include <QWidget>

#include <vector>

class QAction;

// Jedno pole menu radialnego: polecenie (nullptr = puste pole) albo czarne strzałki »,
// które po najechaniu rozszerzają kółko o zewnętrzny łuk z dodatkowymi polami.
struct RadialSlot
{
    QAction* action = nullptr;
    bool expands = false;    // pole z » (rozszerza kółko)
    QList<QAction*> more;    // pola na zewnętrznym łuku (nullptr = puste)
};

// Menu radialne pod PPM (jak w Alphacam): jeden pierścień pól wokół kursora,
// ułożonych od godz. 3 zgodnie z ruchem wskazówek zegara (przy 14 polach środek
// pola 11 wypada dokładnie na godz. 12).
class RadialMenu : public QWidget
{
    Q_OBJECT

public:
    RadialMenu(const std::vector<RadialSlot>& items, QWidget* parent = nullptr);

    // Pokazuje menu ze środkiem w punkcie ekranu (tam, gdzie kliknięto PPM).
    void popup(const QPoint& globalCenter);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    struct Hit
    {
        int ring = -1;  // 0 = pierścień, 1 = zewnętrzny łuk, -1 = nic
        int index = -1;

        bool operator==(const Hit& o) const { return ring == o.ring && index == o.index; }
    };

    QPointF center() const;
    double step() const;                // szerokość pola pierścienia w stopniach
    double slotAngle(int index) const;  // środek pola (stopnie, przeciwnie do zegara od godz. 3)
    double outerAngle(int index) const; // środek pola na łuku otwartych strzałek
    Hit hitTest(const QPointF& pos) const;
    QAction* actionAt(const Hit& hit) const;
    QString tipFor(const Hit& hit) const;
    QRect tipRect() const; // ramka podpowiedzi pod kółkiem (pusta = brak)
    void updateMask();

    std::vector<RadialSlot> m_slots;
    int m_open = -1; // pole z », którego łuk jest rozwinięty
    Hit m_hover;
};
