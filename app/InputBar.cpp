#include "InputBar.h"

#include <QButtonGroup>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>

#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

// Prosty parser rekurencyjny: wyrażenie = składnik {(+|-) składnik},
// składnik = czynnik {(*|/) czynnik}, czynnik = [+|-] (liczba | "(" wyrażenie ")").
class Parser
{
public:
    explicit Parser(const QString& text) : m_s(text) {}

    std::optional<double> parse()
    {
        auto v = expression();
        skipSpaces();
        if (!v || m_pos != m_s.size())
            return std::nullopt;
        return v;
    }

private:
    void skipSpaces()
    {
        while (m_pos < m_s.size() && m_s[m_pos].isSpace())
            ++m_pos;
    }
    bool eat(QChar c)
    {
        skipSpaces();
        if (m_pos < m_s.size() && m_s[m_pos] == c) {
            ++m_pos;
            return true;
        }
        return false;
    }
    std::optional<double> expression()
    {
        auto v = term();
        while (v) {
            if (eat('+')) {
                auto r = term();
                if (!r) return std::nullopt;
                *v += *r;
            } else if (eat('-')) {
                auto r = term();
                if (!r) return std::nullopt;
                *v -= *r;
            } else {
                break;
            }
        }
        return v;
    }
    std::optional<double> term()
    {
        auto v = factor();
        while (v) {
            if (eat('*')) {
                auto r = factor();
                if (!r) return std::nullopt;
                *v *= *r;
            } else if (eat('/')) {
                auto r = factor();
                if (!r || *r == 0) return std::nullopt;
                *v /= *r;
            } else {
                break;
            }
        }
        return v;
    }
    std::optional<double> factor()
    {
        if (eat('-')) {
            auto v = factor();
            return v ? std::optional<double>(-*v) : std::nullopt;
        }
        if (eat('+'))
            return factor();
        if (eat('(')) {
            auto v = expression();
            if (!v || !eat(')'))
                return std::nullopt;
            return v;
        }
        skipSpaces();
        const int start = m_pos;
        while (m_pos < m_s.size() && (m_s[m_pos].isDigit() || m_s[m_pos] == '.' || m_s[m_pos] == ','))
            ++m_pos;
        if (m_pos == start)
            return std::nullopt;
        bool ok = false;
        const double v = m_s.mid(start, m_pos - start).replace(',', '.').toDouble(&ok);
        return ok ? std::optional<double>(v) : std::nullopt;
    }

    QString m_s;
    int m_pos = 0;
};

QString formatValue(double v)
{
    if (std::abs(v) < 0.0005)
        v = 0; // bez "-0.000"
    return QString::number(v, 'f', 3);
}

} // namespace

std::optional<double> evaluateExpression(const QString& text)
{
    if (text.trimmed().isEmpty())
        return std::nullopt;
    return Parser(text).parse();
}

InputBar::InputBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("inputBar");
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_command = new QLabel(this);
    m_command->setObjectName("inputCommand");
    m_prompt = new QLabel(this);
    m_prompt->setObjectName("inputPrompt");
    layout->addWidget(m_command);
    layout->addWidget(m_prompt);

    // Pola współrzędnych z przełącznikiem trybu.
    m_fieldsBox = new QWidget(this);
    auto* fieldsLayout = new QHBoxLayout(m_fieldsBox);
    fieldsLayout->setContentsMargins(6, 0, 0, 0);
    fieldsLayout->setSpacing(3);
    for (int i = 0; i < 3; ++i) {
        auto* label = new QLabel(m_fieldsBox);
        label->setMinimumWidth(24);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* field = new QLineEdit(m_fieldsBox);
        field->setFixedWidth(92);
        field->setAlignment(Qt::AlignRight);
        field->installEventFilter(this);
        // Wpisanie czegokolwiek przypina pole – przestaje iść za kursorem.
        connect(field, &QLineEdit::textEdited, this, [this, i](const QString& text) {
            setLocked(i, !text.trimmed().isEmpty());
        });
        connect(field, &QLineEdit::returnPressed, this, &InputBar::accept);
        fieldsLayout->addWidget(label);
        fieldsLayout->addWidget(field);
        m_labels.push_back(label);
        m_fields.push_back(field);
        m_locked.push_back(false);
    }
    fieldsLayout->addSpacing(6);
    auto* group = new QButtonGroup(this);
    const char* names[] = {"Abs", "Przyr", "Bieg"};
    const char* tips[] = {"Współrzędne bezwzględne (X, Y, Z)",
                          "Współrzędne przyrostowe od poprzedniego punktu (dX, dY, dZ)",
                          "Współrzędne biegunowe od poprzedniego punktu (długość, kąt)"};
    for (int i = 0; i < 3; ++i) {
        auto* b = new QToolButton(m_fieldsBox);
        b->setText(names[i]);
        b->setToolTip(tips[i]);
        b->setCheckable(true);
        b->setFocusPolicy(Qt::NoFocus); // Tab krąży tylko po polach
        b->setObjectName("inputMode");
        group->addButton(b, i);
        fieldsLayout->addWidget(b);
        m_modeButtons[i] = b;
    }
    m_modeButtons[0]->setChecked(true);
    connect(group, &QButtonGroup::idClicked, this, [this](int id) { setMode(static_cast<Mode>(id)); });
    layout->addWidget(m_fieldsBox);

    m_done = new QPushButton("Gotowe", this);
    m_done->setToolTip("Zatwierdź wybór (jak PPM lub Enter)");
    m_cancel = new QPushButton("Anuluj", this);
    m_cancel->setToolTip("Przerwij polecenie (Esc)");
    m_done->setFocusPolicy(Qt::NoFocus);
    m_cancel->setFocusPolicy(Qt::NoFocus);
    connect(m_done, &QPushButton::clicked, this, &InputBar::selectionDone);
    connect(m_cancel, &QPushButton::clicked, this, &InputBar::cancelled);
    layout->addWidget(m_done);
    layout->addWidget(m_cancel);

    updateLabels();
    showIdle();
}

void InputBar::showIdle()
{
    m_picking = false;
    m_command->setText("Polecenie:");
    m_prompt->setText("wybierz polecenie na wstążce");
    m_prompt->setEnabled(false);
    m_fieldsBox->hide();
    m_done->hide();
    m_cancel->hide();
}

void InputBar::startSelect(const QString& command, const QString& prompt)
{
    m_picking = false;
    m_command->setText(command + ":");
    setPrompt(prompt);
    m_fieldsBox->hide();
    m_done->show();
    m_cancel->show();
}

void InputBar::startPoint(const QString& command, const QString& prompt, std::optional<gp_Pnt> reference)
{
    m_picking = true;
    m_reference = reference.value_or(gp_Pnt(0, 0, 0));
    m_command->setText(command + ":");
    setPrompt(prompt);
    for (int i = 0; i < 3; ++i)
        setLocked(i, false);
    refreshTracked();
    m_fieldsBox->show();
    m_done->hide();
    m_cancel->show();
}

void InputBar::setPrompt(const QString& prompt)
{
    m_prompt->setEnabled(true);
    m_prompt->setText(prompt);
}

void InputBar::setMode(Mode mode)
{
    m_mode = mode;
    m_modeButtons[static_cast<int>(mode)]->setChecked(true);
    // Wpisane wartości znaczyły co innego w poprzednim trybie.
    for (int i = 0; i < 3; ++i)
        setLocked(i, false);
    updateLabels();
    refreshTracked();
}

void InputBar::updateLabels()
{
    static const char* abs[] = {"X", "Y", "Z"};
    static const char* inc[] = {"dX", "dY", "dZ"};
    static const char* pol[] = {"L", "Kąt", "dZ"};
    const char** names = m_mode == Mode::Absolute ? abs : m_mode == Mode::Incremental ? inc : pol;
    for (int i = 0; i < 3; ++i)
        m_labels[static_cast<size_t>(i)]->setText(QString::fromUtf8(names[i]));
}

void InputBar::setLocked(int field, bool locked)
{
    QLineEdit* f = m_fields[static_cast<size_t>(field)];
    m_locked[static_cast<size_t>(field)] = locked;
    f->setProperty("locked", locked);
    f->setProperty("error", false);
    f->style()->unpolish(f);
    f->style()->polish(f);
}

std::vector<double> InputBar::fieldsFor(const gp_Pnt& p) const
{
    const double dx = p.X() - m_reference.X();
    const double dy = p.Y() - m_reference.Y();
    const double dz = p.Z() - m_reference.Z();
    switch (m_mode) {
    case Mode::Absolute:
        return {p.X(), p.Y(), p.Z()};
    case Mode::Incremental:
        return {dx, dy, dz};
    case Mode::Polar:
        return {std::hypot(dx, dy), std::atan2(dy, dx) * 180.0 / kPi, dz};
    }
    return {0, 0, 0};
}

gp_Pnt InputBar::pointFrom(const std::vector<double>& v) const
{
    switch (m_mode) {
    case Mode::Absolute:
        return gp_Pnt(v[0], v[1], v[2]);
    case Mode::Incremental:
        return gp_Pnt(m_reference.X() + v[0], m_reference.Y() + v[1], m_reference.Z() + v[2]);
    case Mode::Polar: {
        const double a = v[1] * kPi / 180.0;
        return gp_Pnt(m_reference.X() + v[0] * std::cos(a), m_reference.Y() + v[0] * std::sin(a),
                      m_reference.Z() + v[2]);
    }
    }
    return m_reference;
}

void InputBar::refreshTracked()
{
    // Kursor leży na płaszczyźnie stołu; w trybach względnych Z zostaje na
    // wysokości poprzedniego punktu (dZ = 0).
    gp_Pnt c = m_cursor;
    c.SetZ(m_mode == Mode::Absolute ? 0.0 : m_reference.Z());
    const std::vector<double> values = fieldsFor(c);
    for (size_t i = 0; i < m_fields.size(); ++i)
        if (!m_locked[i])
            m_fields[i]->setText(formatValue(values[i]));
}

void InputBar::trackCursor(double x, double y)
{
    m_cursor = gp_Pnt(x, y, 0);
    if (m_picking)
        refreshTracked();
}

std::optional<gp_Pnt> InputBar::resolve(const gp_Pnt& cursor, bool markErrors)
{
    gp_Pnt c = cursor;
    c.SetZ(m_mode == Mode::Absolute ? 0.0 : m_reference.Z());
    std::vector<double> values = fieldsFor(c);
    bool ok = true;
    for (size_t i = 0; i < m_fields.size(); ++i) {
        if (!m_locked[i])
            continue;
        const auto v = evaluateExpression(m_fields[i]->text());
        if (v) {
            values[i] = *v;
        } else {
            ok = false;
            if (markErrors) {
                m_fields[i]->setProperty("error", true);
                m_fields[i]->style()->unpolish(m_fields[i]);
                m_fields[i]->style()->polish(m_fields[i]);
            }
        }
    }
    if (!ok)
        return std::nullopt;
    return pointFrom(values);
}

std::optional<gp_Pnt> InputBar::resolveClick(double x, double y)
{
    return resolve(gp_Pnt(x, y, 0), true);
}

void InputBar::accept()
{
    if (!m_picking)
        return;
    if (const auto p = resolve(m_cursor, true))
        emit pointEntered(p->X(), p->Y(), p->Z());
}

void InputBar::focusField(int field)
{
    QLineEdit* f = m_fields[static_cast<size_t>((field + 3) % 3)];
    f->setFocus(Qt::TabFocusReason);
    f->selectAll();
}

bool InputBar::handleViewKey(QKeyEvent* e)
{
    if (!m_picking)
        return false;
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        accept();
        return true;
    }
    if (e->key() == Qt::Key_Tab) {
        focusField(0);
        return true;
    }
    // Pierwszy znak liczby lub wyrażenia: wpisujemy go od razu do pola X (L).
    const QString t = e->text();
    if (t.size() == 1 && (t[0].isDigit() || QString("-+.,(").contains(t[0]))) {
        QLineEdit* f = m_fields[0];
        f->setFocus(Qt::OtherFocusReason);
        f->setText(t);
        setLocked(0, true);
        return true;
    }
    return false;
}

bool InputBar::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        for (int i = 0; i < 3; ++i) {
            if (watched != m_fields[static_cast<size_t>(i)])
                continue;
            auto* e = static_cast<QKeyEvent*>(event);
            if (e->key() == Qt::Key_Tab) {
                focusField(i + 1);
                return true;
            }
            if (e->key() == Qt::Key_Backtab) {
                focusField(i - 1);
                return true;
            }
            if (e->key() == Qt::Key_Escape) {
                emit cancelled();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
