#include "DrawCommands.h"

#include "InputBar.h"
#include "OccView.h"
#include "Sketch.h"

#include <BRep_Builder.hxx>
#include <QRegularExpression>
#include <cmath>
#include <optional>
#include <TopoDS_Compound.hxx>

namespace {

// Wartości podane ostatnio – następnym razem pasek proponuje je od razu (jak w Alphacam).
double g_circleDiameter = 10.0;
double g_circleRadius = 5.0;
double g_arcRadius = 10.0;
double g_arcAngle = 90.0;

constexpr double kSame = 1e-6; // punkty bliżej siebie uznajemy za ten sam punkt

TopoDS_Shape shapeOf(const std::optional<TopoDS_Edge>& edge)
{
    return edge ? TopoDS_Shape(*edge) : TopoDS_Shape();
}

TopoDS_Shape compoundOf(const std::vector<TopoDS_Edge>& edges)
{
    if (edges.empty())
        return {};
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (const TopoDS_Edge& e : edges)
        builder.Add(compound, e);
    return compound;
}

QString num(double v) { return QString::number(v, 'f', 3).remove(QRegularExpression("\\.?0+$")); }

} // namespace

// ---- DrawCommand ----

bool DrawCommand::start()
{
    host().view()->clearSelection();
    host().view()->setInteraction(OccView::Interaction::PickPoint);
    host().view()->setFocus();
    restart();
    return true;
}

void DrawCommand::cancel()
{
    finish(name() + ": zakończono");
}

void DrawCommand::askPoint(const QString& prompt)
{
    host().inputBar()->startPoint(name() + ":", prompt);
    host().showMessage(QString("%1: %2 – kliknij w widoku albo wpisz X, Y i Enter; PPM lub Esc kończy")
                           .arg(name(), prompt.toLower()));
}

void DrawCommand::askValue(const QString& prompt, const QString& label, double initial, bool clicks)
{
    host().inputBar()->startValue(name() + ":", prompt, label, initial, clicks);
    host().showMessage(QString("%1: wpisz %2 i Enter%3; PPM lub Esc kończy")
                           .arg(name(), label.toLower(), clicks ? " albo kliknij w widoku" : QString()));
}

void DrawCommand::preview(std::function<TopoDS_Shape(const gp_Pnt&)> build)
{
    if (build)
        host().view()->setDrawPreview(std::move(build));
    else
        host().view()->stopDrawPreview();
}

int DrawCommand::addGeometry(const std::vector<TopoDS_Edge>& edges, int replace)
{
    const EditState before = host().editState();
    EditState after = before;
    camcore::Geometry g;
    g.contour = camcore::sketchContour(edges);
    int index = replace;
    if (replace >= 0 && replace < static_cast<int>(after.geometries.size())) {
        g.layer = after.geometries[static_cast<size_t>(replace)].layer;
        after.geometries[static_cast<size_t>(replace)] = g;
    } else {
        after.geometries.push_back(g);
        index = static_cast<int>(after.geometries.size()) - 1;
    }
    host().commitEdit(name(), before, after);
    return index;
}

void DrawCommand::notBuilt()
{
    host().showMessage(name() + ": z tych punktów nie da się narysować elementu – wskaż inny punkt");
}

// ---- Linia ----

void LineCommand::restart()
{
    m_points.clear();
    m_edges.clear();
    m_geometry = -1;
    preview(nullptr);
    askPoint("Od");
}

void LineCommand::pointEntered(const gp_Pnt& p)
{
    if (m_points.empty()) {
        m_points.push_back(p);
        preview([this](const gp_Pnt& c) { return shapeOf(camcore::lineEdge(m_points.back(), c)); });
        askPoint("Do");
        return;
    }
    const auto edge = camcore::lineEdge(m_points.back(), p);
    if (!edge) {
        notBuilt();
        return;
    }
    m_edges.push_back(*edge);
    m_points.push_back(p);
    // Kolejne odcinki to jedna geometria – podmieniamy ją po każdym punkcie (Ctrl+Z cofa odcinek).
    m_geometry = addGeometry(m_edges, m_geometry);
    const gp_Pnt& first = m_points.front();
    if (m_edges.size() >= 2 && std::hypot(p.X() - first.X(), p.Y() - first.Y()) < kSame) {
        const size_t count = m_edges.size();
        restart();
        host().showMessage(QString("Linia: zamknięto kontur (%1 odcinków) – wskaż początek nowej linii").arg(count));
        return;
    }
    askPoint("Do");
}

// ---- Łuk ----

void ArcCommand::restart()
{
    m_points.clear();
    preview(nullptr);
    askPoint("Początek");
}

void ArcCommand::build(const std::optional<TopoDS_Edge>& arc)
{
    if (!arc) {
        if (m_points.size() == 3)
            m_points.pop_back(); // ten punkt się nie nadaje – czekamy na inny
        notBuilt();
        return;
    }
    addGeometry({*arc});
    restart();
}

void ArcCommand::pointEntered(const gp_Pnt& p)
{
    m_points.push_back(p);
    const size_t n = m_points.size();
    if (n == 1) {
        // Od początku do kursora – zwykła linia, dopóki nie ma drugiego punktu.
        preview([this](const gp_Pnt& c) { return shapeOf(camcore::lineEdge(m_points[0], c)); });
        switch (m_method) {
        case Method::StartSecondEnd: askPoint("Drugi punkt"); break;
        case Method::TwoPointsRadius:
        case Method::TwoPointsCentre: askPoint("Koniec"); break;
        case Method::StartCentreAngle: askPoint("Środek łuku"); break;
        }
        return;
    }
    switch (m_method) {
    case Method::StartSecondEnd:
        if (n == 2) {
            preview([this](const gp_Pnt& c) {
                return shapeOf(camcore::arcThreePoints(m_points[0], m_points[1], c));
            });
            askPoint("Koniec");
        } else {
            build(camcore::arcThreePoints(m_points[0], m_points[1], m_points[2]));
        }
        break;
    case Method::TwoPointsRadius:
        preview(nullptr);
        askValue("Promień (dodatni – krótszy łuk, ujemny – dłuższy)", "R", g_arcRadius);
        break;
    case Method::TwoPointsCentre:
        if (n == 2) {
            preview([this](const gp_Pnt& c) {
                return shapeOf(camcore::arcTwoPointsCentre(m_points[0], m_points[1], c));
            });
            askPoint("Środek łuku");
        } else {
            build(camcore::arcTwoPointsCentre(m_points[0], m_points[1], m_points[2]));
        }
        break;
    case Method::StartCentreAngle:
        if (n == 2) {
            // Kąt z kliknięcia: od początku do kursora, przeciwnie do ruchu wskazówek zegara.
            preview([this](const gp_Pnt& c) {
                return shapeOf(camcore::arcStartCentreAngle(
                    m_points[0], m_points[1], camcore::sweepAngleDeg(m_points[1], m_points[0], c)));
            });
            askValue("Kąt rozwarcia (dodatni – przeciwnie do wskazówek zegara)", "Kąt", g_arcAngle, true);
        } else {
            m_points.pop_back(); // kliknięty punkt daje tylko kąt
            valueEntered(camcore::sweepAngleDeg(m_points[1], m_points[0], p));
        }
        break;
    }
}

void ArcCommand::valueEntered(double value)
{
    if (m_method == Method::TwoPointsRadius && m_points.size() == 2) {
        const double chord = m_points[0].Distance(m_points[1]);
        const auto arc = camcore::arcTwoPointsRadius(m_points[0], m_points[1], value);
        if (!arc) {
            host().showMessage(QString("Łuk: promień musi mieć co najmniej %1 (połowa odległości punktów)")
                                   .arg(num(chord / 2)));
            return;
        }
        g_arcRadius = value;
        build(arc);
    } else if (m_method == Method::StartCentreAngle && m_points.size() == 2) {
        g_arcAngle = value;
        build(camcore::arcStartCentreAngle(m_points[0], m_points[1], value));
    }
}

// ---- Koło ----

void CircleCommand::restart()
{
    m_points.clear();
    preview(nullptr);
    switch (m_method) {
    case Method::CentreDiameter: askValue("Średnica", "Ø", g_circleDiameter); break;
    case Method::CentreRadius: askValue("Promień", "R", g_circleRadius); break;
    case Method::CentrePoint: askPoint("Środek koła"); break;
    case Method::TwoPoints:
    case Method::ThreePoints: askPoint("Punkt (1)"); break;
    }
}

void CircleCommand::askCentre()
{
    // Koło o podanym promieniu jedzie za kursorem; każde kliknięcie stawia kolejne.
    preview([this](const gp_Pnt& c) { return shapeOf(camcore::circleEdge(c, m_radius)); });
    askPoint("Środek koła");
}

void CircleCommand::valueEntered(double value)
{
    if (m_method != Method::CentreDiameter && m_method != Method::CentreRadius)
        return;
    if (value <= 0) {
        host().showMessage("Koło: wartość musi być większa od zera");
        return;
    }
    if (m_method == Method::CentreDiameter) {
        g_circleDiameter = value;
        m_radius = value / 2;
    } else {
        g_circleRadius = value;
        m_radius = value;
    }
    askCentre();
}

void CircleCommand::build(const std::optional<TopoDS_Edge>& circle)
{
    if (!circle) {
        m_points.pop_back();
        notBuilt();
        return;
    }
    addGeometry({*circle});
    restart();
}

void CircleCommand::pointEntered(const gp_Pnt& p)
{
    switch (m_method) {
    case Method::CentreDiameter:
    case Method::CentreRadius:
        if (m_radius <= 0)
            return; // najpierw wartość
        addGeometry({*camcore::circleEdge(p, m_radius)});
        askCentre(); // ta sama średnica – kolejny środek
        return;
    case Method::CentrePoint:
        m_points.push_back(p);
        if (m_points.size() == 1) {
            preview([this](const gp_Pnt& c) {
                const gp_Pnt& o = m_points[0];
                return shapeOf(camcore::circleEdge(o, std::hypot(c.X() - o.X(), c.Y() - o.Y())));
            });
            askPoint("Punkt na okręgu");
        } else {
            const gp_Pnt& o = m_points[0];
            build(camcore::circleEdge(o, std::hypot(p.X() - o.X(), p.Y() - o.Y())));
        }
        return;
    case Method::TwoPoints: {
        m_points.push_back(p);
        auto circle = [](const gp_Pnt& a, const gp_Pnt& b) {
            const gp_Pnt c((a.X() + b.X()) / 2, (a.Y() + b.Y()) / 2, 0);
            return camcore::circleEdge(c, std::hypot(a.X() - b.X(), a.Y() - b.Y()) / 2);
        };
        if (m_points.size() == 1) {
            preview([this, circle](const gp_Pnt& c) { return shapeOf(circle(m_points[0], c)); });
            askPoint("Punkt (2)");
        } else {
            build(circle(m_points[0], m_points[1]));
        }
        return;
    }
    case Method::ThreePoints:
        m_points.push_back(p);
        if (m_points.size() == 1) {
            preview([this](const gp_Pnt& c) { return shapeOf(camcore::lineEdge(m_points[0], c)); });
            askPoint("Punkt (2)");
        } else if (m_points.size() == 2) {
            preview([this](const gp_Pnt& c) {
                return shapeOf(camcore::circleThreePoints(m_points[0], m_points[1], c));
            });
            askPoint("Punkt (3)");
        } else {
            build(camcore::circleThreePoints(m_points[0], m_points[1], m_points[2]));
        }
        return;
    }
}

// ---- Prostokąt ----

void RectangleCommand::restart()
{
    m_points.clear();
    preview(nullptr);
    askPoint("Pierwszy narożnik");
}

void RectangleCommand::pointEntered(const gp_Pnt& p)
{
    if (m_points.empty()) {
        m_points.push_back(p);
        preview([this](const gp_Pnt& c) { return compoundOf(camcore::rectangleEdges(m_points[0], c)); });
        askPoint("Drugi narożnik");
        return;
    }
    const std::vector<TopoDS_Edge> edges = camcore::rectangleEdges(m_points[0], p);
    if (edges.empty()) {
        notBuilt();
        return;
    }
    addGeometry(edges);
    const QString size = num(std::abs(p.X() - m_points[0].X())) + " × " + num(std::abs(p.Y() - m_points[0].Y()));
    restart();
    host().showMessage("Prostokąt: narysowano " + size + " – wskaż pierwszy narożnik kolejnego, PPM lub Esc kończy");
}
