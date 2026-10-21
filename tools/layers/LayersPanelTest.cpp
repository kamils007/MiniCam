#include "LayersPanel.h"
#include <QApplication>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <iostream>
#include <stdexcept>

void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
QTreeWidgetItem* childNamed(QTreeWidgetItem* parent, const QString& name)
{
    for (int i=0; i<parent->childCount(); ++i)
        if (parent->child(i)->data(0, Qt::UserRole+1).toString()==name) return parent->child(i);
    throw std::runtime_error("missing user layer");
}
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    try {
        LayersPanel panel;
        auto* tree=panel.findChild<QTreeWidget*>();
        check(tree && tree->topLevelItemCount()==10, "unexpected fixed groups");
        auto* system=tree->topLevelItem(1);
        auto* user=tree->topLevelItem(9);
        panel.setUserLayers({{"Konstrukcje", Qt::blue}, {"FAZA", Qt::red}});
        panel.setGeometries({{"source system", "Konstrukcje", true, true},
                             {"source user", "Konstrukcje", true, false},
                             {"APS", "Geometrie APS", true, true},
                             {"faza", "FAZA", true, false}});
        check(user->childCount()==2, "system layers duplicated under user group");
        auto* sameName=childNamed(user, "Konstrukcje");
        check(system->childCount()==1 && sameName->childCount()==1,
              "same-named user geometry merged into fixed group");
        check(system->child(0)->data(0, Qt::UserRole).toInt()==0
              && sameName->child(0)->data(0, Qt::UserRole).toInt()==1,
              "wrong geometry layer assignment");
        std::vector<int> changed;
        QObject::connect(&panel, &LayersPanel::geometryVisibilityChanged,
                         [&](int index, bool visible) { if (!visible) changed.push_back(index); });
        system->setCheckState(0, Qt::Unchecked);
        check(changed==std::vector<int>{0}, "system visibility toggles user geometry");
        changed.clear();sameName->setCheckState(0, Qt::Unchecked);
        check(changed==std::vector<int>{1}, "user visibility toggles system geometry");
        panel.setUserLayers({});panel.setGeometries({});
        check(system->childCount()==0 && user->childCount()==0
              && system->text(0)=="Konstrukcje", "previous import retained after reload");
        std::cout << "PASS: reserved groups, user identity, visibility and reload\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
