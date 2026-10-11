#pragma once

#include <QMainWindow>

#include "Geometry.h"

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include "PartContours.h"
#include "ModelAlign.h"
#include "ModelImport.h"
#include "RadialMenu.h"
#include "commands/Command.h"

class OccView;
class CommandBar;
class QDockWidget;
class LayersPanel;
class InputBar;
class QWidget;
class QAction;
class QLabel;
class QCloseEvent;
class QMenu;
class QPrinter;
class QToolBar;
class QToolButton;
class QUndoStack;

class MainWindow : public QMainWindow, public CommandHost
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    void openFile(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onOpen();
    void onAutoAlign();
    void onAlignSettings();
    void onRecognizeFeatures();
    void onPointPicked(double x, double y, double z);
    void onPointEntered(double x, double y, double z);
    void cancelCommand();

private:
    void createRibbon();
    void createFileMenu(QMenu* menu, QAction* quitAct); // FileMenu.cpp
    void addRecentFile(const QString& path);
    void updateRecentMenu();
    void onNew();
    bool onSave();
    bool onSaveAs();
    bool saveTo(const QString& path);
    bool maybeSave(); // pyta o zapis zmian; false = użytkownik anulował
    void onPrint(bool preview);
    void printTo(QPrinter* printer);
    void createDock();
    void createBottomBars();

public:
    // CommandHost – to, czego potrzebują polecenia (app/commands).
    OccView* view() override { return m_view; }
    InputBar* inputBar() override { return m_inputBar; }
    void showMessage(const QString& message) override;
    void releaseSnap() override;
    EditState editState() const override { return {m_shown, m_geometries}; }
    void commitEdit(const QString& text, const EditState& before, const EditState& after) override;
    void commandFinished(const QString& message) override;

    void restoreEditState(const EditState& state);

private:
    // Uruchamia polecenie (poprzednie, jeśli trwa, zostaje przerwane).
    void runCommand(Command* command);
    void showRadialMenu(const QPoint& globalPos); // menu pod PPM w widoku 3D
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

    Command* m_command = nullptr;     // trwające polecenie (np. Przesuń) albo brak
    QAction* m_lastCommand = nullptr; // ostatnie polecenie – powtarza je spacja
    QAction* m_repeatAct = nullptr;   // skrót spacji
    // Pola menu radialnego (PPM), od godz. 3 zgodnie z ruchem wskazówek zegara
    // (pole 1 zaczyna się na godz. 3, środek pola 11 jest na godz. 12).
    // Puste pola (action = nullptr) uzupełniamy poleceniami na bieżąco.
    std::vector<RadialSlot> m_radialSlots;
    QUndoStack* m_undo = nullptr; // historia zmian do cofania (Ctrl+Z) i ponawiania (Ctrl+Y)
    CommandBar* m_commandBar = nullptr;     // belka polecenia (druga od dołu)
    QLabel* m_cursorLabel = nullptr;        // współrzędne kursora w stopce
    InputBar* m_inputBar = nullptr;         // pasek wprowadzania (lewa strona belki)
    camcore::ImportedModel m_original; // model dokładnie jak w pliku (przed wyrównaniem)
    QString m_fileName;
    QMenu* m_recentMenu = nullptr; // Plik → Ostatnie
    QString m_savePath; // plik, do którego zapisuje "Zapisz" (pusty = zapytaj o nazwę)
    bool m_aligned = false; // czy pokazany model jest wyrównany
    bool m_flipped = false; // czy leży na przeciwnej stronie niż wybrał algorytm
    camcore::AlignSettings m_alignSettings;
};
