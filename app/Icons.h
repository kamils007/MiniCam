#pragma once

#include <QIcon>

// Ikona programu z zasobów (resources/ikony/png): ta sama ikona w 16, 32 i 64 px,
// Qt sam wybiera rozmiar pasujący do przycisku i skalowania ekranu.
// Nazwa jak plik SVG bez rozszerzenia, np. appIcon("przesun").
QIcon appIcon(const QString& name);
