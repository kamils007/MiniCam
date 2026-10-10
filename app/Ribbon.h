#pragma once

#include <QTabWidget>

class QAction;
class QHBoxLayout;
class QMenu;

// Prosta "wstążka" w stylu Alphacam/Office: zakładki, w nich grupy,
// w grupach duże przyciski (ikona nad tekstem) z podpisem grupy na dole.
class RibbonGroup : public QWidget
{
    Q_OBJECT

public:
    RibbonGroup(const QString& title, QWidget* parent = nullptr);
    void addAction(QAction* action);

private:
    QHBoxLayout* m_buttons = nullptr;
};

class RibbonPage : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonPage(QWidget* parent = nullptr);
    RibbonGroup* addGroup(const QString& title);

private:
    QHBoxLayout* m_layout = nullptr;
};

class Ribbon : public QTabWidget
{
    Q_OBJECT

public:
    explicit Ribbon(QWidget* parent = nullptr);

    // Niebieski przycisk "Plik" po lewej stronie zakładek, rozwija menu.
    QMenu* fileMenu() const { return m_fileMenu; }
    RibbonPage* addPage(const QString& title);

private:
    QMenu* m_fileMenu = nullptr;
};
