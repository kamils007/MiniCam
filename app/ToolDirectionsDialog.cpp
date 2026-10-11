#include "ToolDirectionsDialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QVBoxLayout>

namespace {

const char* const kSettingsGroup = "kierunkiNarzedzia";

// Ramka z przyciskami opcji; numer przycisku w grupie = wartość wyliczenia.
QGroupBox* radioBox(const QString& title, const QStringList& labels, QButtonGroup*& group, int checked,
                    QWidget* parent)
{
    auto* box = new QGroupBox(title, parent);
    auto* layout = new QVBoxLayout(box);
    layout->setSpacing(2);
    layout->setContentsMargins(8, 4, 8, 4);
    group = new QButtonGroup(box);
    for (int i = 0; i < labels.size(); ++i) {
        auto* radio = new QRadioButton(labels[i], box);
        group->addButton(radio, i);
        layout->addWidget(radio);
    }
    layout->addStretch();
    if (QAbstractButton* b = group->button(checked))
        b->setChecked(true);
    return box;
}

int stored(const char* key, int fallback)
{
    QSettings s("MiniCAM", "MiniCAM");
    s.beginGroup(kSettingsGroup);
    return s.value(key, fallback).toInt();
}

} // namespace

ToolDirectionsDialog::ToolDirectionsDialog(QWidget* parent)
    : QDialog(parent)
{
    using C = camcore::DirectionChange;
    const C defaults;
    setWindowTitle("Kierunki narzędzia");
    auto* layout = new QVBoxLayout(this);

    auto* ghost = new QCheckBox("Pokaż narzędzia-duchy", this);
    ghost->setEnabled(false);
    ghost->setToolTip("Wkrótce – gdy geometrie dostaną narzędzia");
    auto* top = new QHBoxLayout;
    top->addStretch();
    top->addWidget(ghost);
    layout->addLayout(top);

    auto* open = new QGroupBox("Geometrie otwarte", this);
    auto* openLayout = new QHBoxLayout(open);
    openLayout->addWidget(radioBox("Kierunek", {"Odwróć", "Bez zmian"}, m_openDirection,
                                   stored("otwarteKierunek", int(defaults.openDirection)), open));
    openLayout->addWidget(radioBox("Strona", {"Lewa", "Prawa", "Zamień L/P", "Środkiem", "Bez zmian"}, m_openSide,
                                   stored("otwarteStrona", int(defaults.openSide)), open));
    layout->addWidget(open);

    auto* closed = new QGroupBox("Geometrie zamknięte", this);
    auto* closedLayout = new QGridLayout(closed);
    closedLayout->addWidget(
        radioBox("Kierunek",
                 {"CW (zgodnie ze wskazówkami)", "CCW (przeciwnie)", "Odwróć", "Bez zmian",
                  "Narzędzie po lewej (współbieżnie)", "Narzędzie po prawej (przeciwbieżnie)"},
                 m_closedDirection, stored("zamknieteKierunek", int(defaults.closedDirection)), closed),
        0, 0);
    closedLayout->addWidget(radioBox("Strona",
                                     {"Na zewnątrz", "Do środka", "Lewa", "Prawa", "Zamień zewnątrz/środek",
                                      "Środkiem", "Bez zmian", "Automatycznie"},
                                     m_closedSide, stored("zamknieteStrona", int(defaults.closedSide)), closed),
                            0, 1);
    m_closedSide->button(int(C::ClosedSide::Automatic))
        ->setToolTip("Obrys bryły na zewnątrz, otwory wiertłem środkiem, reszta po stronie bez materiału\n"
                     "(bez bryły: zagnieżdżenie – w parzystej liczbie geometrii na zewnątrz, w nieparzystej do środka)");

    // Punkt startowy na geometriach zamkniętych – na razie tylko układ (jak w Alphacam).
    auto* start = new QGroupBox("Punkt startowy na geometriach zamkniętych (wkrótce)", closed);
    start->setEnabled(false);
    auto* startLayout = new QGridLayout(start);
    QButtonGroup* startMode = nullptr;
    auto* modes = radioBox(QString(), {"nie", "ręcznie", "automatycznie"}, startMode, 0, start);
    modes->setFlat(true);
    static_cast<QVBoxLayout*>(modes->layout())->setDirection(QBoxLayout::LeftToRight);
    startLayout->addWidget(modes, 0, 0, 1, 4);
    const QStringList rows = {"do środka", "na zewnątrz", "środkiem"};
    for (int i = 0; i < rows.size(); ++i) {
        startLayout->addWidget(new QLabel(rows[i], start), i + 1, 0, Qt::AlignRight);
        startLayout->addWidget(new QComboBox(start), i + 1, 1);
        startLayout->addWidget(new QLabel("preferuj", start), i + 1, 2, Qt::AlignRight);
        startLayout->addWidget(new QComboBox(start), i + 1, 3);
    }
    closedLayout->addWidget(start, 1, 0, 1, 2);
    layout->addWidget(closed);

    auto* all = new QPushButton("Wszystkie geometrie", this);
    all->setToolTip("Zmień kierunek i stronę wszystkich geometrii");
    auto* select = new QPushButton("Wybierz geometrie…", this);
    select->setToolTip("Wskaż geometrie w widoku (LPM), PPM zmienia");
    select->setDefault(true);
    auto* close = new QPushButton("Zamknij", this);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(all);
    buttons->addWidget(select);
    buttons->addWidget(close);
    buttons->addStretch();
    layout->addLayout(buttons);

    connect(all, &QPushButton::clicked, this, [this] {
        save();
        done(AllGeometries);
    });
    connect(select, &QPushButton::clicked, this, [this] {
        save();
        done(SelectGeometries);
    });
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
}

camcore::DirectionChange ToolDirectionsDialog::change() const
{
    using C = camcore::DirectionChange;
    C c;
    c.openDirection = C::OpenDirection(m_openDirection->checkedId());
    c.openSide = C::OpenSide(m_openSide->checkedId());
    c.closedDirection = C::ClosedDirection(m_closedDirection->checkedId());
    c.closedSide = C::ClosedSide(m_closedSide->checkedId());
    return c;
}

void ToolDirectionsDialog::save() const
{
    QSettings s("MiniCAM", "MiniCAM");
    s.beginGroup(kSettingsGroup);
    s.setValue("otwarteKierunek", m_openDirection->checkedId());
    s.setValue("otwarteStrona", m_openSide->checkedId());
    s.setValue("zamknieteKierunek", m_closedDirection->checkedId());
    s.setValue("zamknieteStrona", m_closedSide->checkedId());
}
