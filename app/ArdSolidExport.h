#pragma once
#include <string>
class QWidget;
namespace camcore { struct ImportedModel; }

// Returns the unmodified ObjectDoWrite2 record. Throws on any failure;
// callers must not silently save a document without its solid body.
std::string createArdSolidRecord(const camcore::ImportedModel& model, QWidget* parent);
