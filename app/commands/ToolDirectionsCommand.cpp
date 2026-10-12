#include "ToolDirectionsCommand.h"

QString ToolDirectionsCommand::applyTo(CommandHost& host, const std::vector<int>& indices,
                                       const camcore::DirectionChange& change)
{
    const EditState before = host.editState();
    if (before.geometries.empty())
        return "Kierunki narzędzia: nie ma geometrii – narysuj je albo rozpoznaj cechy";
    EditState after = before;
    camcore::applyDirectionChange(after.geometries, indices, change, before.shown.shape);
    host.commitEdit("Kierunki narzędzia", before, after);
    const size_t count = indices.empty() ? after.geometries.size() : indices.size();
    return QString("Kierunki narzędzia: zmieniono %1 geometrii (Ctrl+Z cofa)").arg(count);
}
