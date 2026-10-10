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
    group->setAttribute(Qt::WA_StyledBackground);
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

    // Kolory ustawiamy na sztywno, niezależnie od trybu ciemnego Windows:
    // ciemny pasek zakładek, jasny pas z ikonami (jak w Alphacam).
    // WA_StyledBackground – bez tego własne klasy widżetów ignorują "background".
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(R"(
        Ribbon, Ribbon > QTabBar { background: #595959; }
        QTabWidget::pane { background: #e1e1e1; border: none; }
        QTabBar::tab { background: #595959; color: white; padding: 6px 16px; border: none; }
        QTabBar::tab:selected { background: #e1e1e1; color: black; }
        QTabBar::tab:hover:!selected { background: #6e6e6e; }
        QToolButton#ribbonFileButton { background: #1f5fa8; color: white; padding: 6px 20px; border: none; }
        QToolButton#ribbonFileButton::menu-indicator { image: none; }
        RibbonPage, RibbonGroup { background: #e1e1e1; }
        RibbonGroup QToolButton { color: black; background: transparent; border: 1px solid transparent; }
        RibbonGroup QToolButton:hover { background: #cfe3f7; border: 1px solid #8fb8e3; }
        RibbonGroup QToolButton:pressed { background: #a9cdf0; }
        RibbonGroup QToolButton:disabled { color: #9a9a9a; }
        QLabel#ribbonGroupTitle { color: #404040; background: transparent; }
        QFrame[frameShape="5"] { color: #b4b4b4; }
    )");
}

RibbonPage* Ribbon::addPage(const QString& title)
{
    auto* page = new RibbonPage(this);
    page->setAttribute(Qt::WA_StyledBackground);
    addTab(page, title);
    return page;
}
