#pragma once

#include <QDialog>

#include "ModelAlign.h"

class QButtonGroup;
class QCheckBox;
class QSettings;

// Ustawienia trzymamy w rejestrze Windows (QSettings) – przetrwają zamknięcie programu.
camcore::AlignSettings loadAlignSettings();
void saveAlignSettings(const camcore::AlignSettings& s);

// Okno "Konfiguracja" z zakładką "Auto-wyrównanie" (układ jak w Alphacam).
class AlignSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AlignSettingsDialog(const camcore::AlignSettings& settings, QWidget* parent = nullptr);
    camcore::AlignSettings settings() const;

private:
    void setSettings(const camcore::AlignSettings& s);
    void exportToFile();
    void importFromFile();
    QWidget* createAutoAlignTab();

    QCheckBox* m_afterImport = nullptr;
    QCheckBox* m_lathe = nullptr;
    QCheckBox* m_panel = nullptr;
    QCheckBox* m_minimalBox = nullptr;
    QButtonGroup* m_zZero = nullptr;
    QButtonGroup* m_longEdge = nullptr;
    QButtonGroup* m_baseX = nullptr;
    QButtonGroup* m_baseY = nullptr;
};
