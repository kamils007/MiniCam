#pragma once
#include <string>
class QWidget;
namespace camcore { struct ImportedModel; }

namespace alphacam {
// Optional module. No vendor DLL is ever loaded into the editor process.
// Throws on failure; the caller must not replace a solid document with 2D only.
std::string createSolidRecord(const camcore::ImportedModel& model, QWidget* parent);
bool configure(QWidget* parent);
}
