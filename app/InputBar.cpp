#include "InputBar.h"

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

    // Pola X i Y, każde z przyciskiem "F1=?" (pomiń nieznaną wartość).
    m_fieldsBox = new QWidget(this);
    auto* fieldsLayout = new QHBoxLayout(m_fieldsBox);
    fieldsLayout->setContentsMargins(8, 0, 0, 0);
    fieldsLayout->setSpacing(4);
    const char* names[] = {"X", "Y"};
    for (int i = 0; i < 2; ++i) {
        auto* label = new QLabel(names[i], m_fieldsBox);
        label->setContentsMargins(6, 0, 0, 0);
        auto* field = new QLineEdit(m_fieldsBox);
        field->setFixedWidth(92);
        field->installEventFilter(this);
        connect(field, &QLineEdit::textEdited, this, [this, i] {
            // Wpisanie wartości cofa pominięcie pola.
            if (m_bypassed[static_cast<size_t>(i)]) {
                m_bypassed[static_cast<size_t>(i)] = false;
                showPrompt();
            }
            setError(i, false);
        });
        connect(field, &QLineEdit::returnPressed, this, &InputBar::onOk);
        auto* f1 = new QToolButton(m_fieldsBox);
        f1->setText("F1=?");
        f1->setObjectName("inputF1");
        f1->setToolTip("Nie znam tej wartości – weź ją z kliknięcia w widoku (F1)");
        f1->setFocusPolicy(Qt::NoFocus);
        connect(f1, &QToolButton::clicked, this, [this, i] { bypass(i); });
        fieldsLayout->addWidget(label);
        fieldsLayout->addWidget(field);
        fieldsLayout->addWidget(f1);
        m_fields.push_back(field);
        m_bypassed.push_back(false);
    }
    layout->addWidget(m_fieldsBox);

    // Podpowiedzi przy wyborze elementów (jak w Alphacam) – na razie bez działania.
    m_selectBox = new QWidget(this);
    auto* selectLayout = new QHBoxLayout(m_selectBox);
    selectLayout->setContentsMargins(8, 0, 0, 0);
    selectLayout->setSpacing(6);
    for (const char* name : {"Poprzednie", "Zakończ (ESC)", "Wszystko (A)", "Warstwy (L)"}) {
        auto* b = new QPushButton(QString::fromUtf8(name), m_selectBox);
        b->setObjectName("inputHint");
        b->setFocusPolicy(Qt::NoFocus);
        b->setToolTip(b->text() + " (wkrótce)");
        b->setEnabled(false);
        selectLayout->addWidget(b);
    }
    layout->addWidget(m_selectBox);

    m_ok = new QPushButton("OK", this);
    m_ok->setFocusPolicy(Qt::NoFocus);
    connect(m_ok, &QPushButton::clicked, this, &InputBar::onOk);
    layout->addWidget(m_ok);

    showIdle();
}

void InputBar::showIdle()
{
    // Jak w Alphacam: bez polecenia miejsce paska jest puste.
    m_picking = false;
    m_selecting = false;
    m_command->clear();
    m_prompt->clear();
    m_command->hide();
    m_prompt->hide();
    m_fieldsBox->hide();
    m_selectBox->hide();
    m_ok->hide();
    emit activeChanged(false);
}

void InputBar::startSelect(const QString& command, const QString& prompt)
{
    m_picking = false;
    m_selecting = true;
    m_command->setText(command.toUpper());
    m_command->show();
    setPrompt(prompt);
    m_fieldsBox->hide();
    m_selectBox->show();
    m_ok->hide(); // wybór zatwierdza PPM lub Enter
    emit activeChanged(false);
}

void InputBar::startPoint(const QString& command, const QString& prompt)
{
    m_picking = true;
    m_selecting = false;
    m_command->setText(command.toUpper());
    m_command->show();
    for (int i = 0; i < 2; ++i) {
        m_bypassed[static_cast<size_t>(i)] = false;
        m_fields[static_cast<size_t>(i)]->setText("0");
        setError(i, false);
    }
    setPrompt(prompt);
    m_fieldsBox->show();
    m_selectBox->hide();
    m_ok->show();
    emit activeChanged(true);
}

void InputBar::setPrompt(const QString& prompt)
{
    m_basePrompt = prompt;
    showPrompt();
}

void InputBar::showPrompt()
{
    QString text = m_basePrompt;
    if (m_picking && anyBypassed()) {
        // Podpowiedź zastępcza po pominięciu wartości (F1).
        QStringList unknown;
        if (m_bypassed[0]) unknown << "X";
        if (m_bypassed[1]) unknown << "Y";
        text += QString(" – wskaż w widoku (%1 z kliknięcia)").arg(unknown.join(", "));
    }
    m_prompt->setText(text);
    m_prompt->show();
}

bool InputBar::anyBypassed() const
{
    for (bool b : m_bypassed)
        if (b)
            return true;
    return false;
}

void InputBar::setError(int field, bool error)
{
    QLineEdit* f = m_fields[static_cast<size_t>(field)];
    f->setProperty("error", error);
    f->style()->unpolish(f);
    f->style()->polish(f);
}

void InputBar::bypass(int field)
{
    if (!m_picking)
        return;
    m_bypassed[static_cast<size_t>(field)] = true;
    QLineEdit* f = m_fields[static_cast<size_t>(field)];
    f->setText("?");
    setError(field, false);
    showPrompt();
    // Kolejne pole, którego jeszcze nie pominięto.
    const int next = (field + 1) % 2;
    if (!m_bypassed[static_cast<size_t>(next)])
        focusField(next);
}

std::optional<gp_Pnt> InputBar::resolveClick(double x, double y)
{
    // Bez pominiętych pól liczy się samo kliknięcie (jak w Alphacam).
    if (!anyBypassed())
        return gp_Pnt(x, y, 0);
    double v[2] = {x, y};
    bool ok = true;
    for (int i = 0; i < 2; ++i) {
        if (m_bypassed[static_cast<size_t>(i)])
            continue;
        if (const auto value = evaluateExpression(m_fields[static_cast<size_t>(i)]->text())) {
            v[i] = *value;
        } else {
            setError(i, true);
            ok = false;
        }
    }
    if (!ok)
        return std::nullopt;
    return gp_Pnt(v[0], v[1], 0);
}

void InputBar::onOk()
{
    if (m_selecting) {
        emit selectionDone();
        return;
    }
    if (!m_picking)
        return;
    if (anyBypassed()) {
        // Brakującej wartości nie da się zatwierdzić – musi przyjść z kliknięcia.
        showPrompt();
        return;
    }
    double v[2] = {0, 0};
    bool ok = true;
    for (int i = 0; i < 2; ++i) {
        if (const auto value = evaluateExpression(m_fields[static_cast<size_t>(i)]->text())) {
            v[i] = *value;
        } else {
            setError(i, true);
            ok = false;
        }
    }
    if (ok)
        emit pointEntered(v[0], v[1], 0);
}

void InputBar::focusField(int field)
{
    QLineEdit* f = m_fields[static_cast<size_t>((field + 2) % 2)];
    f->setFocus(Qt::TabFocusReason);
    f->selectAll();
}

bool InputBar::handleViewKey(QKeyEvent* e)
{
    if (!m_picking)
        return false;
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        onOk();
        return true;
    }
    if (e->key() == Qt::Key_Tab) {
        focusField(0);
        return true;
    }
    if (e->key() == Qt::Key_F1) {
        bypass(m_bypassed[0] ? 1 : 0);
        return true;
    }
    // Pierwszy znak liczby lub wyrażenia: wpisujemy go od razu do pola X.
    const QString t = e->text();
    if (t.size() == 1 && (t[0].isDigit() || QString("-+.,(").contains(t[0]))) {
        QLineEdit* f = m_fields[0];
        f->setFocus(Qt::OtherFocusReason);
        f->setText(t);
        m_bypassed[0] = false;
        setError(0, false);
        showPrompt();
        return true;
    }
    return false;
}

bool InputBar::eventFilter(QObject* watched, QEvent* event)
{
    // F1 jest w Qt skrótem pomocy – przechwytujemy go, zanim pole go zignoruje.
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
        for (int i = 0; i < 2; ++i) {
            if (watched != m_fields[static_cast<size_t>(i)])
                continue;
            auto* e = static_cast<QKeyEvent*>(event);
            const bool press = event->type() == QEvent::KeyPress;
            if (e->key() == Qt::Key_F1) {
                if (press)
                    bypass(i);
                event->accept();
                return true;
            }
            if (!press)
                break;
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
