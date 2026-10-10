#pragma once

#include <QMainWindow>

#include "Geometry.h"
#include "PartContours.h"
#include "ModelAlign.h"
#include "ModelImport.h"

class OccView;
class QDockWidget;
class LayersPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    void openFile(const QString& path);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onOpen();
    void onAutoAlign();
    void onAlignSettings();
    void onRecognizeFeatures();

private:
    void createRibbon();
    void createDock();
    void dockToHome();
    void showAligned();
    void showModel(const camcore::ImportedModel& model);
    void clearFeatures();
    void showGeometries();
    void setGeometryVisible(int index, bool visible);

    OccView* m_view = nullptr;
    QDockWidget* m_dock = nullptr;
    LayersPanel* m_layers = nullptr;
    camcore::ImportedModel m_shown;          // model tak, jak jest teraz na ekranie
    camcore::PartContours m_contours; // kontury i kieszenie z ostatniego rozpoznawania
    std::vector<camcore::Geometry> m_geometries; // geometrie z właściwościami (kolor, widoczność)
    camcore::ImportedModel m_original; // model dokładnie jak w pliku (przed wyrównaniem)
    QString m_fileName;
    bool m_aligned = false; // czy pokazany model jest wyrównany
    bool m_flipped = false; // czy leży na przeciwnej stronie niż wybrał algorytm
    camcore::AlignSettings m_alignSettings;
};
