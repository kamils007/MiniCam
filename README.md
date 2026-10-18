# cam&L

Materiały integracji brył ALPHACAM/ARD, działający wzorzec i granice portu C++:
[przekazanie integracji](docs/alphacam/integracja.md).

Opcjonalny [moduł eksportu ALPHACAM](modules/alphacam/README.md) dodaje w menu
Plik zapis bieżącej bryły i geometrii do ARD. Konwersja działa w osobnym procesie;
ustawienia i zależności są potrzebne dopiero przy użyciu eksportu.

(dawniej MiniCAM; plik programu nadal nazywa się `minicam.exe`)

Okno Qt 6 z widokiem 3D Open CASCADE. Wczytuje STEP, IGES i BREP razem z kolorami
zapisanymi w pliku. Na górze okna jest wstążka w stylu Alphacam (Plik, Narzędzia
główne, Ekstrakcja modelu bryłowego, Bryły - Użytkowe).

**Narzędzia główne → CAD** (jak Input CAD / Output CAD w Alphacam): „Wczytaj CAD…” na razie
wyłączone. „Zapisz CAD…” zapisuje widoczne geometrie do DXF (R12, mm, `core/DxfExport`):
odcinki → LINE, łuki → ARC, okręgi → CIRCLE, inne krzywe → krótkie odcinki; każdy element
na swojej wysokości Z. Warstwy użytkownika stają się warstwami DXF (kolor najbliższy
z pełnej palety DXF 1–255, elementy mają kolor i typ linii „jak warstwa”, nazwa bez
polskich znaków i spacji), geometrie APS idą na warstwę „0”.

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

**Przesuń** (Edycja → Przesuń): na pasku wprowadzania „PRZESUŃ: Wskaż” i przyciski
Poprzednie / Zakończ (ESC) / Wszystko (A) / Warstwy (L) (na razie bez działania).
Geometria pod kursorem dostaje białą przerywaną linię, bryła pod kursorem się wyszarza.
LPM dodaje element do wyboru (drugie kliknięcie odejmuje): wybrana geometria ma cienką
ciągłą niebieską linię, wybrana bryła jest cała niebieska. PPM (albo Enter) kończy wybór.
Potem przy kursorze jest krzyż linii X/Y/Z, a pasek wprowadzania czeka na punkt bazowy
i docelowy: klik w widoku (płaszczyzna Z 0), uchwyt albo wpisanie X/Y. Po punkcie
bazowym uchwyt się zwalnia, a kopia elementów jedzie za kursorem (bryła półprzezroczysta,
geometria same linie w kolorze warstwy). Esc przerywa.

**Uchwyty** (przy wskazywaniu punktu, na belce polecenia): AUTO (końce, środki,
ćwiartki), KONIEC elementu (F6), ŚRODEK elementu (F7), CENTRUM okręgu (F8), ĆWIARTKI
koła. Po wybraniu uchwytu znika krzyż, a kursor klei się do takich punktów widocznych
geometrii i krawędzi bryły (zielona piłeczka); kliknięcie bierze ten punkt (X, Y). Drugie kliknięcie wyłącza
uchwyt. Przecięcie, styczna, prostopadła, równoległa i Filtry – na razie bez działania.

**Cofnij / Ponów** (Edycja, Ctrl+Z / Ctrl+Y): cofa i ponawia przesunięcia bryły
i geometrii. Otwarcie pliku, wyrównanie i ponowne rozpoznanie cech czyszczą historię.

**Geometria → 2D** (rysowanie jak w Alphacam, `app/commands/DrawCommands`, `core/Sketch`):
wszystko leży na płaszczyźnie XY (Z 0) i ma zerową wysokość – także punkty złapane uchwytem
na bryle trafiają na Z 0. Punkty klika się w widoku (z uchwytami) albo wpisuje w pasek
wprowadzania; za kursorem jedzie biały podgląd elementu. Polecenie działa, dopóki się go
nie przerwie (PPM albo Esc), nowa geometria trafia do warstwy APS, Ctrl+Z cofa.
- **Linia** – punkt „Od”, potem kolejne „Do”; odcinki tworzą jedną geometrię (Ctrl+Z cofa
  ostatni odcinek). Kliknięcie w punkt początkowy (uchwyt KONIEC) zamyka kontur.
- **Łuk ▾** – Początek + Drugi + Koniec, 2 punkty + Promień (przeciwnie do wskazówek zegara;
  promień ujemny = dłuższy łuk), 2 punkty + Środek, Początek + Środek + Kąt rozwarcia
  (wpisany albo z kliknięcia; ujemny = zgodnie ze wskazówkami).
- **Koło ▾** – Środek + Średnica / Promień (wartość raz, potem klikane kolejne środki),
  Środek + Punkt, 2 punkty, 3 punkty.
- **Prostokąt** – dwa przeciwległe narożniki.

**Utnij / Połącz / Rozdziel** (Edycja → Przerwij, połącz itd., jak Join / Explode w Alphacam,
`app/commands/JoinCommands`, `core/GeometryEdit`): wybór geometrii (LPM), PPM, Enter albo OK
wykonuje, Esc anuluje, Ctrl+Z cofa.
- **Połącz** – elementy wybranych geometrii łączą się koniec do końca (tolerancja 0,01 mm)
  w jedną geometrię; gdy się nie stykają, powstaje kilka. Wspólna warstwa zostaje,
  z różnych warstw geometria trafia do APS.
- **Utnij** (`app/commands/TrimCommand`, jak Trim w Alphacam) – najpierw geometrie tnące (LPM,
  PPM zatwierdza; zostają pomarańczowe), potem geometrie do ucięcia: klika się je w kawałku,
  który ma zniknąć, PPM ucina. Kawałek znika między najbliższymi przecięciami z geometriami
  tnącymi (w rzucie na XY), a na otwartej geometrii bez przecięcia po jednej stronie – aż do
  jej końca. Zamknięta geometria potrzebuje dwóch przecięć i staje się otwarta. Warstwa
  i wysokość ścian zostają.
- **Rozdziel** – geometria rozpada się na osobne elementy (każdy odcinek i łuk osobno),
  z tą samą warstwą i wysokością ścian.

**3D → Poziomy Z** (grupa „Ustaw poziomy Z/Materiał”, jak Z levels w Alphacam,
`app/commands/ZLevelsCommand`, `core/GeometryEdit`): wybór geometrii (LPM), PPM, Enter albo OK
otwiera okno „Definiuj poziomy Z” jak w Alphacam: Góra Z i Dół Z (na start aktualne poziomy wybranej
geometrii; Dół może być poniżej 0, w polach działają wyrażenia), przy każdym przycisk „...” – Z ze
wskazanego punktu (uchwyt albo pasek wprowadzania), i Różnica liczona na bieżąco. OK kończy
polecenie – płaska geometria dostaje wysokość: leży na Górze Z, a ściany schodzą do Dołu Z. Esc
anuluje, Ctrl+Z cofa. Przesuń zmienia położenie geometrii tylko w X i Y –
poziomy Z zostają, także gdy punkt bazowy albo docelowy złapano uchwytem na innej wysokości. Pozostałe grupy zakładki 3D – później.

**Kierunek i strona narzędzia** (jak Direction / Tool side w Alphacam, `core/ToolDirection`):
każda geometria ma kierunek (CW / CCW – zgodnie lub przeciwnie do ruchu wskazówek zegara,
patrząc z góry) i stronę narzędzia względem kierunku ruchu (lewa, prawa, środkiem).
Domyślnie wszystkie są na zewnątrz, CW (CW + narzędzie po lewej). Pokazuje to mała zielona
strzałka 3D na górnym poziomie geometrii: wychodzi z punktu początkowego, wskazuje kierunek
ruchu frezu i stoi po stronie narzędzia (środkiem – na linii). Ma stały rozmiar na ekranie.
Technologia → Narzędzia i kierunki → Pokaż kierunki albo Ctrl+G włącza i wyłącza strzałki (stan się zapamiętuje).
**Kierunki narzędzia…** (obok, jak Tool Directions w Alphacam, `app/ToolDirectionsDialog`): okno z kierunkiem
i stroną osobno dla geometrii otwartych (Odwróć / Bez zmian; Lewa, Prawa, Zamień L/P, Środkiem, Bez zmian)
i zamkniętych (CW, CCW, Odwróć, Bez zmian, Narzędzie po lewej / po prawej; Na zewnątrz, Do środka, Lewa,
Prawa, Zamień zewnątrz/środek, Środkiem, Bez zmian, Automatycznie). Zmiana kierunku zostawia narzędzie
po tej samej stronie geometrii. „Wszystkie geometrie” zmienia od razu wszystkie, „Wybierz geometrie…” –
wybór w widoku (LPM), PPM zmienia; Ctrl+Z cofa. Punkt startowy – na razie tylko układ (wyłączony).
Domyślną stronę wybiera program (`core/ToolSides`, frezowanie współbieżne: na zewnątrz CW,
do środka CCW, narzędzie zawsze po lewej):
- po rozpoznaniu cech obrys bryły jest zawsze na zewnątrz, otwory o średnicy wiertła – środkiem,
  a reszta idzie po stronie, po której przy ściance nie ma materiału (sprawdzenie bryły tuż obok
  geometrii, w połowie wysokości ścianki),
- geometria narysowana albo połączona: leżąca w parzystej liczbie zamkniętych geometrii
  (0, 2 – wyspa…) – na zewnątrz, w nieparzystej – do środka; otwarta – środkiem.

**Pasek wprowadzania** (lewa część belki polecenia, jak Input Bar w Alphacam,
`app/InputBar`): bez polecenia belka jest pusta. Polecenie, które potrzebuje punktu,
pokazuje podpowiedź (np. „PRZESUŃ Punkt bazowy”), pola X, Y i Z z przyciskami „F1=?”,
przycisk OK i przyciąganie. Punkt można wpisać w pola (OK lub Enter) albo kliknąć
w widoku. F1 lub „F1=?” pomija nieznaną wartość – podpowiedź się zmienia, a brakująca
współrzędna przychodzi z kliknięcia (wpisane zostają). Pisanie w widoku od razu trafia
do pola X, Tab / Shift+Tab przechodzi między polami, Esc przerywa. W polach można pisać
wyrażenia, np. `100/3+2*(5-1)`; przecinek działa jak kropka.

**Dolne belki** (jak w Alphacam): belka polecenia – po lewej pasek wprowadzania,
po prawej przyciąganie (9 przycisków);
stopka – komunikaty, współrzędne kursora na płaszczyźnie Z 0, 15 przycisków
widoków i 4 przełączniki. Przyciski są na razie puste, bez funkcji.

```
minicam/
├── CMakeLists.txt
├── core/            rdzeń (C++ + OCCT, bez Qt)
│   ├── ModelImport.h/.cpp   wczytywanie plików (geometria + kolory)
│   ├── ModelAlign.h/.cpp    auto-wyrównanie (obrót + przesunięcie do 0,0,0)
│   ├── Sketch.h/.cpp        rysowanie 2D: odcinki, łuki, okręgi, prostokąty
│   ├── GeometryEdit.h/.cpp  edycja geometrii: Utnij, Połącz, Rozdziel, poziomy Z
│   ├── ToolDirection.h/.cpp kierunek i strona narzędzia, strzałka kierunku
│   ├── ToolSides.h/.cpp     domyślna strona frezowania (bryła, zagnieżdżenie)
│   └── FeatureRecognition.h/.cpp  rozpoznawanie otworów, kieszeni, wycięć
└── app/             GUI (Qt)
    ├── main.cpp
    ├── MainWindow.h/.cpp
    ├── Ribbon.h/.cpp              wstążka (zakładki, grupy, przyciski)
    ├── AlignSettingsDialog.h/.cpp okno Konfiguracja → Auto-wyrównanie
    ├── BottomBars.h/.cpp          dolne belki: belka polecenia (CommandBar) i stopka
    ├── InputBar.h/.cpp            pasek wprowadzania (lewa część belki polecenia)
    ├── Shortcuts.h/.cpp           skróty klawiszowe – wszystkie w jednym miejscu
    ├── ToolDirectionsDialog.h/.cpp okno Kierunki narzędzia (Technologia)
    ├── OccView.h/.cpp             widok 3D: rysowanie, wybór, mysz
    ├── OccViewPick.cpp            widok 3D: wskazywanie punktu, uchwyty, podgląd przesuwania
    └── commands/                  polecenia krok po kroku (jak w Alphacam)
        ├── Command.h/.cpp         klasa bazowa + CommandHost (co polecenie dostaje od okna)
        ├── DrawCommands.h/.cpp    Linia, Łuk, Koło, Prostokąt (zakładka Geometria)
        ├── JoinCommands.h/.cpp    Połącz, Rozdziel (Edycja)
        ├── TrimCommand.h/.cpp     Utnij (Edycja)
        ├── MoveCommand.h/.cpp     Przesuń
        └── ZLevelsCommand.h/.cpp  Poziomy Z (zakładka 3D)
```

## Sterowanie

Spacja powtarza ostatnie polecenie (Przesuń, Rozpoznaj cechy, Auto-Wyrównanie,
Rozpoznawanie cech modelu…) – tak, jakby kliknąć jego przycisk na wstążce.

| Akcja | Mysz / klawisz |
|---|---|
| Obrót | lewy przycisk |
| Przesuwanie | środkowy lub prawy przycisk |
| Zoom (do kursora) | kółko |
| Dopasuj do okna | F |
| Otwórz | Ctrl+O |
| Pokaż/ukryj kierunki geometrii | Ctrl+G |

Wszystkie skróty są w jednym miejscu – `app/Shortcuts.h/.cpp` (identyfikator, nazwa,
klawisze domyślne), pod przyszłe okno Plik → Dostosuj → Klawiatura. Zmienione klawisze
zapisują się w ustawieniach (grupa „skroty”) i od razu działają w akcjach podpiętych
przez `shortcuts::bind`.

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

**Menu radialne** (PPM w widoku 3D, gdy nie trwa żadne polecenie): jeden pierścień 14 pól
wokół kursora jak w Alphacam; najechanie na czarne strzałki » rozszerza kółko o łuk
z dodatkowymi polami. Na razie pola są puste – polecenia wpisuje się w
`m_radialSlots` (MainWindow). Podczas wybierania elementów PPM nadal zatwierdza wybór.
