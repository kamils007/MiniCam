#pragma once
#include "ArdImport.h"
#include "ModelImport.h"
#include <QByteArray>
class QWidget;

// Feature runs only in its isolated worker. Failure leaves current document intact.
camcore::ImportedModel readArdSolid(const QByteArray& source,
                                   const camcore::ArdDocument& document, QWidget* parent);
