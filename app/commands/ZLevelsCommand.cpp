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

// Okno "Poziomy Z": dolny poziom (może być poniżej 0) i górny. Ostatnie wartości
// zapamiętuje. false = Anuluj.
bool askLevels(QWidget* parent, double& bottom, double& top)
{
    QSettings settings;
    QDialog dialog(parent);
    dialog.setWindowTitle("Poziomy Z");
    auto* form = new QFormLayout(&dialog);
    QDoubleSpinBox* topBox = levelBox(settings.value("ZLevels/top", 0.0).toDouble(), &dialog);
    QDoubleSpinBox* bottomBox = levelBox(settings.value("ZLevels/bottom", -18.0).toDouble(), &dialog);
    form->addRow("Dolny poziom Z:", bottomBox);
    form->addRow("Górny poziom Z:", topBox);
    auto* error = new QLabel(&dialog);
    error->setStyleSheet("color: #c62828;");
    form->addRow(error);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Cancel)->setText("Anuluj");
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (topBox->value() <= bottomBox->value()) {
            error->setText("Górny poziom musi być wyżej niż dolny");
            return;
        }
        dialog.accept();
    });
    bottomBox->setFocus();
    bottomBox->selectAll();
    if (dialog.exec() != QDialog::Accepted)
        return false;
    top = topBox->value();
    bottom = bottomBox->value();
    settings.setValue("ZLevels/top", top);
    settings.setValue("ZLevels/bottom", bottom);
    return true;
}

} // namespace

bool ZLevelsCommand::start()
{
    if (host().editState().geometries.empty()) {
        host().showMessage(name() + ": nie ma geometrii – narysuj je albo rozpoznaj cechy");
        return false;
    }
    if (!askLevels(host().view(), m_bottom, m_top)) {
        host().showMessage(name() + ": anulowane");
        return false;
    }
    return GeometrySelectCommand::start();
}

QString ZLevelsCommand::hint() const
{
    return QString("wskaż geometrie (Z od %1 do %2)").arg(num(m_bottom), num(m_top));
}

QString ZLevelsCommand::apply(const std::vector<int>& geometries)
{
    const EditState before = host().editState();
    EditState after = before;
    for (int i : geometries) {
        camcore::Contour& c = after.geometries[static_cast<size_t>(i)].contour;
        c = camcore::withZLevels(c, m_bottom, m_top);
    }
    host().commitEdit(name(), before, after);
    return QString("Poziomy Z: %1 geometrii od Z %2 do Z %3, wys. %4 (Ctrl+Z cofa)")
        .arg(geometries.size())
        .arg(num(m_bottom), num(m_top), num(m_top - m_bottom));
}
