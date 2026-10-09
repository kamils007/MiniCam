#pragma once

#include <QMainWindow>

class OccView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    void openFile(const QString& path);

private slots:
    void onOpen();
    void onSelectionChanged();

private:
    OccView* m_view = nullptr;
    QString m_fileName; // nazwa wczytanego pliku – do paska stanu
};
