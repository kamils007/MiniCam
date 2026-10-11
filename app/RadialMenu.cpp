#include "RadialMenu.h"

#include <QAction>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QScreen>

#include <algorithm>
#include <cmath>

namespace {

// Wymiary pierścienia [px]: dziura w środku i zewnętrzny brzeg pól.
constexpr double kHole = 38;
constexpr double kRing = 92;
constexpr int kMargin = 4;
constexpr int kTipSpace = 44; // miejsce pod pierścieniem na podpowiedź

const QColor kFill(214, 229, 246);
const QColor kHover(255, 231, 150);
const QColor kDisabled(215, 215, 215);
const QColor kBorder(140, 170, 205);

constexpr double kPi = 3.14159265358979323846;

// Wycinek pierścienia między promieniami r0 i r1, kąty w stopniach (Qt: przeciwnie do zegara).
QPainterPath sector(const QPointF& c, double r0, double r1, double fromDeg, double spanDeg)
{
    QPainterPath path;
    const QRectF outer(c.x() - r1, c.y() - r1, 2 * r1, 2 * r1);
    const QRectF inner(c.x() - r0, c.y() - r0, 2 * r0, 2 * r0);
    path.arcMoveTo(outer, fromDeg);
    path.arcTo(outer, fromDeg, spanDeg);
    path.arcTo(inner, fromDeg + spanDeg, -spanDeg);
    path.closeSubpath();
    return path;
}

QPointF onCircle(const QPointF& c, double r, double deg)
{
    const double a = deg * kPi / 180.0;
    return {c.x() + r * std::cos(a), c.y() - r * std::sin(a)};
}

} // namespace

RadialMenu::RadialMenu(const QList<QAction*>& items, QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    , m_slots(items)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    const int side = int(2 * (kRing + kMargin));
    setFixedSize(std::max(side, 240), side + kTipSpace);
}

void RadialMenu::popup(const QPoint& globalCenter)
{
    QPoint topLeft = globalCenter - center().toPoint();
    // Menu nie może wyjść poza ekran.
    if (QScreen* screen = QGuiApplication::screenAt(globalCenter)) {
        const QRect area = screen->availableGeometry();
        topLeft.setX(std::clamp(topLeft.x(), area.left(), std::max(area.left(), area.right() - width())));
        topLeft.setY(std::clamp(topLeft.y(), area.top(), std::max(area.top(), area.bottom() - height())));
    }
    move(topLeft);
    updateMask();
    show();
    setFocus();
}

QPointF RadialMenu::center() const
{
    return {width() / 2.0, kRing + kMargin};
}

double RadialMenu::step() const
{
    return m_slots.isEmpty() ? 360.0 : 360.0 / m_slots.size();
}

double RadialMenu::slotAngle(int index) const
{
    return -index * step(); // zgodnie z ruchem wskazówek zegara
}

int RadialMenu::slotAt(const QPointF& pos) const
{
    const QPointF d = pos - center();
    const double r = std::hypot(d.x(), d.y());
    if (r < kHole || r > kRing || m_slots.isEmpty())
        return -1;
    const double deg = std::atan2(-d.y(), d.x()) * 180.0 / kPi;
    const int n = int(m_slots.size());
    const int i = int(std::lround(-deg / step()));
    return ((i % n) + n) % n;
}

QString RadialMenu::tipFor(int index) const
{
    if (index < 0)
        return {};
    const QAction* a = m_slots[index];
    if (!a)
        return QString("Puste pole %1").arg(index + 1);
    QString text = a->text().replace('\n', ' ');
    if (!a->isEnabled())
        text += " (niedostępne)";
    return text;
}

QRect RadialMenu::tipRect() const
{
    const QString tip = tipFor(m_hover);
    if (tip.isEmpty())
        return {};
    const QFontMetrics fm(font());
    const int w = fm.horizontalAdvance(tip) + 16;
    return QRect(int(center().x() - w / 2.0), height() - kTipSpace + 10, w, fm.height() + 10);
}

void RadialMenu::updateMask()
{
    // Maska: okno ma kształt pierścienia (działa też bez przezroczystości okien).
    const QPointF c = center();
    auto circle = [&](double r) {
        return QRegion(QRect(int(c.x() - r), int(c.y() - r), int(2 * r) + 1, int(2 * r) + 1), QRegion::Ellipse);
    };
    QRegion region = circle(kRing + 1).subtracted(circle(kHole - 1));
    const QRect tip = tipRect();
    if (!tip.isEmpty())
        region += tip.adjusted(0, 0, 1, 1);
    setMask(region);
}

void RadialMenu::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF c = center();

    for (int i = 0; i < int(m_slots.size()); ++i) {
        QAction* a = m_slots[i];
        const bool disabled = a && !a->isEnabled();
        p.setPen(QPen(kBorder, 1));
        p.setBrush(disabled ? kDisabled : i == m_hover ? kHover : kFill);
        p.drawPath(sector(c, kHole, kRing, slotAngle(i) - step() / 2, step()));
        if (a) {
            const QPointF at = onCircle(c, (kHole + kRing) / 2, slotAngle(i));
            const QRect r(int(at.x() - 13), int(at.y() - 13), 26, 26);
            a->icon().paint(&p, r, Qt::AlignCenter, disabled ? QIcon::Disabled : QIcon::Normal);
        }
    }

    // Podpowiedź z nazwą pola pod pierścieniem (jak "Rotate" w Alphacam).
    const QRect box = tipRect();
    if (!box.isEmpty()) {
        p.setPen(QPen(QColor(120, 120, 120), 1));
        p.setBrush(QColor(250, 250, 250));
        p.drawRect(box);
        p.setPen(Qt::black);
        p.drawText(box, Qt::AlignCenter, tipFor(m_hover));
    }
}

void RadialMenu::mouseMoveEvent(QMouseEvent* e)
{
    const int hover = slotAt(e->position());
    if (hover != m_hover) {
        m_hover = hover;
        updateMask();
        update();
    }
}

void RadialMenu::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::RightButton) {
        close();
        return;
    }
    if (e->button() != Qt::LeftButton)
        return;
    const int i = slotAt(e->position());
    QAction* a = i >= 0 ? m_slots[i] : nullptr;
    if (!a || !a->isEnabled())
        return; // puste pole nic nie robi
    close();
    a->trigger();
}

void RadialMenu::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(e);
}
