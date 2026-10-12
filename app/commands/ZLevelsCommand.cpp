#include "ZLevelsCommand.h"

#include "GeometryEdit.h"
#include "InputBar.h"
#include "OccView.h"

#include <QDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include <algorithm>
#include <optional>

namespace {

QString num(double v) { return QString::number(v, 'g', 10); }

// Wynik okna: OK, Anuluj albo "..." przy jednym z pól (wskaż punkt w widoku).
enum DialogResult { Cancelled = 0, Accepted = 1, PickTop = 2, PickBottom = 3 };

// Okno "Definiuj poziomy Z" jak w Alphacam: Góra Z i Dół Z (pola tekstowe – można pisać
// wyrażenia, np. 18-3) z przyciskami "...", pod nimi Różnica (tylko do odczytu). Dół może
// być poniżej 0; Góra = Dół daje płaską geometrię. Teksty pól wracają w top / bottom.
int levelsDialog(QWidget* parent, QString& top, QString& bottom)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Definiuj poziomy Z");
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 18, 14, 14);
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(10);
    layout->addLayout(grid);

    auto field = [&](int row, const QString& label, const QString& text) {
        auto* l = new QLabel(label, &dialog);
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* edit = new QLineEdit(text, &dialog);
        edit->setFixedWidth(100);
        l->setBuddy(edit);
        grid->addWidget(l, row, 0);
        grid->addWidget(edit, row, 1);
        return edit;
    };
    auto pickButton = [&](int row, int result) {
        auto* b = new QPushButton("...", &dialog);
        b->setFixedWidth(36);
        b->setAutoDefault(false);
        b->setToolTip("Wskaż punkt w widoku – Z z punktu (uchwyt albo pasek wprowadzania)");
        QObject::connect(b, &QPushButton::clicked, &dialog, [&dialog, result] { dialog.done(result); });
        grid->addWidget(b, row, 2);
    };
    QLineEdit* topEdit = field(0, "Góra Z", top);
    pickButton(0, PickTop);
    QLineEdit* bottomEdit = field(1, "Dół Z", bottom);
    pickButton(1, PickBottom);
    QLineEdit* difference = field(2, "Różnica", QString());
    difference->setReadOnly(true);
    difference->setFixedWidth(96);
    difference->setFocusPolicy(Qt::NoFocus);
    difference->setStyleSheet("QLineEdit { background: #f0f0f0; }");

    layout->addSpacing(14);
    auto* buttons = new QHBoxLayout;
    auto* ok = new QPushButton("OK", &dialog);
    auto* cancel = new QPushButton("Anuluj", &dialog);
    for (QPushButton* b : {ok, cancel})
        b->setFixedWidth(110);
    ok->setDefault(true);
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addSpacing(14);
    buttons->addWidget(cancel);
    buttons->addStretch();
    layout->addLayout(buttons);
    QObject::connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);

    // Różnica na bieżąco; OK tylko przy poprawnych liczbach i Górze nie niżej niż Dół.
    const auto update = [&] {
        const std::optional<double> t = evaluateExpression(topEdit->text());
        const std::optional<double> b = evaluateExpression(bottomEdit->text());
        const bool valid = t && b && *t >= *b;
        difference->setText(t && b ? QString::number(*t - *b, 'f', 6) : QString());
        difference->setStyleSheet(t && b && !valid ? "QLineEdit { background: #f0f0f0; color: #c62828; }"
                                                   : "QLineEdit { background: #f0f0f0; }");
        ok->setEnabled(valid);
    };
    update();
    QObject::connect(topEdit, &QLineEdit::textChanged, &dialog, update);
    QObject::connect(bottomEdit, &QLineEdit::textChanged, &dialog, update);
    topEdit->setFocus();
    topEdit->selectAll();

    const int result = dialog.exec();
    top = topEdit->text();
    bottom = bottomEdit->text();
    return result;
}

} // namespace

bool ZLevelsCommand::start()
{
    if (host().editState().geometries.empty()) {
        host().showMessage(name() + ": nie ma geometrii – narysuj je albo rozpoznaj cechy");
        return false;
    }
    host().inputBar()->startSelect(name() + ":", "Wskaż");
    host().view()->clearSelection();
    host().view()->setInteraction(OccView::Interaction::Select);
    host().view()->setFocus();
    host().showMessage("Poziomy Z: wskaż geometrie, PPM zatwierdza, Esc anuluje");
    return true;
}

void ZLevelsCommand::selectionChanged(int count)
{
    if (m_step != Step::Selecting)
        return;
    host().inputBar()->setPrompt(QString("Wskaż (wybrano %1)").arg(count));
    host().showMessage(QString("Poziomy Z: wybrano %1 – klikaj kolejne geometrie, PPM zatwierdza, Esc anuluje")
                           .arg(count));
}

void ZLevelsCommand::selectionConfirmed()
{
    if (m_step != Step::Selecting)
        return;
    m_geometries = host().view()->selectedGeometries();
    if (m_geometries.empty()) {
        host().showMessage("Poziomy Z: nie wybrano geometrii – kliknij geometrię (LPM), potem PPM");
        return;
    }
    std::sort(m_geometries.begin(), m_geometries.end());
    // W oknie na start aktualne poziomy (pierwszej wybranej) geometrii.
    const camcore::Contour& first = host().editState().geometries[static_cast<size_t>(m_geometries.front())].contour;
    m_top = num(first.zTop);
    m_bottom = num(first.zBottom);
    showDialog();
}

void ZLevelsCommand::showDialog()
{
    m_step = Step::Dialog;
    const int result = levelsDialog(host().view(), m_top, m_bottom);
    if (result == Accepted) {
        apply(*evaluateExpression(m_bottom), *evaluateExpression(m_top));
    } else if (result == PickTop || result == PickBottom) {
        // "..." – Z z punktu: uchwyt (prawdziwe Z) albo punkt wpisany w pasek wprowadzania.
        m_step = result == PickTop ? Step::PickTop : Step::PickBottom;
        const QString field = result == PickTop ? "Góra Z" : "Dół Z";
        host().view()->setInteraction(OccView::Interaction::PickPoint);
        host().view()->setFocus();
        host().inputBar()->startPoint(name() + ":", "Punkt – " + field);
        host().showMessage(QString("Poziomy Z: wskaż punkt dla pola %1 (uchwyt daje jego Z), Esc anuluje")
                               .arg(field));
    } else {
        finish(name() + ": anulowane");
    }
}

void ZLevelsCommand::pointEntered(const gp_Pnt& p)
{
    if (m_step == Step::PickTop)
        m_top = num(p.Z());
    else if (m_step == Step::PickBottom)
        m_bottom = num(p.Z());
    else
        return;
    showDialog();
}

void ZLevelsCommand::cancel()
{
    finish(name() + ": anulowane");
}

void ZLevelsCommand::apply(double bottom, double top)
{
    const EditState before = host().editState();
    EditState after = before;
    for (int i : m_geometries) {
        camcore::Contour& c = after.geometries[static_cast<size_t>(i)].contour;
        c = camcore::withZLevels(c, bottom, top);
    }
    host().commitEdit(name(), before, after);
    finish(QString("Poziomy Z: %1 geometrii od Z %2 do Z %3, wys. %4 (Ctrl+Z cofa)")
               .arg(m_geometries.size())
               .arg(num(bottom), num(top), num(top - bottom)));
}
