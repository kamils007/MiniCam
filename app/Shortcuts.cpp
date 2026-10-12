#include "Shortcuts.h"

#include <QAction>
#include <QPointer>
#include <QSettings>
#include <QShortcut>

#include <map>

namespace shortcuts {

namespace {

const char* const kSettingsGroup = "skroty";

QSettings settings() { return QSettings("MiniCAM", "MiniCAM"); }

QList<QKeySequence> standard(QKeySequence::StandardKey key) { return QKeySequence::keyBindings(key); }
QList<QKeySequence> key(QKeyCombination k) { return {QKeySequence(k)}; }

// Podpięte akcje i skróty – dostają nowe klawisze po setKeys().
std::multimap<QString, QPointer<QAction>>& boundActions()
{
    static std::multimap<QString, QPointer<QAction>> actions;
    return actions;
}
std::multimap<QString, QPointer<QShortcut>>& boundShortcuts()
{
    static std::multimap<QString, QPointer<QShortcut>> list;
    return list;
}

void apply(const QString& id)
{
    const QList<QKeySequence> k = keys(id);
    auto [a, aEnd] = boundActions().equal_range(id);
    for (; a != aEnd; ++a)
        if (a->second)
            a->second->setShortcuts(k);
    auto [s, sEnd] = boundShortcuts().equal_range(id);
    for (; s != sEnd; ++s)
        if (s->second)
            s->second->setKeys(k);
}

} // namespace

const std::vector<Definition>& all()
{
    static const std::vector<Definition> table = {
        {kNew, "Plik", "Nowy", standard(QKeySequence::New)},
        {kOpen, "Plik", "Otwórz model", standard(QKeySequence::Open)},
        {kSave, "Plik", "Zapisz", standard(QKeySequence::Save)},
        {kSaveAs, "Plik", "Zapisz jako", standard(QKeySequence::SaveAs)},
        {kPrint, "Plik", "Drukarka/Ploter", standard(QKeySequence::Print)},
        {kQuit, "Plik", "Zakończ", standard(QKeySequence::Quit)},
        {kUndo, "Edycja", "Cofnij", key(Qt::CTRL | Qt::Key_Z)},
        {kRedo, "Edycja", "Ponów", {QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)}},
        {kDelete, "Edycja", "Usuń", standard(QKeySequence::Delete)},
        {kFitAll, "Widok", "Dopasuj do okna", key(QKeyCombination(Qt::Key_F))},
        {kToggleDirections, "Widok", "Pokaż/ukryj kierunki geometrii", key(Qt::CTRL | Qt::Key_G)},
        {kRepeatCommand, "Polecenia", "Powtórz ostatnie polecenie", key(QKeyCombination(Qt::Key_Space))},
        {kCancelCommand, "Polecenia", "Przerwij polecenie", key(QKeyCombination(Qt::Key_Escape))},
        {kSnapEnd, "Uchwyty", "KONIEC elementu", key(QKeyCombination(Qt::Key_F6))},
        {kSnapMid, "Uchwyty", "ŚRODEK elementu", key(QKeyCombination(Qt::Key_F7))},
        {kSnapCentre, "Uchwyty", "CENTRUM okręgu", key(QKeyCombination(Qt::Key_F8))},
    };
    return table;
}

QList<QKeySequence> keys(const QString& id)
{
    QSettings s = settings();
    s.beginGroup(kSettingsGroup);
    if (s.contains(id)) {
        QList<QKeySequence> result;
        for (const QString& k : s.value(id).toStringList())
            result << QKeySequence::fromString(k, QKeySequence::PortableText);
        return result;
    }
    for (const Definition& d : all())
        if (d.id == id)
            return d.defaults;
    return {};
}

QString text(const QString& id)
{
    const QList<QKeySequence> k = keys(id);
    return k.isEmpty() ? QString() : k.first().toString(QKeySequence::NativeText);
}

void setKeys(const QString& id, const QList<QKeySequence>& keys)
{
    QStringList list;
    for (const QKeySequence& k : keys)
        list << k.toString(QKeySequence::PortableText);
    QSettings s = settings();
    s.beginGroup(kSettingsGroup);
    s.setValue(id, list);
    s.endGroup();
    apply(id);
}

void resetToDefaults()
{
    QSettings s = settings();
    s.remove(kSettingsGroup);
    for (const Definition& d : all())
        apply(d.id);
}

void bind(QAction* action, const QString& id)
{
    boundActions().emplace(id, action);
    action->setShortcuts(keys(id));
}

void bind(QShortcut* shortcut, const QString& id)
{
    boundShortcuts().emplace(id, shortcut);
    shortcut->setKeys(keys(id));
}

} // namespace shortcuts
