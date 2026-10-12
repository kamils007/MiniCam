#include "TrimCommand.h"

#include "GeometryEdit.h"
#include "InputBar.h"
#include "OccView.h"

#include <algorithm>

bool TrimCommand::start()
{
    if (host().editState().geometries.size() < 2) {
        host().showMessage("Utnij: potrzebne są co najmniej dwie geometrie – tnąca i ucinana");
        return false;
    }
    host().inputBar()->startSelect("Utnij:", "Geometrie tnące");
    host().view()->clearSelection();
    host().view()->setInteraction(OccView::Interaction::Select);
    host().view()->setFocus();
    host().showMessage("Utnij: wskaż geometrie tnące, PPM zatwierdza, Esc anuluje");
    return true;
}

void TrimCommand::selectionChanged(int count)
{
    if (m_step == Step::Cutters) {
        host().inputBar()->setPrompt(QString("Geometrie tnące (wybrano %1)").arg(count));
        host().showMessage(QString("Utnij: wybrano %1 geometrii tnących – PPM zatwierdza, Esc anuluje").arg(count));
    } else {
        host().inputBar()->setPrompt(QString("Geometrie do ucięcia (wybrano %1)").arg(count));
        host().showMessage(QString("Utnij: wybrano %1 – klikaj kawałki do usunięcia, PPM ucina, Esc anuluje")
                               .arg(count));
    }
}

void TrimCommand::selectionConfirmed()
{
    OccView* view = host().view();
    std::vector<int> selected = view->selectedGeometries();
    if (selected.empty()) {
        host().showMessage(m_step == Step::Cutters
                               ? "Utnij: nie wybrano geometrii tnących – kliknij geometrię (LPM), potem PPM"
                               : "Utnij: kliknij kawałek geometrii, który ma zniknąć (LPM), potem PPM");
        return;
    }
    if (m_step == Step::Cutters) {
        // Geometrie tnące zostają podświetlone na pomarańczowo; teraz wybór ucinanych.
        m_cutters = selected;
        m_step = Step::Targets;
        view->clearSelection();
        view->highlightGeometry(m_cutters);
        host().inputBar()->setPrompt("Geometrie do ucięcia");
        host().showMessage("Utnij: kliknij geometrie w kawałku, który ma zniknąć, PPM ucina, Esc anuluje");
        return;
    }

    const EditState before = host().editState();
    std::vector<camcore::Contour> cutters;
    std::sort(selected.begin(), selected.end());
    EditState after = before;
    after.geometries.clear();
    int trimmed = 0, missed = 0;
    for (size_t i = 0; i < before.geometries.size(); ++i) {
        const camcore::Geometry& g = before.geometries[i];
        if (!std::binary_search(selected.begin(), selected.end(), static_cast<int>(i))) {
            after.geometries.push_back(g);
            continue;
        }
        // Tnie każda geometria tnąca poza samą ucinaną.
        cutters.clear();
        for (int c : m_cutters)
            if (c != static_cast<int>(i))
                cutters.push_back(before.geometries[static_cast<size_t>(c)].contour);
        const auto pieces = camcore::trimContour(g.contour, cutters, view->selectionPoint(static_cast<int>(i)));
        if (!pieces) {
            ++missed;
            after.geometries.push_back(g);
            continue;
        }
        ++trimmed;
        for (const camcore::Contour& c : *pieces) {
            camcore::Geometry piece = g; // warstwa i widoczność zostają
            piece.contour = c;
            after.geometries.push_back(piece);
        }
    }
    if (trimmed == 0) {
        view->clearSelection();
        host().showMessage("Utnij: wybrane geometrie nie przecinają się z tnącymi – wskaż inne, PPM ucina, Esc kończy");
        return;
    }
    host().commitEdit("Utnij", before, after);
    QString message = QString("Ucięto geometrii: %1 (Ctrl+Z cofa)").arg(trimmed);
    if (missed > 0)
        message += QString(", bez przecięcia: %1").arg(missed);
    done(message);
}

void TrimCommand::cancel()
{
    done("Utnij: anulowane");
}

void TrimCommand::done(const QString& message)
{
    host().view()->highlightGeometry({});
    finish(message);
}
