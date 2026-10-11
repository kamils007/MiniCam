#include "RadialMenuDialog.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

constexpr int kIdRole = Qt::UserRole; // objectName polecenia; puste = podmenu

QString plainText(const QAction* a)
{
    return a->text().replace('\n', ' ');
}

bool isSubmenu(const QTreeWidgetItem* item)
{
    return item && item->data(0, kIdRole).toString().isEmpty();
}

} // namespace

RadialMenuDialog::RadialMenuDialog(const RadialMenuConfig& config, const QList<QAction*>& commands, QWidget* parent)
    : QDialog(parent)
    , m_commands(commands)
{
    setWindowTitle("Dostosuj menu radialne");

    // Lewa kolumna: wszystkie polecenia programu (alfabetycznie).
    auto* source = new QComboBox(this);
    source->addItem("Wszystkie polecenia");
    m_available = new QListWidget(this);
    for (QAction* a : m_commands) {
        auto* item = new QListWidgetItem(a->icon(), plainText(a) + (a->property("soon").toBool() ? " (wkrótce)" : ""));
        item->setData(kIdRole, a->objectName());
        m_available->addItem(item);
    }
    m_available->sortItems();
    m_tooltips = new QCheckBox("Pokaż podpowiedzi", this);
    m_legacy = new QCheckBox("Użyj starego menu kontekstowego", this);

    // Środek: Dodaj / Usuń.
    m_add = new QPushButton("Dodaj >>", this);
    m_remove = new QPushButton("<< Usuń", this);

    // Prawa kolumna: menu radialne jako drzewo (podmenu mają swoje polecenia pod sobą).
    m_menu = new QTreeWidget(this);
    m_menu->setHeaderHidden(true);
    m_menu->setRootIsDecorated(true);
    auto* up = new QToolButton(this);
    up->setArrowType(Qt::UpArrow);
    up->setToolTip("Przesuń wyżej (wcześniej w menu)");
    auto* down = new QToolButton(this);
    down->setArrowType(Qt::DownArrow);
    down->setToolTip("Przesuń niżej (dalej w menu)");
    auto* newSub = new QPushButton("Nowe podmenu", this);
    m_editSubmenu = new QPushButton("Edytuj podmenu", this);
    auto* reset = new QPushButton("Resetuj", this);
    auto* import = new QPushButton("Importuj", this);
    auto* exportBtn = new QPushButton("Eksportuj", this);

    auto* grid = new QGridLayout;
    grid->addWidget(new QLabel("Wybierz polecenia z:", this), 0, 0);
    grid->addWidget(source, 1, 0);
    grid->addWidget(m_available, 2, 0);
    auto* middle = new QVBoxLayout;
    middle->addStretch();
    middle->addWidget(m_add);
    middle->addWidget(m_remove);
    middle->addStretch();
    grid->addLayout(middle, 2, 1);
    grid->addWidget(new QLabel("Menu radialne (od godz. 3, zgodnie z ruchem wskazówek zegara):", this), 0, 2, 2, 1,
                    Qt::AlignBottom);
    grid->addWidget(m_menu, 2, 2);
    auto* arrows = new QVBoxLayout;
    arrows->addStretch();
    arrows->addWidget(up);
    arrows->addWidget(down);
    arrows->addStretch();
    grid->addLayout(arrows, 2, 3);
    auto* checks = new QVBoxLayout;
    checks->addWidget(m_tooltips);
    checks->addWidget(m_legacy);
    grid->addLayout(checks, 3, 0, 2, 1, Qt::AlignTop);
    auto* subRow = new QHBoxLayout;
    subRow->addWidget(newSub);
    subRow->addWidget(m_editSubmenu);
    grid->addLayout(subRow, 3, 2);
    auto* fileRow = new QHBoxLayout;
    fileRow->addWidget(reset);
    fileRow->addWidget(import);
    fileRow->addWidget(exportBtn);
    grid->addLayout(fileRow, 4, 2);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(2, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Cancel)->setText("Anuluj");
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(grid);
    layout->addWidget(buttons);
    resize(760, 460);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_add, &QPushButton::clicked, this, &RadialMenuDialog::addCommand);
    connect(m_available, &QListWidget::itemDoubleClicked, this, &RadialMenuDialog::addCommand);
    connect(m_remove, &QPushButton::clicked, this, &RadialMenuDialog::removeItem);
    connect(up, &QToolButton::clicked, this, [this] { moveItem(-1); });
    connect(down, &QToolButton::clicked, this, [this] { moveItem(1); });
    connect(newSub, &QPushButton::clicked, this, &RadialMenuDialog::newSubmenu);
    connect(m_editSubmenu, &QPushButton::clicked, this, &RadialMenuDialog::editSubmenu);
    connect(reset, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, windowTitle(), "Przywrócić domyślny układ menu radialnego?")
            == QMessageBox::Yes)
            showConfig(RadialMenuConfig::defaults());
    });
    connect(import, &QPushButton::clicked, this, &RadialMenuDialog::importConfig);
    connect(exportBtn, &QPushButton::clicked, this, &RadialMenuDialog::exportConfig);
    connect(m_available, &QListWidget::currentRowChanged, this, &RadialMenuDialog::updateButtons);
    connect(m_menu, &QTreeWidget::currentItemChanged, this, &RadialMenuDialog::updateButtons);

    showConfig(config);
}

QTreeWidgetItem* RadialMenuDialog::commandItem(const QString& id) const
{
    QAction* a = findCommand(m_commands, id);
    if (!a)
        return nullptr; // nieznane polecenie (np. z pliku z nowszej wersji) – pomijamy
    auto* item = new QTreeWidgetItem({plainText(a)});
    item->setIcon(0, a->icon());
    item->setData(0, kIdRole, id);
    item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);
    return item;
}

QTreeWidgetItem* RadialMenuDialog::submenuItem(const QString& title) const
{
    auto* item = new QTreeWidgetItem({title});
    item->setData(0, kIdRole, QString());
    QFont f = item->font(0);
    f.setBold(true);
    item->setFont(0, f);
    return item;
}

void RadialMenuDialog::showConfig(const RadialMenuConfig& config)
{
    m_menu->clear();
    for (const RadialEntry& e : config.entries) {
        if (!e.isSubmenu()) {
            if (QTreeWidgetItem* item = commandItem(e.command))
                m_menu->addTopLevelItem(item);
            continue;
        }
        QTreeWidgetItem* sub = submenuItem(e.title);
        for (const QString& id : e.children)
            if (QTreeWidgetItem* child = commandItem(id))
                sub->addChild(child);
        m_menu->addTopLevelItem(sub);
    }
    m_tooltips->setChecked(config.showTooltips);
    m_legacy->setChecked(config.legacyMenu);
    updateButtons();
}

RadialMenuConfig RadialMenuDialog::config() const
{
    RadialMenuConfig c;
    for (int i = 0; i < m_menu->topLevelItemCount(); ++i) {
        const QTreeWidgetItem* item = m_menu->topLevelItem(i);
        RadialEntry e;
        e.command = item->data(0, kIdRole).toString();
        if (e.isSubmenu()) {
            e.title = item->text(0);
            for (int j = 0; j < item->childCount(); ++j)
                e.children << item->child(j)->data(0, kIdRole).toString();
        }
        c.entries.push_back(e);
    }
    c.showTooltips = m_tooltips->isChecked();
    c.legacyMenu = m_legacy->isChecked();
    return c;
}

void RadialMenuDialog::addCommand()
{
    const QListWidgetItem* chosen = m_available->currentItem();
    if (!chosen)
        return;
    QTreeWidgetItem* item = commandItem(chosen->data(kIdRole).toString());
    if (!item)
        return;
    // Gdzie wstawić: do zaznaczonego podmenu, za zaznaczonym poleceniem albo na koniec.
    QTreeWidgetItem* current = m_menu->currentItem();
    if (isSubmenu(current)) {
        current->addChild(item);
        current->setExpanded(true);
    } else if (current && current->parent()) {
        QTreeWidgetItem* parent = current->parent();
        parent->insertChild(parent->indexOfChild(current) + 1, item);
    } else if (current) {
        m_menu->insertTopLevelItem(m_menu->indexOfTopLevelItem(current) + 1, item);
    } else {
        m_menu->addTopLevelItem(item);
    }
    m_menu->setCurrentItem(item);
}

void RadialMenuDialog::removeItem()
{
    delete m_menu->currentItem(); // podmenu znika razem ze swoimi poleceniami
    updateButtons();
}

void RadialMenuDialog::moveItem(int delta)
{
    QTreeWidgetItem* item = m_menu->currentItem();
    if (!item)
        return;
    const bool expanded = item->isExpanded();
    if (QTreeWidgetItem* parent = item->parent()) {
        const int to = parent->indexOfChild(item) + delta;
        if (to < 0 || to >= parent->childCount())
            return;
        parent->removeChild(item);
        parent->insertChild(to, item);
    } else {
        const int to = m_menu->indexOfTopLevelItem(item) + delta;
        if (to < 0 || to >= m_menu->topLevelItemCount())
            return;
        m_menu->takeTopLevelItem(m_menu->indexOfTopLevelItem(item));
        m_menu->insertTopLevelItem(to, item);
    }
    item->setExpanded(expanded);
    m_menu->setCurrentItem(item);
}

void RadialMenuDialog::newSubmenu()
{
    bool ok = false;
    const QString title =
        QInputDialog::getText(this, "Nowe podmenu", "Nazwa podmenu:", QLineEdit::Normal, "Nowe podmenu", &ok).trimmed();
    if (!ok || title.isEmpty())
        return;
    QTreeWidgetItem* item = submenuItem(title);
    // Podmenu wstawiamy na najwyższym poziomie, za zaznaczoną pozycją.
    QTreeWidgetItem* current = m_menu->currentItem();
    if (current && current->parent())
        current = current->parent();
    m_menu->insertTopLevelItem(current ? m_menu->indexOfTopLevelItem(current) + 1 : m_menu->topLevelItemCount(), item);
    m_menu->setCurrentItem(item);
}

void RadialMenuDialog::editSubmenu()
{
    QTreeWidgetItem* item = m_menu->currentItem();
    if (item && item->parent())
        item = item->parent();
    if (!isSubmenu(item))
        return;
    bool ok = false;
    const QString title =
        QInputDialog::getText(this, "Edytuj podmenu", "Nazwa podmenu:", QLineEdit::Normal, item->text(0), &ok).trimmed();
    if (ok && !title.isEmpty())
        item->setText(0, title);
}

void RadialMenuDialog::importConfig()
{
    const QString path = QFileDialog::getOpenFileName(this, "Importuj menu radialne", QString(), "Ustawienia (*.ini)");
    if (path.isEmpty())
        return;
    RadialMenuConfig c = config();
    if (!c.importFrom(path)) {
        QMessageBox::warning(this, windowTitle(), "W pliku nie ma ustawień menu radialnego.");
        return;
    }
    showConfig(c);
}

void RadialMenuDialog::exportConfig()
{
    const QString path =
        QFileDialog::getSaveFileName(this, "Eksportuj menu radialne", "menu-radialne.ini", "Ustawienia (*.ini)");
    if (path.isEmpty())
        return;
    if (!config().exportTo(path))
        QMessageBox::warning(this, windowTitle(), "Nie udało się zapisać pliku.");
}

void RadialMenuDialog::updateButtons()
{
    m_add->setEnabled(m_available->currentItem() != nullptr);
    m_remove->setEnabled(m_menu->currentItem() != nullptr);
    QTreeWidgetItem* item = m_menu->currentItem();
    m_editSubmenu->setEnabled(isSubmenu(item) || (item && item->parent()));
}
