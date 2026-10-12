#include "AlignSettingsDialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QStyle>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

using camcore::AlignSettings;

namespace {

// Zapis/odczyt do dowolnego QSettings – rejestr albo plik .ini (eksport/import).
void writeSettings(QSettings& q, const AlignSettings& s)
{
    q.beginGroup("AutoAlign");
    q.setValue("alignAfterImport", s.alignAfterImport);
    q.setValue("lathe", s.lathe);
    q.setValue("panel", s.panel);
    q.setValue("minimalBox", s.minimalBox);
    q.setValue("zZero", static_cast<int>(s.zZero));
    q.setValue("longEdge", static_cast<int>(s.longEdge));
    q.setValue("baseX", static_cast<int>(s.baseX));
    q.setValue("baseY", static_cast<int>(s.baseY));
    q.endGroup();
}

AlignSettings readSettings(QSettings& q)
{
    AlignSettings d; // wartości domyślne, gdy czegoś brakuje
    AlignSettings s;
    q.beginGroup("AutoAlign");
    s.alignAfterImport = q.value("alignAfterImport", d.alignAfterImport).toBool();
    s.lathe = q.value("lathe", d.lathe).toBool();
    s.panel = q.value("panel", d.panel).toBool();
    s.minimalBox = q.value("minimalBox", d.minimalBox).toBool();
    s.zZero = static_cast<AlignSettings::ZZero>(q.value("zZero", int(d.zZero)).toInt());
    s.longEdge = static_cast<AlignSettings::LongEdge>(q.value("longEdge", int(d.longEdge)).toInt());
    s.baseX = static_cast<AlignSettings::BaseX>(q.value("baseX", int(d.baseX)).toInt());
    s.baseY = static_cast<AlignSettings::BaseY>(q.value("baseY", int(d.baseY)).toInt());
    q.endGroup();
    return s;
}

// Ramka z pionową listą przycisków radiowych; id przycisku = wartość enuma.
QGroupBox* radioBox(const QString& title, const QStringList& labels, QButtonGroup*& group,
                    QWidget* parent, bool horizontal = false)
{
    auto* box = new QGroupBox(title, parent);
    QBoxLayout* layout = horizontal ? static_cast<QBoxLayout*>(new QHBoxLayout(box))
                                    : static_cast<QBoxLayout*>(new QVBoxLayout(box));
    group = new QButtonGroup(box);
    for (int i = 0; i < labels.size(); ++i) {
        auto* radio = new QRadioButton(labels[i], box);
        group->addButton(radio, i);
        layout->addWidget(radio);
    }
    return box;
}

} // namespace

AlignSettings loadAlignSettings()
{
    QSettings q("MiniCAM", "MiniCAM");
    return readSettings(q);
}

void saveAlignSettings(const AlignSettings& s)
{
    QSettings q("MiniCAM", "MiniCAM");
    writeSettings(q, s);
}

AlignSettingsDialog::AlignSettingsDialog(const AlignSettings& settings, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Konfiguracja");

    auto* tabs = new QTabWidget(this);
    tabs->setUsesScrollButtons(false); // wszystkie zakładki widoczne naraz
    // Pozostałe zakładki – na razie puste, żeby układ był jak w docelowym oknie.
    for (const char* name : {"Ogólne", "Nazwy Warstw", "Płaszczyzny pracy", "Geometrie"})
        tabs->setTabEnabled(tabs->addTab(new QWidget(tabs), name), false);
    const int alignTab = tabs->addTab(createAutoAlignTab(), "Auto-wyrównanie");
    tabs->setTabEnabled(tabs->addTab(new QWidget(tabs), "Powiadomienie"), false);
    tabs->setCurrentIndex(alignTab);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Cancel)->setText("Anuluj");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);

    setSettings(settings);
}

QWidget* AlignSettingsDialog::createAutoAlignTab()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    // Pasek ikon: eksport/import ustawień do pliku.
    auto* icons = new QHBoxLayout;
    auto* exportBtn = new QToolButton(page);
    exportBtn->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    exportBtn->setIconSize(QSize(24, 24));
    exportBtn->setToolTip("Zapisz ustawienia do pliku");
    connect(exportBtn, &QToolButton::clicked, this, &AlignSettingsDialog::exportToFile);
    auto* importBtn = new QToolButton(page);
    importBtn->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    importBtn->setIconSize(QSize(24, 24));
    importBtn->setToolTip("Wczytaj ustawienia z pliku");
    connect(importBtn, &QToolButton::clicked, this, &AlignSettingsDialog::importFromFile);
    icons->addWidget(exportBtn);
    icons->addWidget(importBtn);
    icons->addStretch();
    layout->addLayout(icons);

    m_afterImport = new QCheckBox("Wyrównaj po imporcie", page);
    m_lathe = new QCheckBox("Użyj  Wyrównanie dla toczenia", page);
    m_lathe->setEnabled(false); // toczenie – jeszcze nieobsługiwane
    m_lathe->setToolTip("Jeszcze nieobsługiwane");
    m_panel = new QCheckBox("Użyj  Wyrównanie panelu", page);
    m_minimalBox = new QCheckBox("Użyj minimalnego prostopadłościanu", page);
    m_minimalBox->setToolTip("Orientacja z najmniejszego obróconego prostopadłościanu,\n"
                             "zamiast z największej płaskiej ściany");

    layout->addWidget(m_afterImport);
    auto* latheRow = new QHBoxLayout;
    latheRow->addSpacing(20); // wcięcie jak w oryginale
    latheRow->addWidget(m_lathe);
    layout->addLayout(latheRow);
    layout->addWidget(m_panel);
    layout->addWidget(m_minimalBox);

    layout->addWidget(radioBox("Ustaw zero osi Z na",
                               {"Górze części", "Środku Części", "Spodzie części"}, m_zZero, page));
    layout->addWidget(radioBox("Wyrównaj z najdłuższą krawędzią w",
                               {"Osi X", "Osi Y"}, m_longEdge, page, true));

    auto* base = new QGroupBox("Punkt Bazowy", page);
    auto* baseLayout = new QHBoxLayout(base);
    baseLayout->addWidget(radioBox("X", {"Lewy", "Środek", "Prawy"}, m_baseX, base));
    baseLayout->addWidget(radioBox("Y", {"Dół", "Środek", "Góra"}, m_baseY, base));
    layout->addWidget(base);

    // Obrót płyty ma sens tylko z "Wyrównanie panelu".
    connect(m_panel, &QCheckBox::toggled, this, [this](bool on) {
        m_minimalBox->setEnabled(on);
        for (QAbstractButton* b : m_longEdge->buttons())
            b->setEnabled(on);
    });
    return page;
}

void AlignSettingsDialog::setSettings(const AlignSettings& s)
{
    m_afterImport->setChecked(s.alignAfterImport);
    m_lathe->setChecked(s.lathe);
    m_panel->setChecked(s.panel);
    m_minimalBox->setChecked(s.minimalBox);
    m_zZero->button(static_cast<int>(s.zZero))->setChecked(true);
    m_longEdge->button(static_cast<int>(s.longEdge))->setChecked(true);
    m_baseX->button(static_cast<int>(s.baseX))->setChecked(true);
    m_baseY->button(static_cast<int>(s.baseY))->setChecked(true);
    // toggled() nie przychodzi, gdy stan się nie zmienia – ustawiamy ręcznie.
    m_minimalBox->setEnabled(s.panel);
    for (QAbstractButton* b : m_longEdge->buttons())
        b->setEnabled(s.panel);
}

AlignSettings AlignSettingsDialog::settings() const
{
    AlignSettings s;
    s.alignAfterImport = m_afterImport->isChecked();
    s.lathe = m_lathe->isChecked();
    s.panel = m_panel->isChecked();
    s.minimalBox = m_minimalBox->isChecked();
    s.zZero = static_cast<AlignSettings::ZZero>(m_zZero->checkedId());
    s.longEdge = static_cast<AlignSettings::LongEdge>(m_longEdge->checkedId());
    s.baseX = static_cast<AlignSettings::BaseX>(m_baseX->checkedId());
    s.baseY = static_cast<AlignSettings::BaseY>(m_baseY->checkedId());
    return s;
}

void AlignSettingsDialog::exportToFile()
{
    const QString path = QFileDialog::getSaveFileName(this, "Zapisz ustawienia", "wyrownanie.ini",
                                                      "Ustawienia (*.ini)");
    if (path.isEmpty())
        return;
    QSettings q(path, QSettings::IniFormat);
    writeSettings(q, settings());
}

void AlignSettingsDialog::importFromFile()
{
    const QString path = QFileDialog::getOpenFileName(this, "Wczytaj ustawienia", QString(),
                                                      "Ustawienia (*.ini)");
    if (path.isEmpty())
        return;
    QSettings q(path, QSettings::IniFormat);
    setSettings(readSettings(q));
}
