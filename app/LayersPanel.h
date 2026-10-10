#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include <vector>

class QTreeWidget;
class QTreeWidgetItem;

// Panel "Warstwy" jak w Alphacam: pionowy pasek zakładek po lewej, pasek
// narzędzi warstw u góry i drzewo warstw z checkboxami (widoczność).
// Na razie wszystkie geometrie trafiają do warstwy "Geometrie APS"
// (niesklasyfikowane); klasyfikacja do innych warstw przyjdzie później.
class LayersPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LayersPanel(QWidget* parent = nullptr);

    // Warstwa użytkownika (pod "Warstwy Użytkownika"): nazwa i kolor.
    struct LayerRow
    {
        QString name;
        QColor color;
    };
    void setUserLayers(const std::vector<LayerRow>& layers);

    // Geometria widoczna w drzewku: opis, warstwa i widoczność (checkbox).
    struct GeometryRow
    {
        QString label;
        QString layer; // "Geometrie APS" albo nazwa warstwy użytkownika
        bool visible = true;
    };
    // Geometrie trafiają pod swoje warstwy – jedna pozycja na geometrię.
    void setGeometries(const std::vector<GeometryRow>& rows);
    // Bryła w warstwie Bryły (pusty tekst = brak bryły).
    void setModelName(const QString& name);

signals:
    void geometryVisibilityChanged(int index, bool visible);
    void modelVisibilityChanged(bool visible);
    void geometriesSelected(const std::vector<int>& indices); // do podświetlenia

private slots:
    void onItemChanged(QTreeWidgetItem* item);
    void onItemClicked(QTreeWidgetItem* item);

private:
    QTreeWidgetItem* addLayer(const QString& name, int iconKind);
    QTreeWidgetItem* geometryLayer(const QString& name) const;
    std::vector<QTreeWidgetItem*> geometryLayers() const;
    static bool isGeometryLayer(const QTreeWidgetItem* item);
    void setLayerChecked(QTreeWidgetItem* layer, Qt::CheckState state);
    void updateCounts();

    QTreeWidget* m_tree = nullptr;
    QTreeWidgetItem* m_aps = nullptr;    // Geometrie APS
    QTreeWidgetItem* m_solids = nullptr; // Bryły
    QTreeWidgetItem* m_user = nullptr;   // Warstwy Użytkownika
    bool m_updating = false; // zmieniamy checkboxy z kodu – nie reagujemy na itemChanged
};
