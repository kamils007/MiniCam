#pragma once
#include "ModelImport.h"
#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace ardimport {

// Accept only this conversion's STEP after source/output correlation and
// normal native/CLS completion. Import the complete STEP through ModelImport.
camcore::ImportedModel modelFromWholeBodyResult(const QByteArray& report,
    const QString& stepPath, const QString& sourceArdSha256, const QString& payloadSha256);

// Checks the result in OCCT. Does not certify fidelity against an unavailable
// original Parasolid model. Useful for the editor and the offline verifier.
QJsonObject inspectWholeBody(const camcore::ImportedModel& model);

}
