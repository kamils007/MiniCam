#pragma once
#include <QByteArray>
#include <QString>
#include <TopoDS_Shape.hxx>

namespace ardimport {
// Exact supported analytic subset. Throws before returning an incomplete solid.
// Does not apply alignment, call vendor code, or write an intermediate STEP.
TopoDS_Shape shapeFromSnapshot(const QByteArray& snapshot);
// Validate source/payload/geometry correlation before constructing the GUI model.
TopoDS_Shape shapeFromReadResult(const QByteArray& report, const QByteArray& snapshot,
                               const QString& sourceSha, const QString& payloadSha);
}
