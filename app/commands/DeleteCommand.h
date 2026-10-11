#pragma once

#include "Command.h"

// Usuń: wybór elementów (bryła i/lub geometrie) → PPM, Enter albo OK usuwa je.
// Usunięcie można cofnąć (Cofnij).
class DeleteCommand : public Command
{
    Q_OBJECT

public:
    using Command::Command;

    QString name() const override { return "Usuń"; }
    bool start() override;
    void selectionChanged(int count) override;
    void selectionConfirmed() override;
    void cancel() override;
    bool isSelecting() const override { return true; }
};
