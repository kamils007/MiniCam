#include "JoinCommands.h"
#include "ToolSides.h"

#include "GeometryEdit.h"
#include "InputBar.h"
#include "OccView.h"

#include <algorithm>

bool GeometrySelectCommand::start()
{
    if (host().editState().geometries.empty()) {
        host().showMessage(name() + ": nie ma geometrii – narysuj je albo rozpoznaj cechy");
        return false;
    }
    host().inputBar()->startSelect(name() + ":", "Wskaż");
    host().view()->clearSelection();
    host().view()->setInteraction(OccView::Interaction::Select);
    host().view()->setFocus();
    host().showMessage(QString("%1: %2, PPM zatwierdza, Esc anuluje").arg(name(), hint()));
    return true;
}

void GeometrySelectCommand::selectionChanged(int count)
{
    host().inputBar()->setPrompt(QString("Wskaż (wybrano %1)").arg(count));
    host().showMessage(QString("%1: wybrano %2 – klikaj kolejne geometrie, PPM zatwierdza, Esc anuluje")
                           .arg(name())
                           .arg(count));
}

void GeometrySelectCommand::selectionConfirmed()
{
    std::vector<int> geometries = host().view()->selectedGeometries();
    if (geometries.empty()) {
        host().showMessage(name() + ": nie wybrano geometrii – kliknij geometrię (LPM), potem PPM");
        return;
    }
    std::sort(geometries.begin(), geometries.end());
    finish(apply(geometries));
}

QString ExplodeCommand::apply(const std::vector<int>& geometries)
{
    const EditState before = host().editState();
    EditState after = before;
    after.geometries.clear();
    int exploded = 0, pieces = 0;
    for (size_t i = 0; i < before.geometries.size(); ++i) {
        const camcore::Geometry& g = before.geometries[i];
        if (!std::binary_search(geometries.begin(), geometries.end(), static_cast<int>(i))) {
            after.geometries.push_back(g);
            continue;
        }
        // Elementy zostają w miejscu geometrii, z jej warstwą i widocznością.
        const std::vector<camcore::Contour> parts = camcore::explodeContour(g.contour);
        if (parts.size() > 1) {
            ++exploded;
            pieces += static_cast<int>(parts.size());
        }
        for (const camcore::Contour& c : parts) {
            camcore::Geometry piece = g;
            piece.contour = c;
            after.geometries.push_back(piece);
        }
    }
    if (exploded == 0)
        return "Rozdziel: wybrane geometrie mają po jednym elemencie – nie ma czego rozdzielać";
    host().commitEdit("Rozdziel", before, after);
    return QString("Rozdzielono geometrie: %1 → %2 elementów (Ctrl+Z cofa)").arg(exploded).arg(pieces);
}

QString JoinCommand::apply(const std::vector<int>& geometries)
{
    if (geometries.size() < 2)
        return "Połącz: wybierz co najmniej dwie geometrie";
    const EditState before = host().editState();
    std::vector<camcore::Contour> contours;
    bool sameLayer = true;
    const std::string& layer = before.geometries[static_cast<size_t>(geometries.front())].layer;
    for (int i : geometries) {
        const camcore::Geometry& g = before.geometries[static_cast<size_t>(i)];
        contours.push_back(g.contour);
        sameLayer = sameLayer && g.layer == layer;
    }
    const std::vector<camcore::Contour> joined = camcore::joinContours(contours);
    if (joined.empty() || joined.size() == geometries.size())
        return "Połącz: wybrane geometrie nie stykają się końcami – nic nie połączono";

    // Połączone geometrie stają na miejscu pierwszej wybranej; jak w Alphacam:
    // wspólna warstwa zostaje, z różnych warstw – geometria trafia do APS.
    EditState after = before;
    after.geometries.clear();
    std::vector<size_t> joinedIndices;
    for (size_t i = 0; i < before.geometries.size(); ++i) {
        if (static_cast<int>(i) == geometries.front()) {
            for (const camcore::Contour& c : joined) {
                camcore::Geometry g;
                g.contour = c;
                g.layer = sameLayer ? layer : camcore::kApsLayer;
                after.geometries.push_back(g);
                joinedIndices.push_back(after.geometries.size() - 1);
            }
        } else if (!std::binary_search(geometries.begin(), geometries.end(), static_cast<int>(i))) {
            after.geometries.push_back(before.geometries[i]);
        }
    }
    for (size_t i : joinedIndices)
        camcore::assignToolSide(after.geometries, i); // domyślna strona frezowania
    host().commitEdit("Połącz", before, after);
    if (joined.size() == 1)
        return QString("Połączono %1 geometrii w jedną (Ctrl+Z cofa)").arg(geometries.size());
    return QString("Połączono %1 geometrii w %2 – części się nie stykają (Ctrl+Z cofa)")
        .arg(geometries.size())
        .arg(joined.size());
}
