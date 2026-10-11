#pragma once

#include <QList>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <vector>

class QAction;
class QMenu;
class QSettings;

// Menu radialne pod PPM (jak w Alphacam): pierścień ikon wokół kursora.
// Pozycja z » to podmenu – po najechaniu otwiera się zewnętrzny łuk z jego poleceniami.
// Polecenia to akcje okna (QAction) rozpoznawane po objectName, np. "move", "undo".

// Jedna pozycja menu: polecenie albo podmenu z listą poleceń.
struct RadialEntry
{
    QString command;      // objectName akcji; puste = podmenu
    QString title;        // nazwa podmenu
    QStringList children; // polecenia w podmenu

    bool isSubmenu() const { return command.isEmpty(); }
};

// Układ menu i jego opcje (okno "Dostosuj menu radialne"). Zapis w rejestrze
// (QSettings), eksport i import do pliku .ini.
struct RadialMenuConfig
{
    std::vector<RadialEntry> entries; // kolejność zgodnie z ruchem wskazówek zegara od godz. 3
    bool showTooltips = true;
    bool legacyMenu = false; // zamiast pierścienia zwykłe menu kontekstowe

    static RadialMenuConfig defaults(); // układ jak w Alphacam
    static RadialMenuConfig load();
    void save() const;
    bool exportTo(const QString& path) const;
    bool importFrom(const QString& path);

private:
    void write(QSettings& q) const;
    bool read(QSettings& q);
};

// Szuka polecenia po objectName (nullptr = brak).
QAction* findCommand(const QList<QAction*>& commands, const QString& id);

// Zwykłe menu kontekstowe z tymi samymi poleceniami ("Użyj starego menu kontekstowego").
QMenu* createLegacyMenu(const RadialMenuConfig& config, const QList<QAction*>& commands, QWidget* parent);

class RadialMenu : public QWidget
{
    Q_OBJECT

public:
    RadialMenu(const RadialMenuConfig& config, const QList<QAction*>& commands, QWidget* parent = nullptr);

    // Pokazuje menu ze środkiem w punkcie ekranu (tam, gdzie kliknięto PPM).
    void popup(const QPoint& globalCenter);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    struct Slot
    {
        QAction* action = nullptr;        // polecenie (nullptr = podmenu)
        QString title;                    // nazwa podmenu
        std::vector<QAction*> children;   // polecenia podmenu
    };
    struct Hit
    {
        int ring = -1;  // 0 = wewnętrzny pierścień, 1 = łuk podmenu, -1 = nic
        int index = -1;
    };

    QPointF center() const;
    double slotAngle(int index) const;  // środek pozycji w stopniach (przeciwnie do zegara od godz. 3)
    double childAngle(int index) const; // środek pozycji na łuku otwartego podmenu
    double innerStep() const;
    Hit hitTest(const QPointF& pos) const;
    QAction* actionAt(const Hit& hit) const;
    QString tipFor(const Hit& hit) const;
    QRect tipRect() const; // ramka podpowiedzi pod pierścieniem (pusta = brak)
    void updateMask();

    std::vector<Slot> m_slots;
    bool m_showTooltips = true;
    int m_open = -1; // otwarte podmenu (indeks w m_slots)
    Hit m_hover;
    QPointF m_mouse;
};
