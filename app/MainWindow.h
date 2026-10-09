#pragma once

#include <QMainWindow>

#include "ModelAlign.h"
#include "ModelImport.h"

class OccView;
class QDockWidget;

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

private:
    void createRibbon();
    void createDock();
    void dockToHome();
    void showAligned();

    OccView* m_view = nullptr;
    QDockWidget* m_dock = nullptr;
    camcore::ImportedModel m_original; // model dokładnie jak w pliku (przed wyrównaniem)
    QString m_fileName;
    bool m_aligned = false; // czy pokazany model jest wyrównany
    bool m_flipped = false; // czy leży na przeciwnej stronie niż wybrał algorytm
    camcore::AlignSettings m_alignSettings;
};
