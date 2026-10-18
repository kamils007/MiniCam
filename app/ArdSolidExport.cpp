#include "ArdSolidExport.h"
#include "AlphacamExport.h"

std::string createArdSolidRecord(const camcore::ImportedModel& model, QWidget* parent)
{
    return alphacam::createSolidRecord(model, parent);
}
