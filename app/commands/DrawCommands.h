#pragma once

#include "Command.h"

#include <functional>
#include <optional>
#include <vector>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>

// Rysowanie geometrii 2D (zakładka Geometria → 2D, jak w Alphacam). Punkty klika się
// w widoku (z uchwytami) albo wpisuje w pasku wprowadzania; rysujemy na płaszczyźnie Z = 0.
// Każde polecenie działa, dopóki się go nie przerwie (PPM albo Esc): po narysowaniu
// elementu od razu pyta o kolejny. Nowa geometria trafia do warstwy APS, Ctrl+Z ją cofa.
class DrawCommand : public Command
{
    Q_OBJECT

public:
    using Command::Command;
    bool start() override;
    void cancel() override; // PPM / Esc kończy rysowanie; narysowane elementy zostają

protected:
    // Pierwszy krok polecenia (pytanie o pierwszy punkt albo wartość).
    virtual void restart() = 0;

    void askPoint(const QString& prompt);
    void askValue(const QString& prompt, const QString& label, double initial, bool clicks = false);
    // Podgląd elementu od ostatniego punktu do kursora (pusta funkcja = bez podglądu).
    void preview(std::function<TopoDS_Shape(const gp_Pnt&)> build);
    // Dodaje geometrię z elementów (replace >= 0: podmienia tę geometrię) i zapisuje
    // zmianę na liście Cofnij. Zwraca numer geometrii.
    int addGeometry(const std::vector<TopoDS_Edge>& edges, int replace = -1);
    void notBuilt(); // komunikat, gdy z podanych punktów nie da się zbudować elementu
};

// Linia: punkt 'Od', potem kolejne punkty 'Do' – odcinki łączą się w jedną geometrię.
// Kliknięcie w punkt początkowy zamyka kontur i zaczyna nową linię.
class LineCommand : public DrawCommand
{
    Q_OBJECT

public:
    using DrawCommand::DrawCommand;
    QString name() const override { return "Linia"; }
    void pointEntered(const gp_Pnt& p) override;

protected:
    void restart() override;

private:
    std::vector<gp_Pnt> m_points;
    std::vector<TopoDS_Edge> m_edges;
    int m_geometry = -1; // rysowana geometria (po pierwszym odcinku)
};

// Łuk – metody jak w Alphacam (Geometria → Łuk ►).
class ArcCommand : public DrawCommand
{
    Q_OBJECT

public:
    enum class Method { StartSecondEnd, TwoPointsRadius, TwoPointsCentre, StartCentreAngle };
    ArcCommand(Method method, CommandHost& host, QObject* parent = nullptr)
        : DrawCommand(host, parent), m_method(method)
    {
    }
    QString name() const override { return "Łuk"; }
    void pointEntered(const gp_Pnt& p) override;
    void valueEntered(double value) override;

protected:
    void restart() override;

private:
    void build(const std::optional<TopoDS_Edge>& arc);

    Method m_method;
    std::vector<gp_Pnt> m_points;
};

// Koło – metody jak w Alphacam (Geometria → Koło ►).
class CircleCommand : public DrawCommand
{
    Q_OBJECT

public:
    enum class Method { CentreDiameter, CentreRadius, CentrePoint, TwoPoints, ThreePoints };
    CircleCommand(Method method, CommandHost& host, QObject* parent = nullptr)
        : DrawCommand(host, parent), m_method(method)
    {
    }
    QString name() const override { return "Koło"; }
    void pointEntered(const gp_Pnt& p) override;
    void valueEntered(double value) override;

protected:
    void restart() override;

private:
    void askCentre();
    void build(const std::optional<TopoDS_Edge>& circle);

    Method m_method;
    std::vector<gp_Pnt> m_points;
    double m_radius = 0; // średnica/promień podane na początku (Środek + Średnica/Promień)
};

// Prostokąt: dwa przeciwległe narożniki.
class RectangleCommand : public DrawCommand
{
    Q_OBJECT

public:
    using DrawCommand::DrawCommand;
    QString name() const override { return "Prostokąt"; }
    void pointEntered(const gp_Pnt& p) override;

protected:
    void restart() override;

private:
    std::vector<gp_Pnt> m_points;
};
