#pragma once

#include <QList>
#include <QPoint>
#include <QWidget>

class QAction;

// Menu radialne pod PPM (jak w Alphacam): pierścień pól wokół kursora,
// ułożonych od godz. 3 zgodnie z ruchem wskazówek zegara.
// Pole to polecenie (QAction) albo nullptr – puste pole, uzupełniane na bieżąco.
class RadialMenu : public QWidget
{
    Q_OBJECT

public:
    RadialMenu(const QList<QAction*>& items, QWidget* parent = nullptr);

    // Pokazuje menu ze środkiem w punkcie ekranu (tam, gdzie kliknięto PPM).
    void popup(const QPoint& globalCenter);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    QPointF center() const;
    double step() const;               // szerokość pola w stopniach
    double slotAngle(int index) const; // środek pola (stopnie, przeciwnie do zegara od godz. 3)
    int slotAt(const QPointF& pos) const; // -1 = żadne
    QString tipFor(int index) const;
    QRect tipRect() const; // ramka podpowiedzi pod pierścieniem (pusta = brak)
    void updateMask();

    QList<QAction*> m_slots;
    int m_hover = -1;
};
