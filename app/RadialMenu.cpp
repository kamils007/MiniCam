#include "RadialMenu.h"

#include <QAction>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QScreen>
#include <QSettings>

#include <algorithm>
#include <cmath>

namespace {

// Wymiary pierścieni [px]: dziura w środku, pierścień poleceń, łuk podmenu.
constexpr double kHole = 38;
constexpr double kInner = 92;
constexpr double kOuterFrom = 96;
constexpr double kOuter = 146;
constexpr double kChildStep = 22; // szerokość pozycji na łuku podmenu [stopnie]
constexpr int kMargin = 4;
constexpr int kTipSpace = 44; // miejsce pod pierścieniem na podpowiedź

const QColor kFill(214, 229, 246);
const QColor kFillOuter(200, 219, 241);
const QColor kHover(255, 231, 150);
const QColor kDisabled(205, 205, 205);
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

// Ikona podmenu: podwójna strzałka », jak w Alphacam.
QIcon submenuIcon()
{
    QPixmap pix(32, 32);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(30, 30, 30));
    for (double x : {5.0, 15.0}) {
        const QPointF chevron[] = {{x, 6}, {x + 6, 6}, {x + 14, 16}, {x + 6, 26}, {x, 26}, {x + 8, 16}};
        p.drawPolygon(chevron, 6);
    }
    return QIcon(pix);
}

QString plainText(const QAction* a)
{
    return a->text().replace('\n', ' ');
}

QString encodeEntry(const RadialEntry& e)
{
    if (!e.isSubmenu())
        return e.command;
    return "submenu:" + e.title + "|" + e.children.join(',');
}

RadialEntry decodeEntry(const QString& s)
{
    RadialEntry e;
    if (!s.startsWith("submenu:")) {
        e.command = s;
        return e;
    }
    const QString body = s.mid(8);
    const int bar = body.lastIndexOf('|');
    e.title = body.left(bar);
    e.children = body.mid(bar + 1).split(',', Qt::SkipEmptyParts);
    return e;
}

} // namespace

// ---- Układ menu ----

RadialMenuConfig RadialMenuConfig::defaults()
{
    // Kolejność jak na obrazku z pomocy Alphacam: od godz. 3 zgodnie z ruchem wskazówek zegara.
    RadialMenuConfig c;
    auto cmd = [&](const char* id) { c.entries.push_back({id, {}, {}}); };
    auto sub = [&](const char* title, QStringList children) { c.entries.push_back({{}, title, children}); };
    sub("Symulacja", {"simPaths", "simSolid"});
    cmd("fit");
    cmd("zoomWindow");
    cmd("reverseView");
    cmd("refresh");
    cmd("views3d");
    cmd("shading");
    sub("Przesuń, kopiuj itd.", {"move", "rotate", "mirror", "scale"});
    sub("Przerwij, połącz itd.", {"break", "trim", "join"});
    cmd("normalToView");
    cmd("delete");
    cmd("undo");
    cmd("redo");
    cmd("layersWindow");
    return c;
}

void RadialMenuConfig::write(QSettings& q) const
{
    QStringList list;
    for (const RadialEntry& e : entries)
        list << encodeEntry(e);
    q.beginGroup("RadialMenu");
    q.setValue("entries", list);
    q.setValue("showTooltips", showTooltips);
    q.setValue("legacyMenu", legacyMenu);
    q.endGroup();
}

bool RadialMenuConfig::read(QSettings& q)
{
    q.beginGroup("RadialMenu");
    const bool has = q.contains("entries");
    if (has) {
        entries.clear();
        for (const QString& s : q.value("entries").toStringList())
            entries.push_back(decodeEntry(s));
        showTooltips = q.value("showTooltips", true).toBool();
        legacyMenu = q.value("legacyMenu", false).toBool();
    }
    q.endGroup();
    return has;
}

RadialMenuConfig RadialMenuConfig::load()
{
    RadialMenuConfig c = defaults();
    QSettings q("MiniCAM", "MiniCAM");
    c.read(q);
    return c;
}

void RadialMenuConfig::save() const
{
    QSettings q("MiniCAM", "MiniCAM");
    write(q);
}

bool RadialMenuConfig::exportTo(const QString& path) const
{
    QSettings q(path, QSettings::IniFormat);
    write(q);
    q.sync();
    return q.status() == QSettings::NoError;
}

bool RadialMenuConfig::importFrom(const QString& path)
{
    QSettings q(path, QSettings::IniFormat);
    return read(q);
}

QAction* findCommand(const QList<QAction*>& commands, const QString& id)
{
    for (QAction* a : commands)
        if (a->objectName() == id)
            return a;
    return nullptr;
}

QMenu* createLegacyMenu(const RadialMenuConfig& config, const QList<QAction*>& commands, QWidget* parent)
{
    auto* menu = new QMenu(parent);
    menu->setToolTipsVisible(config.showTooltips);
    for (const RadialEntry& e : config.entries) {
        if (!e.isSubmenu()) {
            if (QAction* a = findCommand(commands, e.command))
                menu->addAction(a);
            continue;
        }
        QMenu* sub = menu->addMenu(e.title);
        for (const QString& id : e.children)
            if (QAction* a = findCommand(commands, id))
                sub->addAction(a);
    }
    return menu;
}

// ---- Widżet menu ----

RadialMenu::RadialMenu(const RadialMenuConfig& config, const QList<QAction*>& commands, QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    , m_showTooltips(config.showTooltips)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    // Polecenia, których program nie zna (np. z importowanego pliku), pomijamy.
    for (const RadialEntry& e : config.entries) {
        Slot slot;
        if (e.isSubmenu()) {
            slot.title = e.title;
            for (const QString& id : e.children)
                if (QAction* a = findCommand(commands, id))
                    slot.children.push_back(a);
        } else {
            slot.action = findCommand(commands, e.command);
            if (!slot.action)
                continue;
        }
        m_slots.push_back(slot);
    }
    const int side = int(2 * (kOuter + kMargin));
    setFixedSize(side, side + kTipSpace);
}

void RadialMenu::popup(const QPoint& globalCenter)
{
    QPoint topLeft = globalCenter - QPoint(width() / 2, int(kOuter + kMargin));
    // Menu nie może wyjść poza ekran.
    if (QScreen* screen = QGuiApplication::screenAt(globalCenter)) {
        const QRect area = screen->availableGeometry();
        topLeft.setX(std::clamp(topLeft.x(), area.left(), std::max(area.left(), area.right() - width())));
        topLeft.setY(std::clamp(topLeft.y(), area.top(), std::max(area.top(), area.bottom() - height())));
    }
    move(topLeft);
    m_mouse = mapFromGlobal(globalCenter);
    updateMask();
    show();
    setFocus();
}

QPointF RadialMenu::center() const
{
    return {width() / 2.0, kOuter + kMargin};
}

double RadialMenu::innerStep() const
{
    return m_slots.empty() ? 360.0 : 360.0 / double(m_slots.size());
}

double RadialMenu::slotAngle(int index) const
{
    return -index * innerStep(); // zgodnie z ruchem wskazówek zegara
}

double RadialMenu::childAngle(int index) const
{
    const int count = int(m_slots[m_open].children.size());
    return slotAngle(m_open) - (index - (count - 1) / 2.0) * kChildStep;
}

RadialMenu::Hit RadialMenu::hitTest(const QPointF& pos) const
{
    const QPointF d = pos - center();
    const double r = std::hypot(d.x(), d.y());
    const double deg = std::atan2(-d.y(), d.x()) * 180.0 / kPi;
    if (r >= kHole && r <= kInner && !m_slots.empty()) {
        const int n = int(m_slots.size());
        const int i = int(std::lround(-deg / innerStep()));
        return {0, ((i % n) + n) % n};
    }
    if (r >= kOuterFrom && r <= kOuter && m_open >= 0) {
        const auto& children = m_slots[m_open].children;
        for (int j = 0; j < int(children.size()); ++j)
            if (std::abs(angleDiff(deg, childAngle(j))) <= kChildStep / 2)
                return {1, j};
    }
    return {};
}

QAction* RadialMenu::actionAt(const Hit& hit) const
{
    if (hit.ring == 0)
        return m_slots[hit.index].action;
    if (hit.ring == 1)
        return m_slots[m_open].children[hit.index];
    return nullptr;
}

QString RadialMenu::tipFor(const Hit& hit) const
{
    if (hit.ring < 0)
        return {};
    const QAction* a = actionAt(hit);
    if (!a)
        return m_slots[hit.index].title;
    QString text = plainText(a);
    if (!a->isEnabled())
        text += a->property("soon").toBool() ? " (wkrótce)" : " (niedostępne)";
    return text;
}

QRect RadialMenu::tipRect() const
{
    const QString tip = m_showTooltips ? tipFor(m_hover) : QString();
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
    QRegion region = circle(kInner + 1).subtracted(circle(kHole - 1));
    if (m_open >= 0) {
        for (int j = 0; j < int(m_slots[m_open].children.size()); ++j) {
            const QPainterPath p = sector(c, kOuterFrom - 1, kOuter + 1, childAngle(j) - kChildStep / 2 - 0.5,
                                          kChildStep + 1);
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
    const double step = innerStep();

    auto drawSlot = [&](const QPainterPath& path, QAction* action, bool hovered, bool disabled, const QIcon& icon,
                        const QPointF& at, int iconSize, const QColor& fill) {
        p.setPen(QPen(kBorder, 1));
        p.setBrush(disabled ? kDisabled : hovered ? kHover : fill);
        p.drawPath(path);
        const QRect r(int(at.x() - iconSize / 2.0), int(at.y() - iconSize / 2.0), iconSize, iconSize);
        icon.paint(&p, r, Qt::AlignCenter, disabled ? QIcon::Disabled : QIcon::Normal);
        if (!action && icon.isNull())
            p.drawText(r, Qt::AlignCenter, "?");
    };

    // Wewnętrzny pierścień.
    for (int i = 0; i < int(m_slots.size()); ++i) {
        const Slot& s = m_slots[i];
        const double a = slotAngle(i);
        const bool hovered = (m_hover.ring == 0 && m_hover.index == i) || m_open == i;
        const bool disabled = s.action ? !s.action->isEnabled() : s.children.empty();
        const QIcon icon = s.action ? s.action->icon() : submenuIcon();
        drawSlot(sector(c, kHole, kInner, a - step / 2, step), s.action, hovered, disabled, icon,
                 onCircle(c, (kHole + kInner) / 2, a), 26, kFill);
    }

    // Łuk otwartego podmenu.
    if (m_open >= 0) {
        const auto& children = m_slots[m_open].children;
        for (int j = 0; j < int(children.size()); ++j) {
            QAction* a = children[j];
            const double deg = childAngle(j);
            const bool hovered = m_hover.ring == 1 && m_hover.index == j;
            drawSlot(sector(c, kOuterFrom, kOuter, deg - kChildStep / 2, kChildStep), a, hovered, !a->isEnabled(),
                     a->icon(), onCircle(c, (kOuterFrom + kOuter) / 2, deg), 24, kFillOuter);
        }
    }

    // Podpowiedź z nazwą polecenia pod pierścieniem (jak "Rotate" w Alphacam).
    const QRect box = tipRect();
    if (!box.isEmpty()) {
        const QString tip = tipFor(m_hover);
        p.setPen(QPen(QColor(120, 120, 120), 1));
        p.setBrush(QColor(250, 250, 250));
        p.drawRect(box);
        p.setPen(Qt::black);
        p.drawText(box, Qt::AlignCenter, tip);
    }
}

void RadialMenu::mouseMoveEvent(QMouseEvent* e)
{
    m_mouse = e->position();
    const Hit hit = hitTest(m_mouse);
    int open = m_open;
    if (hit.ring == 0)
        open = m_slots[hit.index].action ? -1 : hit.index; // najechanie na podmenu je otwiera
    if (hit.ring != m_hover.ring || hit.index != m_hover.index || open != m_open) {
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
    const Hit hit = hitTest(e->position());
    QAction* a = actionAt(hit);
    if (hit.ring == 0 && !a) {
        // Kliknięcie w podmenu tylko je otwiera.
        m_open = hit.index;
        updateMask();
        update();
        return;
    }
    if (!a || !a->isEnabled())
        return;
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
