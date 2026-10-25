#pragma once
#include "ArdImport.h"
#include "ModelImport.h"
#include <QByteArray>
class QWidget;

// Whole-body conversion runs in isolated SolidLink/CLS workers, followed by the
// normal STEP reader. Failure leaves the current document intact.
camcore::ImportedModel readArdSolid(const QByteArray& source,
                                   const camcore::ArdDocument& document, QWidget* parent);
