#include "DeleteCommand.h"

#include "InputBar.h"
#include "OccView.h"

#include <algorithm>
#include <functional>

bool DeleteCommand::start()
{
    const EditState state = host().editState();
    if (state.shown.shape.IsNull() && state.geometries.empty()) {
        host().showMessage("Usuń: nie ma nic do usunięcia");
        return false;
    }
    host().inputBar()->startSelect("Usuń:", "Wskaż");
    host().view()->clearSelection();
    host().view()->setInteraction(OccView::Interaction::Select);
    host().view()->setFocus();
    host().showMessage("Usuń: wskaż bryłę lub geometrie, PPM usuwa, Esc anuluje");
    return true;
}

void DeleteCommand::selectionChanged(int count)
{
    host().inputBar()->setPrompt(QString("Wskaż (wybrano %1)").arg(count));
    host().showMessage(QString("Usuń: wybrano %1 – klikaj kolejne elementy, PPM usuwa, Esc anuluje").arg(count));
}

void DeleteCommand::selectionConfirmed()
{
    std::vector<int> geometries = host().view()->selectedGeometries();
    const bool model = host().view()->isModelSelected();
    if (geometries.empty() && !model) {
        host().showMessage("Usuń: nic nie wybrano – kliknij element (LPM), potem PPM");
        return;
    }
    const EditState before = host().editState();
    EditState after = before;
    if (model)
        after.shown = {}; // pusta bryła = brak bryły
    // Od końca, żeby usuwanie nie przesuwało numerów jeszcze nieusuniętych geometrii.
    std::sort(geometries.begin(), geometries.end(), std::greater<int>());
    for (int i : geometries)
        after.geometries.erase(after.geometries.begin() + i);
    host().commitEdit("Usuń", before, after);
    const int count = static_cast<int>(geometries.size()) + (model ? 1 : 0);
    finish(QString("Usunięto elementów: %1 (Ctrl+Z cofa)").arg(count));
}

void DeleteCommand::cancel()
{
    finish("Usuwanie anulowane");
}
