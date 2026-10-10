#include "MoveCommand.h"

#include "InputBar.h"
#include "ModelAlign.h"
#include "OccView.h"
#include "PartContours.h"

#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

bool MoveCommand::start()
{
    if (host().editState().shown.shape.IsNull()) {
        host().showMessage("Najpierw otwórz model (Ctrl+O)");
        return false;
    }
    host().inputBar()->startSelect("Przesuń:", "Wskaż");
    host().view()->clearSelection();
    host().view()->setInteraction(OccView::Interaction::Select);
    host().view()->setFocus();
    host().showMessage("Przesuń: wskaż bryłę lub geometrie, PPM zatwierdza, Esc anuluje");
    return true;
}

void MoveCommand::selectionChanged(int count)
{
    if (m_step != Step::Selecting)
        return;
    host().inputBar()->setPrompt(QString("Wskaż (wybrano %1)").arg(count));
    host().showMessage(QString("Przesuń: wybrano %1 – klikaj kolejne elementy, PPM zatwierdza, Esc anuluje")
                           .arg(count));
}

void MoveCommand::selectionConfirmed()
{
    if (m_step != Step::Selecting)
        return;
    m_geometries = host().view()->selectedGeometries();
    m_model = host().view()->isModelSelected();
    if (m_geometries.empty() && !m_model) {
        host().showMessage("Przesuń: nic nie wybrano – kliknij element (LPM), potem PPM");
        return;
    }
    // Wybór zostaje podświetlony; teraz wskazujemy, o ile przesunąć.
    m_step = Step::PickBase;
    host().view()->setInteraction(OccView::Interaction::PickPoint);
    host().inputBar()->startPoint("Przesuń:", "Punkt bazowy");
    host().showMessage("Przesuń: kliknij punkt bazowy albo wpisz go w pasku wprowadzania i Enter");
}

void MoveCommand::pointEntered(const gp_Pnt& p)
{
    if (m_step == Step::PickBase) {
        m_base = p;
        m_step = Step::PickTarget;
        // Uchwyt służył punktowi bazowemu – zwalniamy go (wraca krzyż), a kopia
        // przesuwanych elementów jedzie za kursorem aż do kliknięcia celu.
        host().releaseSnap();
        host().view()->startMovePreview(m_base, m_model, m_geometries);
        host().inputBar()->startPoint("Przesuń:", "Punkt docelowy");
        host().showMessage(QString("Przesuń: punkt bazowy X %1 Y %2 Z %3 – wskaż punkt docelowy")
                               .arg(p.X(), 0, 'f', 2)
                               .arg(p.Y(), 0, 'f', 2)
                               .arg(p.Z(), 0, 'f', 2));
    } else if (m_step == Step::PickTarget) {
        apply(gp_Vec(m_base, p));
    }
}

void MoveCommand::apply(const gp_Vec& offset)
{
    const EditState before = host().editState();
    EditState after = before;
    if (m_model) {
        gp_Trsf t;
        t.SetTranslation(offset);
        after.shown = camcore::transformed(after.shown, t);
    }
    // Bryła jedzie także w Z; geometrie (kontury 2D) tylko w X i Y.
    const gp_Vec flat(offset.X(), offset.Y(), 0.0);
    for (int i : m_geometries) {
        camcore::Geometry& g = after.geometries[static_cast<size_t>(i)];
        g.contour = camcore::translated(g.contour, flat);
    }
    host().commitEdit("Przesuń", before, after);
    finish(QString("Przesunięto o dX %1  dY %2  dZ %3 mm")
               .arg(offset.X(), 0, 'f', 2)
               .arg(offset.Y(), 0, 'f', 2)
               .arg(offset.Z(), 0, 'f', 2));
}

void MoveCommand::cancel()
{
    finish("Przesuwanie anulowane");
}
