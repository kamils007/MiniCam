#pragma once

#include <QDialog>
#include <QList>

#include "RadialMenu.h"

class QAction;
class QCheckBox;
class QListWidget;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

// Okno "Dostosuj menu radialne" (jak Customise → Radial Menu w Alphacam):
// dodawanie, usuwanie i kolejność poleceń, podmenu, podpowiedzi, stare menu
// kontekstowe, import, eksport i przywrócenie ustawień domyślnych.
class RadialMenuDialog : public QDialog
{
    Q_OBJECT

public:
    RadialMenuDialog(const RadialMenuConfig& config, const QList<QAction*>& commands, QWidget* parent = nullptr);

    RadialMenuConfig config() const;

private:
    void showConfig(const RadialMenuConfig& config);
    QTreeWidgetItem* commandItem(const QString& id) const;
    QTreeWidgetItem* submenuItem(const QString& title) const;
    void addCommand();
    void removeItem();
    void moveItem(int delta);
    void newSubmenu();
    void editSubmenu();
    void importConfig();
    void exportConfig();
    void updateButtons();

    QList<QAction*> m_commands;
    QListWidget* m_available = nullptr;
    QTreeWidget* m_menu = nullptr;
    QCheckBox* m_tooltips = nullptr;
    QCheckBox* m_legacy = nullptr;
    QPushButton* m_add = nullptr;
    QPushButton* m_remove = nullptr;
    QPushButton* m_editSubmenu = nullptr;
};
