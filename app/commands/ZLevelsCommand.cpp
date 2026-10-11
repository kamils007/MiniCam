#include "ZLevelsCommand.h"

#include "GeometryEdit.h"
#include "OccView.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>

namespace {

QString num(double v) { return QString::number(v, 'g', 10); }

QDoubleSpinBox* levelBox(double value, QWidget* parent)
{
    auto* box = new QDoubleSpinBox(parent);
    box->setRange(-100000, 100000);
    box->setDecimals(3);
    box->setSuffix(" mm");
    box->setValue(value);
    box->setMinimumWidth(120);
    return box;
}

// Okno "Poziomy Z": górny i dolny poziom (dolny może być poniżej 0), pod nimi różnica
// (wysokość ścian) liczona na bieżąco. Za pierwszym razem 0 i 0, potem ostatnie wartości.
// Górny = dolny daje płaską geometrię (zerowa wysokość). false = Anuluj.
bool askLevels(QWidget* parent, double& bottom, double& top)
{
    QSettings settings;
    QDialog dialog(parent);
    dialog.setWindowTitle("Poziomy Z");
    auto* form = new QFormLayout(&dialog);
    QDoubleSpinBox* topBox = levelBox(settings.value("ZLevels/top", 0.0).toDouble(), &dialog);
    QDoubleSpinBox* bottomBox = levelBox(settings.value("ZLevels/bottom", 0.0).toDouble(), &dialog);
    form->addRow("Górny poziom Z:", topBox);
    form->addRow("Dolny poziom Z:", bottomBox);
    auto* difference = new QLabel(&dialog);
    form->addRow("Różnica:", difference);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Cancel)->setText("Anuluj");
    form->addRow(buttons);
    const auto update = [&] {
        const double d = topBox->value() - bottomBox->value();
        const bool ok = d >= 0;
        difference->setText(ok ? num(d) + " mm" : "górny poziom musi być nad dolnym");
        difference->setStyleSheet(ok ? "font-weight: bold;" : "color: #c62828;");
        buttons->button(QDialogButtonBox::Ok)->setEnabled(ok);
    };
    update();
    QObject::connect(topBox, &QDoubleSpinBox::valueChanged, &dialog, update);
    QObject::connect(bottomBox, &QDoubleSpinBox::valueChanged, &dialog, update);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    topBox->setFocus();
    topBox->selectAll();
    if (dialog.exec() != QDialog::Accepted)
        return false;
    top = topBox->value();
    bottom = bottomBox->value();
    settings.setValue("ZLevels/top", top);
    settings.setValue("ZLevels/bottom", bottom);
    return true;
}

} // namespace

QString ZLevelsCommand::hint() const
{
    return "wskaż geometrie, którym ustawić poziomy Z";
}

QString ZLevelsCommand::apply(const std::vector<int>& geometries)
{
    // Jak w Alphacam: najpierw wybór geometrii, po PPM okno z poziomami, OK kończy polecenie.
    double bottom = 0, top = 0;
    if (!askLevels(host().view(), bottom, top))
        return name() + ": anulowane";
    const EditState before = host().editState();
    EditState after = before;
    for (int i : geometries) {
        camcore::Contour& c = after.geometries[static_cast<size_t>(i)].contour;
        c = camcore::withZLevels(c, bottom, top);
    }
    host().commitEdit(name(), before, after);
    return QString("Poziomy Z: %1 geometrii od Z %2 do Z %3, wys. %4 (Ctrl+Z cofa)")
        .arg(geometries.size())
        .arg(num(bottom), num(top), num(top - bottom));
}
