# MiniCAM

Okno Qt 6 z widokiem 3D Open CASCADE. Wczytuje STEP, IGES i BREP
(krok 1) i pozwala wybierać ściany bryły myszką (krok 2).

```
minicam/
├── CMakeLists.txt
├── core/            rdzeń (C++ + OCCT, bez Qt)
│   ├── ModelImport.h/.cpp   wczytywanie plików
│   └── FaceInfo.h/.cpp      typ, pole, normalna i wysokość ściany
└── app/             GUI (Qt)
    ├── main.cpp
    ├── MainWindow.h/.cpp
    └── OccView.h/.cpp   widok 3D
```

## Sterowanie

| Akcja | Mysz / klawisz |
|---|---|
| Obrót | lewy przycisk – przeciągnij |
| Wybór ściany | lewy przycisk – kliknij |
| Dodaj/usuń ścianę z wyboru | Ctrl + klik |
| Wyczyść wybór | Esc |
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
- Informacje o wybranej ścianie pojawiają się w pasku stanu. Dla ściany poziomej
  widać jej wysokość Z – np. dno kieszeni 8 mm pod górą detalu ma Z o 8 mniejsze.
- Plik testowy: dowolny STEP z producenta części, np. z GrabCAD lub katalogu Misumi.
