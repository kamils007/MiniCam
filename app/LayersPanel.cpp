#include "LayersPanel.h"

#include <QAction>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStyle>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

enum IconKind { Aps, Construction, Toolpath, Dimension, Spline, Surface, Text, Solid, Stl, UserLayers };

// Małe ikonki warstw rysowane w kodzie (podobne do tych z Alphacam).
QIcon layerIcon(int kind)
{
    QPixmap pix(20, 20);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor red(200, 40, 30), yellow(240, 200, 40), gray(150, 150, 150);
    switch (kind) {
    case Aps: { // łamana z punktami
        p.setPen(QPen(red, 1.6));
        const QPointF pts[] = {{3, 16}, {8, 6}, {13, 12}, {17, 4}};
        p.drawPolyline(pts, 4);
        p.setPen(QPen(yellow.darker(130), 1));
        p.setBrush(yellow);
        for (const QPointF& pt : pts)
            p.drawRect(QRectF(pt.x() - 1.5, pt.y() - 1.5, 3, 3));
        break;
    }
    case Construction: // krzyż konstrukcyjny
        p.setPen(QPen(Qt::black, 1.2));
        p.drawLine(2, 6, 18, 6);
        p.drawLine(6, 2, 6, 18);
        p.setPen(QPen(red, 1.4));
        p.drawLine(12, 12, 18, 18);
        p.drawLine(18, 12, 12, 18);
        break;
    case Toolpath: { // ścieżka narzędzia
        p.setPen(QPen(red, 2));
        QPainterPath path(QPointF(10, 18));
        path.cubicTo(6, 12, 12, 8, 8, 3);
        p.drawPath(path);
        p.setBrush(yellow);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(8, 3), 2.5, 2.5);
        break;
    }
    case Dimension: // linijka
        p.setPen(QPen(yellow.darker(150), 1));
        p.setBrush(yellow);
        p.drawRect(QRectF(1, 7, 18, 6));
        for (int x = 3; x < 19; x += 3)
            p.drawLine(x, 7, x, 10);
        break;
    case Spline: { // krzywa S
        p.setPen(QPen(red, 2));
        QPainterPath path(QPointF(5, 18));
        path.cubicTo(18, 16, 2, 4, 15, 2);
        p.drawPath(path);
        break;
    }
    case Surface: // powierzchnia
        p.setPen(QPen(yellow.darker(150), 1));
        p.setBrush(yellow);
        p.drawEllipse(QRectF(1, 5, 18, 10));
        break;
    case Text:
        p.setPen(Qt::black);
        p.setFont(QFont("Arial", 11));
        p.drawText(pix.rect(), Qt::AlignCenter, "Ab");
        break;
    case Solid:
    case Stl: { // prostopadłościan
        p.setPen(QPen(gray.darker(150), 1));
        p.setBrush(QColor(235, 235, 235));
        const QPointF top[] = {{2, 8}, {7, 4}, {18, 4}, {13, 8}};
        p.drawPolygon(top, 4);
        p.setBrush(gray);
        const QPointF side[] = {{13, 8}, {18, 4}, {18, 11}, {13, 15}};
        p.drawPolygon(side, 4);
        p.setBrush(QColor(200, 200, 200));
        p.drawRect(QRectF(2, 8, 11, 7));
        break;
    }
    case UserLayers: // stos warstw
        p.setPen(QPen(Qt::black, 1));
        p.setBrush(Qt::white);
        p.drawRect(QRectF(7, 2, 11, 9));
        p.drawRect(QRectF(4, 5, 11, 9));
        p.drawRect(QRectF(1, 8, 11, 9));
        break;
    }
    return QIcon(pix);
}

// Akcja-zaślepka: widać ją na pasku, ale jeszcze nic nie robi.
QAction* placeholder(QToolBar* bar, const QIcon& icon, const QString& text)
{
    QAction* a = bar->addAction(icon, text);
    a->setToolTip(text + " (wkrótce)");
    a->setEnabled(false);
    return a;
}

} // namespace

LayersPanel::LayersPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Pionowy pasek po lewej – zakładki panelu. Na razie jest tylko "Warstwy".
    auto* side = new QToolBar(this);
    side->setOrientation(Qt::Vertical);
    side->setIconSize(QSize(20, 20));
    side->setObjectName("layersSide");
    QAction* layersTab = side->addAction(layerIcon(UserLayers), "Warstwy");
    layersTab->setCheckable(true);
    layersTab->setChecked(true);
    placeholder(side, style()->standardIcon(QStyle::SP_FileDialogDetailedView), "Operacje");
    placeholder(side, style()->standardIcon(QStyle::SP_DriveHDIcon), "Narzędzia");
    placeholder(side, style()->standardIcon(QStyle::SP_FileDialogInfoView), "Właściwości");
    layout->addWidget(side);

    auto* right = new QVBoxLayout;
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(0);
    layout->addLayout(right);

    // Pasek narzędzi warstw – przyciski przyjdą razem z klasyfikacją geometrii.
    auto* bar = new QToolBar(this);
    bar->setIconSize(QSize(20, 20));
    bar->setObjectName("layersBar");
    placeholder(bar, style()->standardIcon(QStyle::SP_FileDialogContentsView), "Właściwości warstwy");
    placeholder(bar, style()->standardIcon(QStyle::SP_FileDialogNewFolder), "Nowa warstwa");
    placeholder(bar, style()->standardIcon(QStyle::SP_DialogDiscardButton), "Usuń warstwę");
    placeholder(bar, style()->standardIcon(QStyle::SP_ArrowUp), "Przenieś w górę");
    placeholder(bar, style()->standardIcon(QStyle::SP_ArrowDown), "Przenieś w dół");
    right->addWidget(bar);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setIconSize(QSize(20, 20));
    m_tree->setRootIsDecorated(true);
    right->addWidget(m_tree);

    m_aps = addLayer("Geometrie APS", Aps);
    addLayer("Konstrukcje", Construction);
    addLayer("Drogi Narzędzia", Toolpath);
    addLayer("Wymiary", Dimension);
    addLayer("Splajny", Spline);
    addLayer("Powierzchnie", Surface);
    addLayer("Tekst", Text);
    m_solids = addLayer("Bryły", Solid);
    addLayer("STL", Stl);
    addLayer("Warstwy Użytkownika", UserLayers);

    connect(m_tree, &QTreeWidget::itemChanged, this, &LayersPanel::onItemChanged);
    connect(m_tree, &QTreeWidget::itemClicked, this, &LayersPanel::onItemClicked);
}

QTreeWidgetItem* LayersPanel::addLayer(const QString& name, int iconKind)
{
    auto* item = new QTreeWidgetItem(m_tree, {name});
    item->setIcon(0, layerIcon(iconKind));
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    item->setCheckState(0, Qt::Checked);
    item->setData(0, Qt::UserRole + 1, name); // nazwa bez licznika
    return item;
}

void LayersPanel::setGeometries(const std::vector<GeometryRow>& rows)
{
    m_updating = true;
    qDeleteAll(m_aps->takeChildren());
    for (size_t i = 0; i < rows.size(); ++i) {
        auto* item = new QTreeWidgetItem(m_aps, {rows[i].label});
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        item->setCheckState(0, rows[i].visible ? Qt::Checked : Qt::Unchecked);
        item->setData(0, Qt::UserRole, static_cast<int>(i)); // numer geometrii
    }
    updateCounts();
    m_updating = false;
}

void LayersPanel::setModelName(const QString& name)
{
    m_updating = true;
    qDeleteAll(m_solids->takeChildren());
    m_solids->setCheckState(0, Qt::Checked); // nowa bryła zawsze widoczna
    if (!name.isEmpty()) {
        auto* item = new QTreeWidgetItem(m_solids, {name});
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        item->setCheckState(0, m_solids->checkState(0));
    }
    updateCounts();
    m_updating = false;
}

void LayersPanel::updateCounts()
{
    // Warstwy z zawartością pokazują liczbę elementów, np. "Geometrie APS (8)".
    for (QTreeWidgetItem* layer : {m_aps, m_solids}) {
        const QString name = layer->data(0, Qt::UserRole + 1).toString();
        layer->setText(0, layer->childCount() ? QString("%1 (%2)").arg(name).arg(layer->childCount()) : name);
    }
}

void LayersPanel::onItemChanged(QTreeWidgetItem* item)
{
    if (m_updating)
        return;
    m_updating = true;
    const bool on = item->checkState(0) == Qt::Checked;
    if (item == m_aps) {
        // Cała warstwa: wszystkie jej geometrie razem.
        for (int i = 0; i < m_aps->childCount(); ++i) {
            m_aps->child(i)->setCheckState(0, item->checkState(0));
            emit geometryVisibilityChanged(i, on);
        }
    } else if (item->parent() == m_aps) {
        emit geometryVisibilityChanged(item->data(0, Qt::UserRole).toInt(), on);
    } else if (item == m_solids || item->parent() == m_solids) {
        // Bryła jest jedna – checkbox warstwy i bryły działają razem.
        m_solids->setCheckState(0, item->checkState(0));
        for (int i = 0; i < m_solids->childCount(); ++i)
            m_solids->child(i)->setCheckState(0, item->checkState(0));
        emit modelVisibilityChanged(on);
    }
    m_updating = false;
}

void LayersPanel::onItemClicked(QTreeWidgetItem* item)
{
    std::vector<int> indices;
    if (item == m_aps) {
        for (int i = 0; i < m_aps->childCount(); ++i)
            indices.push_back(i);
    } else if (item->parent() == m_aps) {
        indices.push_back(item->data(0, Qt::UserRole).toInt());
    }
    emit geometriesSelected(indices); // pusta lista gasi podświetlenie
}
