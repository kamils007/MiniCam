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

    // Geometria widoczna w drzewku: opis, kolor (kwadracik) i widoczność (checkbox).
    struct GeometryRow
    {
        QString label;
        QColor color;
        bool visible = true;
    };
    // Geometrie w warstwie APS – jedna pozycja na geometrię (pusta lista = brak).
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
    void updateCounts();

    QTreeWidget* m_tree = nullptr;
    QTreeWidgetItem* m_aps = nullptr;    // Geometrie APS
    QTreeWidgetItem* m_solids = nullptr; // Bryły
    bool m_updating = false; // zmieniamy checkboxy z kodu – nie reagujemy na itemChanged
};
