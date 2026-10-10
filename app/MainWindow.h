#pragma once

#include <QMainWindow>

#include "FeatureRecognition.h"
#include "ModelAlign.h"
#include "ModelImport.h"

class OccView;
class QDockWidget;
class QTreeWidget;
class QTreeWidgetItem;

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
    void onFeatureClicked(QTreeWidgetItem* item);

private:
    void createRibbon();
    void createDock();
    void dockToHome();
    void showAligned();
    void showModel(const camcore::ImportedModel& model);
    void clearFeatures();

    OccView* m_view = nullptr;
    QDockWidget* m_dock = nullptr;
    QTreeWidget* m_featureTree = nullptr;
    camcore::ImportedModel m_shown;          // model tak, jak jest teraz na ekranie
    std::vector<camcore::Feature> m_features; // wynik ostatniego rozpoznawania
    camcore::ImportedModel m_original; // model dokładnie jak w pliku (przed wyrównaniem)
    QString m_fileName;
    bool m_aligned = false; // czy pokazany model jest wyrównany
    bool m_flipped = false; // czy leży na przeciwnej stronie niż wybrał algorytm
    camcore::AlignSettings m_alignSettings;
};
