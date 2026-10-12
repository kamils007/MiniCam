#pragma once

#include <QString>

// Nazwa programu widoczna dla użytkownika (tytuł okna, Pomoc, wydruk).
// Plik wykonywalny (minicam.exe) i klucze ustawień (QSettings "MiniCAM") zostają bez zmian,
// żeby nie zgubić zapisanych ustawień. W tekście przycisków i menu "&" trzeba podwoić ("&&").
inline const QString kAppName = QStringLiteral("cam&L");
