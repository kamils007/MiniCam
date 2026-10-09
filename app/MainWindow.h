#pragma once

#include <QMainWindow>

#include "ModelAlign.h"
#include "ModelImport.h"

class OccView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    void openFile(const QString& path);

private slots:
    void onOpen();
    void onAutoAlign();
    void onAlignSettings();

private:
    void createRibbon();

    OccView* m_view = nullptr;
    camcore::ImportedModel m_original; // model dokładnie jak w pliku (przed wyrównaniem)
    QString m_fileName;
    camcore::AlignSettings m_alignSettings;
};
