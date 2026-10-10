#pragma once

#include <QMainWindow>

#include "Geometry.h"

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include "PartContours.h"
#include "ModelAlign.h"
#include "ModelImport.h"

class OccView;
class QDockWidget;
class LayersPanel;
class InputBar;
class QWidget;
class QAction;
class QLabel;
class QToolBar;

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
    void onMove();
    void onSelectionConfirmed();
    void onPointPicked(double x, double y, double z);
    void onPointEntered(double x, double y, double z);
    void cancelMove();

private:
    void createRibbon();
    void createDock();
    void createBottomBars();
    void applyMove(const gp_Vec& offset);
    void finishMove(const QString& message);
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
    std::vector<camcore::Geometry> m_geometries; // geometrie z właściwościami (warstwa, widoczność)
    camcore::LayerList m_layerList; // warstwy: APS + warstwy użytkownika

    // Polecenie "Przesuń": wybór elementów → punkt bazowy → punkt docelowy
    // (punkty klikane w widoku albo wpisane w pasku wprowadzania).
    enum class MoveStep { None, Selecting, PickBase, PickTarget };
    MoveStep m_moveStep = MoveStep::None;
    std::vector<int> m_moveGeometries; // wybrane geometrie
    bool m_moveModel = false;          // czy wybrano bryłę
    gp_Pnt m_moveBase;
    QToolBar* m_commandBar = nullptr;       // belka polecenia (druga od dołu)
    QLabel* m_cursorLabel = nullptr;        // współrzędne kursora w stopce
    InputBar* m_inputBar = nullptr;         // pasek wprowadzania (lewa strona belki)
    camcore::ImportedModel m_original; // model dokładnie jak w pliku (przed wyrównaniem)
    QString m_fileName;
    bool m_aligned = false; // czy pokazany model jest wyrównany
    bool m_flipped = false; // czy leży na przeciwnej stronie niż wybrał algorytm
    camcore::AlignSettings m_alignSettings;
};
