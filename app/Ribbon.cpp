#include "Ribbon.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

RibbonGroup::RibbonGroup(const QString& title, QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 0);
    layout->setSpacing(0);

    m_buttons = new QHBoxLayout;
    m_buttons->setSpacing(2);
    layout->addLayout(m_buttons);
    layout->addStretch();

    auto* label = new QLabel(title, this);
    label->setAlignment(Qt::AlignCenter);
    label->setObjectName("ribbonGroupTitle");
    layout->addWidget(label);
}

void RibbonGroup::addAction(QAction* action)
{
    // QToolButton powiązany z akcją: kliknięcie = action->trigger(),
    // a tekst, ikona i dostępność (enabled) biorą się z akcji.
    auto* button = new QToolButton(this);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setIconSize(QSize(32, 32));
    button->setAutoRaise(true);
    button->setMinimumWidth(64);
    m_buttons->addWidget(button);
}

RibbonPage::RibbonPage(QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(4, 2, 4, 2);
    m_layout->setSpacing(4);
    m_layout->addStretch(); // grupy dosuwane do lewej
}

RibbonGroup* RibbonPage::addGroup(const QString& title)
{
    // Pionowa kreska oddzielająca grupy.
    if (m_layout->count() > 1) {
        auto* line = new QFrame(this);
        line->setFrameShape(QFrame::VLine);
        line->setFrameShadow(QFrame::Sunken);
        m_layout->insertWidget(m_layout->count() - 1, line);
    }
    auto* group = new RibbonGroup(title, this);
    m_layout->insertWidget(m_layout->count() - 1, group);
    return group;
}

Ribbon::Ribbon(QWidget* parent)
    : QTabWidget(parent)
{
    setDocumentMode(true);
    setFixedHeight(118);

    auto* fileButton = new QToolButton(this);
    fileButton->setText("Plik");
    fileButton->setObjectName("ribbonFileButton");
    fileButton->setPopupMode(QToolButton::InstantPopup);
    m_fileMenu = new QMenu(fileButton);
    fileButton->setMenu(m_fileMenu);
    setCornerWidget(fileButton, Qt::TopLeftCorner);

    setStyleSheet(R"(
        QTabWidget::pane { background: #d9d9d9; border: none; border-top: 1px solid #b0b0b0; }
        QTabBar::tab { background: #595959; color: white; padding: 5px 14px; border: none; }
        QTabBar::tab:selected { background: #d9d9d9; color: black; }
        QTabBar::tab:hover:!selected { background: #6e6e6e; }
        QToolButton#ribbonFileButton { background: #1f5fa8; color: white; padding: 5px 18px; border: none; }
        QToolButton#ribbonFileButton::menu-indicator { image: none; }
        QLabel#ribbonGroupTitle { color: #404040; }
        RibbonPage { background: #d9d9d9; }
    )");
}

RibbonPage* Ribbon::addPage(const QString& title)
{
    auto* page = new RibbonPage(this);
    addTab(page, title);
    return page;
}
