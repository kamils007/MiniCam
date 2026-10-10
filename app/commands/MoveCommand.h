#pragma once

#include "Command.h"

// Przesuń: wybór elementów (bryła i/lub geometrie) → punkt bazowy → punkt docelowy.
// Bryła przesuwa się także w Z, geometrie (kontury 2D) tylko w X i Y.
class MoveCommand : public Command
{
    Q_OBJECT

public:
    using Command::Command;

    QString name() const override { return "Przesuń"; }
    bool start() override;
    void selectionChanged(int count) override;
    void selectionConfirmed() override;
    void pointEntered(const gp_Pnt& p) override;
    void cancel() override;
    bool isSelecting() const override { return m_step == Step::Selecting; }

private:
    void apply(const gp_Vec& offset);

    enum class Step { Selecting, PickBase, PickTarget };
    Step m_step = Step::Selecting;
    std::vector<int> m_geometries; // wybrane geometrie
    bool m_model = false;          // czy wybrano bryłę
    gp_Pnt m_base;
};
