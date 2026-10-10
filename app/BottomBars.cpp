#include "BottomBars.h"

#include "InputBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QShortcut>
#include <QStatusBar>
#include <QToolButton>

#include <algorithm>

namespace {

// Ikony uchwytów (jak w Alphacam): szary element i pomarańczowy punkt przyciągania.
QIcon snapIcon(int kind)
{
    QPixmap pix(18, 18);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    const QPen line(QColor(60, 60, 60), 1.6);
    const QColor dot(240, 150, 20);
    auto point = [&](double x, double y) {
        p.setPen(QPen(QColor(120, 60, 0), 0.8));
        p.setBrush(dot);
        p.drawRect(QRectF(x - 2.2, y - 2.2, 4.4, 4.4));
    };
    p.setPen(line);
    p.setBrush(Qt::NoBrush);
    switch (kind) {
    case 0: // auto: element z kilkoma punktami
        p.drawLine(QPointF(3, 15), QPointF(15, 3));
        point(3, 15);
        point(9, 9);
        point(15, 3);
        break;
    case 1: // koniec
        p.drawLine(QPointF(4, 9), QPointF(16, 9));
        point(4, 9);
        break;
    case 2: // środek
        p.drawLine(QPointF(2, 9), QPointF(16, 9));
        point(9, 9);
        break;
    case 3: // centrum okręgu
        p.drawEllipse(QPointF(9, 9), 6.5, 6.5);
        point(9, 9);
        break;
    case 8: // ćwiartki
        p.drawEllipse(QPointF(9, 9), 6, 6);
        point(9, 3);
        point(15, 9);
        point(9, 15);
        point(3, 9);
        break;
    default:
        break;
    }
    return QIcon(pix);
}

// Kursor przy uchwycie: strzałka, a obok niej (w ramce) ikonka wybranego uchwytu.
QCursor snapCursor(int kind)
{
    QPixmap pix(40, 40);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF arrow[] = {{1, 1}, {1, 17}, {5, 13}, {8, 20}, {11, 19}, {8, 12}, {13, 12}};
    p.setPen(QPen(Qt::black, 1));
    p.setBrush(Qt::white);
    p.drawPolygon(arrow, 7);
    const QRectF box(16, 16, 22, 22);
    p.setPen(QPen(QColor(40, 40, 40), 1));
    p.setBrush(QColor(255, 255, 255, 230));
    p.drawRect(box);
    snapIcon(kind).paint(&p, box.adjusted(2, 2, -2, -2).toRect());
    return QCursor(pix, 1, 1);
}

// Pusty przycisk – miejsce na przyszłą funkcję (na razie bez ikony i bez działania).
QToolButton* placeholderButton(QWidget* parent, const QString& tip, int width = 26)
{
    auto* b = new QToolButton(parent);
    b->setFixedSize(width, 24);
    b->setToolTip(tip + " (wkrótce)");
    b->setEnabled(false);
    b->setProperty("placeholder", true);
    return b;
}
} // namespace

CommandBar::CommandBar(QWidget* shortcutWindow, QWidget* parent)
    : QToolBar("Polecenie", parent)
{
    // Belka polecenia (druga od dołu, jak w Alphacam): po lewej pasek wprowadzania
    // (podpowiedź i pola bieżącego polecenia), po prawej przyciąganie.
    setObjectName("commandBar");
    setMovable(false);
    setFloatable(false);
    setContextMenuPolicy(Qt::PreventContextMenu);
    setMinimumHeight(32); // pusta belka (bez polecenia) zostaje na swoim miejscu
    m_inputBar = new InputBar(this);
    addWidget(m_inputBar);
    auto* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(spacer);
    // Uchwyty (przyciąganie, Snaps) – jak w Alphacam widać je tylko wtedy, gdy polecenie
    // czeka na punkt. Wybrany uchwyt: znika krzyż, a kursor klei się do takich punktów
    // geometrii. Drugie kliknięcie tego samego uchwytu go wyłącza.
    std::vector<QAction*> snaps;
    snaps.push_back(this->addSeparator());
    snaps.push_back(this->addWidget(new QLabel(" Uchwyty ", this)));
    struct SnapDef
    {
        const char* name;
        OccView::Snap snap; // None = jeszcze bez działania
        int key;            // skrót klawiszowy (0 = brak)
    };
    const SnapDef snapDefs[] = {
        {"AUTO uchwyt (końce, środki, ćwiartki)", OccView::Snap::Auto, 0},
        {"KONIEC elementu", OccView::Snap::End, Qt::Key_F6},
        {"ŚRODEK elementu", OccView::Snap::Mid, Qt::Key_F7},
        {"CENTRUM okręgu", OccView::Snap::Centre, Qt::Key_F8},
        {"PRZECIĘCIE elementów", OccView::Snap::None, 0},
        {"STYCZNA do łuku lub okręgu", OccView::Snap::None, 0},
        {"PROSTOPADŁA do elementu", OccView::Snap::None, 0},
        {"RÓWNOLEGŁA do elementu", OccView::Snap::None, 0},
        {"ĆWIARTKI koła", OccView::Snap::Quadrant, 0},
    };
    for (int i = 0; i < 9; ++i) {
        const SnapDef& d = snapDefs[i];
        const QString name = QString::fromUtf8(d.name);
        if (d.snap == OccView::Snap::None) {
            snaps.push_back(this->addWidget(placeholderButton(this, name)));
            continue;
        }
        auto* b = new QToolButton(this);
        b->setObjectName("snapButton");
        b->setIcon(snapIcon(i));
        b->setIconSize(QSize(18, 18));
        b->setFixedSize(26, 24);
        b->setCheckable(true);
        b->setFocusPolicy(Qt::NoFocus);
        b->setToolTip(d.key ? name + "  " + QKeySequence(d.key).toString() : name);
        const OccView::Snap snap = d.snap;
        const QCursor cursor = snapCursor(i);
        connect(b, &QToolButton::toggled, this, [this, b, snap, cursor](bool on) {
            if (on) {
                for (QToolButton* other : m_snapButtons)
                    if (other != b)
                        other->setChecked(false); // naraz działa jeden uchwyt
                emit snapChanged(snap, cursor);
            } else if (std::none_of(m_snapButtons.begin(), m_snapButtons.end(),
                                    [](QToolButton* x) { return x->isChecked(); })) {
                emit snapChanged(OccView::Snap::None, QCursor());
            }
        });
        if (d.key) {
            // Skrót działa w całym oknie (także gdy kursor stoi w polu X/Y), ale tylko
            // wtedy, gdy przycisk jest widoczny – czyli polecenie czeka na punkt.
            auto* shortcut = new QShortcut(QKeySequence(d.key), shortcutWindow);
            connect(shortcut, &QShortcut::activated, b, [b] {
                if (b->isVisible())
                    b->toggle();
            });
        }
        m_snapButtons.push_back(b);
        snaps.push_back(this->addWidget(b));
    }
    snaps.push_back(this->addWidget(new QLabel(" Filtry ", this)));
    snaps.push_back(this->addWidget(placeholderButton(this, "Filtr 1")));
    snaps.push_back(this->addWidget(placeholderButton(this, "Filtr 2")));
    connect(m_inputBar, &InputBar::activeChanged, this, [this, snaps](bool pointInput) {
        for (QAction* a : snaps)
            a->setVisible(pointInput);
        if (!pointInput)
            for (QToolButton* b : m_snapButtons)
                b->setChecked(false); // koniec wskazywania – uchwyt się wyłącza
    });
    for (QAction* a : snaps)
        a->setVisible(false);
}

void CommandBar::releaseSnap()
{
    for (QToolButton* b : m_snapButtons)
        b->setChecked(false);
}

void setupFooter(QStatusBar* statusBar)
{
    // Stopka (najniżej): po komunikatach i współrzędnych kursora 15 przycisków widoków
    // i 4 przełączniki – na razie bez działania.
    auto* views = new QWidget(statusBar);
    views->setObjectName("barGroup");
    auto* viewsLayout = new QHBoxLayout(views);
    viewsLayout->setContentsMargins(8, 0, 0, 0);
    viewsLayout->setSpacing(1);
    for (int i = 1; i <= 15; ++i)
        viewsLayout->addWidget(placeholderButton(views, QString("Widok %1").arg(i), 22));
    statusBar->addPermanentWidget(views);

    auto* toggles = new QWidget(statusBar);
    toggles->setObjectName("barGroup");
    auto* togglesLayout = new QHBoxLayout(toggles);
    togglesLayout->setContentsMargins(8, 0, 0, 0);
    togglesLayout->setSpacing(1);
    for (int i = 1; i <= 4; ++i)
        togglesLayout->addWidget(placeholderButton(toggles, QString("Przełącznik %1").arg(i), 50));
    statusBar->addPermanentWidget(toggles);
}

QString bottomBarsStyleSheet()
{
    // Belki odcinamy od siebie liniami i lekkim cieniem, a pola i przyciski mają
    // wklęsłe ramki – żeby nic się nie zlewało z tłem. Kolory tekstu są stałe:
    // przy ciemnym motywie Windows Qt dałby jasny tekst na naszym jasnym tle.
    return R"(
        QToolBar#commandBar QLabel, QStatusBar, QStatusBar QLabel { color: black; }
        QToolBar#commandBar QPushButton {
            color: black;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e6e6e6);
            border: 1px solid #a8a8a8; border-radius: 2px; padding: 2px 14px;
        }
        QToolBar#commandBar QPushButton:pressed { background: #cfe3f7; }
        QToolBar#commandBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f7f7f7, stop:1 #e4e4e4);
            border-top: 1px solid #8c8c8c; border-bottom: 1px solid #8c8c8c;
            padding: 2px 4px; spacing: 3px;
        }
        QStatusBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ececec, stop:1 #dcdcdc);
            border-top: 1px solid #ffffff;
        }
        QStatusBar::item { border: none; }
        QToolButton[placeholder="true"] {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e6e6e6);
            border: 1px solid #a8a8a8; border-radius: 2px;
        }
        QToolButton#snapButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e6e6e6);
            border: 1px solid #a8a8a8; border-radius: 2px;
        }
        QToolButton#snapButton:hover { border-color: #3d7fd1; }
        QToolButton#snapButton:checked { background: #9a9a9a; border: 1px solid #505050; }
        QLabel#cursorLabel {
            background: #fafafa; border: 1px solid #a8a8a8; border-radius: 2px; padding: 1px 6px;
        }
        QWidget#barGroup { border-left: 1px solid #b0b0b0; }
        QLabel#inputCommand { font-weight: bold; }
        QWidget#inputBar QLineEdit {
            color: black; background: #ffffff; border: 1px solid #a8a8a8; border-radius: 2px; padding: 1px 3px;
            selection-background-color: #3d7fd1; selection-color: white;
        }
        QWidget#inputBar QLineEdit[error="true"] { background: #ffd6d6; border-color: #c03030; }
        QToolBar#commandBar QPushButton#inputHint {
            color: #202020; background: #b4b4b4; border: 1px solid #7a7a7a; border-radius: 1px;
            padding: 3px 12px;
        }
        QToolButton#inputF1 {
            color: black;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e6e6e6);
            border: 1px solid #a8a8a8; border-radius: 2px; padding: 1px 5px;
        }
    )";
}
