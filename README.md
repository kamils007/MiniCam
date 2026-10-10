# MiniCAM

Okno Qt 6 z widokiem 3D Open CASCADE. Wczytuje STEP, IGES i BREP razem z kolorami
zapisanymi w pliku. Na górze okna jest wstążka w stylu Alphacam (Plik, Narzędzia
główne, Ekstrakcja modelu bryłowego, Bryły - Użytkowe).

**Auto-wyrównanie** (Bryły - Użytkowe): ustawienia w „Rozpoznawanie cech modelu…”.
Przy „Wyrównanie panelu” na stole kładzie się płaszczyzna o największej sumie pól
leżących w niej ścian (kieszenie mogą być z obu stron – spodem zostaje strona mniej
wycięta), najdłuższa krawędź idzie wzdłuż osi X lub Y, a punkt bazowy i zero Z
trafiają w 0,0,0. Przycisk Auto-Wyrównanie Części odwraca bryłę na drugą stronę
(kolejne kliknięcie przywraca). Ustawienia zapisują się same i można je
wyeksportować do pliku .ini.

**Panel Warstwy** (po lewej, dokowany): wygląd jak w Alphacam – pionowy pasek
zakładek, pasek narzędzi warstw (przyciski na razie nieaktywne) i drzewo warstw
z checkboxami: Geometrie APS, Konstrukcje, Drogi Narzędzia, Wymiary, Splajny,
Powierzchnie, Tekst, Bryły, STL, Warstwy Użytkownika. Checkbox włącza/wyłącza
widoczność warstwy albo pojedynczej geometrii. Rozpoznawanie przypina geometrie
do warstw użytkownika (`layerForContour` w `core/Geometry.h`):
- obrys bryły → „userKonturZew” (niebieska),
- okręgi o średnicach wierteł 2–15 mm (na wylot czy nie) → „userOtwory” (czerwona),
- pozostałe na wylot, wewnątrz obrysu → „userKonturWew” (fioletowa),
- pozostałe okręgi → „userKieszenOkragla” (pomarańczowa),
- reszta na razie niesklasyfikowana → „Geometrie APS” (zielona). Warstwy trzyma `camcore::LayerList`
(`core/Layers.h`): zawsze jest w niej APS, a warstwy użytkownika dochodzą przez
`createLayer` – dziś z rozpoznawania cech (warstwa powstaje przy pierwszej
geometrii, która do niej trafia), później także ręcznie z panelu. Geometria ma właściwości
(`core/Geometry.h`): warstwę i widoczność; kolor bierze z warstwy (APS – zawsze
zielony, jak w Alphacam).

**Rozpoznawanie cech** (Ekstrakcja modelu bryłowego → Rozpoznaj cechy): program
rozbiera płytę na kontury, idąc po płaskich poziomych powierzchniach (bez wierzchu):
- **Kontur** – obrys bryły z rzutu z góry (dokładny algorytm HLR z OCCT: linie
  i łuki zostają liniami i łukami; fazy i zaokrąglenia przy spodzie czy wierzchu
  nie zmniejszają obrysu).
- **Kontury wewnętrzne** – pozostałe wycięcia na wylot.
- **Kieszenie** – wszystko powyżej Z 0. Kieszeń ma kontur zewnętrzny i może mieć
  kontury wewnętrzne (wyspy).

Bierzemy tylko kontury, których ścianki rosną w Z do góry – kieszenie od spodu
są pomijane. Każdy kontur składa się z geometrii (odcinki, łuki – po kolei wzdłuż
konturu) i ma wysokość swoich ścian: leży na ich górze, a pod nim rysowana jest
kolorowa ścianka do dołu ścian. Kliknięcie geometrii w panelu Warstwy podświetla ją
na pomarańczowo. Kod: `core/PartContours`.

```
minicam/
├── CMakeLists.txt
├── core/            rdzeń (C++ + OCCT, bez Qt)
│   ├── ModelImport.h/.cpp   wczytywanie plików (geometria + kolory)
│   ├── ModelAlign.h/.cpp    auto-wyrównanie (obrót + przesunięcie do 0,0,0)
│   └── FeatureRecognition.h/.cpp  rozpoznawanie otworów, kieszeni, wycięć
└── app/             GUI (Qt)
    ├── main.cpp
    ├── MainWindow.h/.cpp
    ├── Ribbon.h/.cpp              wstążka (zakładki, grupy, przyciski)
    ├── AlignSettingsDialog.h/.cpp okno Konfiguracja → Auto-wyrównanie
    └── OccView.h/.cpp   widok 3D
```

## Sterowanie

| Akcja | Mysz / klawisz |
|---|---|
| Obrót | lewy przycisk |
| Przesuwanie | środkowy lub prawy przycisk |
| Zoom (do kursora) | kółko |
| Dopasuj do okna | F |
| Otwórz | Ctrl+O |

## Budowanie

### Windows (Visual Studio + vcpkg) – zalecane

Visual Studio 2022 17.6+ / 2026 ma wbudowane vcpkg – nic nie trzeba instalować.
Wersje bibliotek są przypięte w `vcpkg.json` (`builtin-baseline` + `overrides`).

Otwórz folder projektu (lub sklonuj repo),
wybierz preset **x64 Debug** i cel **minicam.exe**, naciśnij F5.
Biblioteki z `vcpkg.json` (Qt, Open CASCADE) zbudują się same przy pierwszej
konfiguracji – to trwa 1–2 h, później są w cache.

**Preset x64 Debug (DLL)** używa bibliotek dynamicznych (triplet `x64-windows`).
Dobry do codziennej pracy: linkowanie jest szybsze, a jeśli Qt i OCCT w wersji DLL
są już zbudowane, nic nie buduje się od nowa. Każdy preset ma osobny folder
w `out/build/`, więc można je przełączać bez kasowania czegokolwiek.

**Program na inny komputer:** wybierz preset **x64 Release**, zbuduj
i skopiuj `out/build/x64-release/minicam.exe`. To jeden plik – wszystkie
biblioteki są linkowane statycznie (triplet `x64-windows-static`), nie trzeba
DLL ani VC++ Redistributable.

Alternatywa: instalator Qt (qt.io) + gotowe binarki OCCT z dev.opencascade.org,
wtedy wskaż je przez `-DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64;C:/OpenCASCADE-7.9.0/cmake"`.

### Linux (Ubuntu 24.04)

```
sudo apt install cmake g++ qt6-base-dev libocct-*-dev occt-misc
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/minicam detal.step
```

### macOS (Homebrew)

```
brew install qt opencascade cmake
cmake -B build -S . -DCMAKE_PREFIX_PATH="$(brew --prefix qt);$(brew --prefix opencascade)"
cmake --build build -j
```

## Uwagi

- Pod Linuxem program sam przełącza Qt na X11 (`QT_QPA_PLATFORM=xcb`), bo widok OCCT
  wymaga natywnego okna X11. Pod Waylandem działa przez XWayland.
- Przy dynamicznym Qt (triplet `x64-windows`) CMake sam kopiuje wtyczkę
  `platforms/qwindows.dll` obok `minicam.exe`; DLL-e OCCT i Qt muszą być w PATH
  albo obok exe. Przy domyślnym, statycznym tripletcie nic nie trzeba kopiować.
- Plik testowy: dowolny STEP z producenta części, np. z GrabCAD lub katalogu Misumi.
