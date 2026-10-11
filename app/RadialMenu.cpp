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

// Wymiary [px]: dziura w środku, pierścień pól, zewnętrzny łuk po rozwinięciu strzałek.
constexpr double kHole = 38;
constexpr double kRing = 92;
constexpr double kOuterFrom = 96;
constexpr double kOuter = 146;
constexpr double kOuterStep = 22; // szerokość pola na zewnętrznym łuku [stopnie]
constexpr int kMargin = 4;
constexpr int kTipSpace = 44; // miejsce pod kółkiem na podpowiedź

const QColor kFill(214, 229, 246);
const QColor kFillOuter(200, 219, 241);
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

// Różnica kątów sprowadzona do (-180, 180].
double angleDiff(double a, double b)
{
    double d = std::fmod(a - b, 360.0);
    if (d > 180)
        d -= 360;
    if (d <= -180)
        d += 360;
    return d;
}

// Czarne strzałki », jak w Alphacam. Kierunek: na zewnątrz kółka (kąt w stopniach).
void drawExpandArrows(QPainter& p, const QPointF& at, double deg)
{
    p.save();
    p.translate(at);
    p.rotate(-deg); // Qt obraca zgodnie z zegarem
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(30, 30, 30));
    for (double x : {-9.0, -1.0}) {
        const QPointF chevron[] = {{x, -8}, {x + 4, -8}, {x + 10, 0}, {x + 4, 8}, {x, 8}, {x + 6, 0}};
        p.drawPolygon(chevron, 6);
    }
    p.restore();
}

} // namespace

RadialMenu::RadialMenu(const std::vector<RadialSlot>& items, QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    , m_slots(items)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    const int side = int(2 * (kOuter + kMargin));
    setFixedSize(side, side + kTipSpace);
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
    return {width() / 2.0, kOuter + kMargin};
}

double RadialMenu::step() const
{
    return m_slots.empty() ? 360.0 : 360.0 / double(m_slots.size());
}

double RadialMenu::slotAngle(int index) const
{
    return -index * step(); // zgodnie z ruchem wskazówek zegara
}

double RadialMenu::outerAngle(int index) const
{
    // Łuk wyśrodkowany na polu ze strzałkami, dalej zgodnie z ruchem wskazówek zegara.
    const int count = int(m_slots[m_open].more.size());
    return slotAngle(m_open) - (index - (count - 1) / 2.0) * kOuterStep;
}

RadialMenu::Hit RadialMenu::hitTest(const QPointF& pos) const
{
    const QPointF d = pos - center();
    const double r = std::hypot(d.x(), d.y());
    const double deg = std::atan2(-d.y(), d.x()) * 180.0 / kPi;
    if (r >= kHole && r <= kRing && !m_slots.empty()) {
        const int n = int(m_slots.size());
        const int i = int(std::lround(-deg / step()));
        return {0, ((i % n) + n) % n};
    }
    if (r >= kOuterFrom && r <= kOuter && m_open >= 0) {
        for (int j = 0; j < int(m_slots[m_open].more.size()); ++j)
            if (std::abs(angleDiff(deg, outerAngle(j))) <= kOuterStep / 2)
                return {1, j};
    }
    return {};
}

QAction* RadialMenu::actionAt(const Hit& hit) const
{
    if (hit.ring == 0)
        return m_slots[hit.index].action;
    if (hit.ring == 1)
        return m_slots[m_open].more[hit.index];
    return nullptr;
}

QString RadialMenu::tipFor(const Hit& hit) const
{
    if (hit.ring < 0)
        return {};
    if (const QAction* a = actionAt(hit)) {
        QString text = a->text().replace('\n', ' ');
        if (!a->isEnabled())
            text += " (niedostępne)";
        return text;
    }
    if (hit.ring == 0 && m_slots[hit.index].expands)
        return QString("Więcej (pole %1)").arg(hit.index + 1);
    if (hit.ring == 1)
        return QString("Puste pole %1.%2").arg(m_open + 1).arg(hit.index + 1);
    return QString("Puste pole %1").arg(hit.index + 1);
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
    // Maska: okno ma kształt kółka (działa też bez przezroczystości okien).
    const QPointF c = center();
    auto circle = [&](double r) {
        return QRegion(QRect(int(c.x() - r), int(c.y() - r), int(2 * r) + 1, int(2 * r) + 1), QRegion::Ellipse);
    };
    QRegion region = circle(kRing + 1).subtracted(circle(kHole - 1));
    if (m_open >= 0) {
        for (int j = 0; j < int(m_slots[m_open].more.size()); ++j) {
            const QPainterPath p =
                sector(c, kOuterFrom - 1, kOuter + 1, outerAngle(j) - kOuterStep / 2 - 0.5, kOuterStep + 1);
            region += QRegion(p.toFillPolygon().toPolygon());
        }
    }
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

    auto drawField = [&](const QPainterPath& path, QAction* a, bool hovered, const QPointF& at, const QColor& fill) {
        const bool disabled = a && !a->isEnabled();
        p.setPen(QPen(kBorder, 1));
        p.setBrush(disabled ? kDisabled : hovered ? kHover : fill);
        p.drawPath(path);
        if (a) {
            const QRect r(int(at.x() - 13), int(at.y() - 13), 26, 26);
            a->icon().paint(&p, r, Qt::AlignCenter, disabled ? QIcon::Disabled : QIcon::Normal);
        }
    };

    // Pierścień.
    for (int i = 0; i < int(m_slots.size()); ++i) {
        const RadialSlot& s = m_slots[i];
        const double a = slotAngle(i);
        const QPointF at = onCircle(c, (kHole + kRing) / 2, a);
        const bool hovered = m_hover == Hit{0, i} || m_open == i;
        drawField(sector(c, kHole, kRing, a - step() / 2, step()), s.action, hovered, at, kFill);
        if (s.expands && !s.action)
            drawExpandArrows(p, at, a);
    }

    // Zewnętrzny łuk rozwiniętych strzałek.
    if (m_open >= 0) {
        const RadialSlot& s = m_slots[m_open];
        for (int j = 0; j < int(s.more.size()); ++j) {
            const double a = outerAngle(j);
            drawField(sector(c, kOuterFrom, kOuter, a - kOuterStep / 2, kOuterStep), s.more[j], m_hover == Hit{1, j},
                      onCircle(c, (kOuterFrom + kOuter) / 2, a), kFillOuter);
        }
    }

    // Podpowiedź z nazwą pola pod kółkiem (jak "Rotate" w Alphacam).
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
    const Hit hit = hitTest(e->position());
    int open = m_open;
    if (hit.ring == 0)
        open = m_slots[hit.index].expands ? hit.index : -1; // najechanie na » rozszerza kółko
    if (!(hit == m_hover) || open != m_open) {
        m_hover = hit;
        m_open = open;
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
    QAction* a = actionAt(hitTest(e->position()));
    if (!a || !a->isEnabled())
        return; // puste pole i strzałki nic nie uruchamiają
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
