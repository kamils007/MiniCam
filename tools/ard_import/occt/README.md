# Odtworzenie analitycznej bryły z raportu Feature

Narzędzie offline `ard_snapshot_to_occt` czyta `feature_geometry.json` z raportu
odczytu ARD. Buduje rzeczywiste krzywe i powierzchnie OCCT, zszywa ściany,
sprawdza zamkniętą bryłę i zapisuje STEP oraz BREP. Nie zastępuje geometrii
siatką ani prostopadłościanem. STEP jest formatem wyjścia tego testu; nie jest
wymogiem silnika OCCT ani Parasolid.

To osobny prototyp: nie podłącza jeszcze przycisku otwierania ARD w MiniCam,
nie wymaga Feature.dll podczas rekonstrukcji i nie zmienia eksportu ALPHACAM.
Nie dołączamy modeli użytkownika do repozytorium.

## Budowanie i uruchomienie

Wymagane: C++17, Qt6 Core, OCCT z modułem STEP i Shape Healing.

```sh
cmake -S tools/ard_import/occt -B build/ard-occt -DCMAKE_BUILD_TYPE=Release
cmake --build build/ard-occt --config Release
build/ard-occt/ard_snapshot_to_occt feature_geometry.json nowy_model.step
python3 tools/ard_import/occt/check_conversion.py build/ard-occt/ard_snapshot_to_occt
```

W generatorze Visual Studio plik wykonywalny może być w podkatalogu `Release`.
Wynik: `nowy_model.step`, `nowy_model.step.brep`, raport JSON na końcu stdout.
Istniejący plik docelowy lub BREP powoduje odmowę nadpisania. Kod wyjścia 0
potwierdza również ponowny import STEP i sprawdzenie objętości oraz topologii.
Błąd podczas samego zapisu/ponownego importu może pozostawić pliki robocze;
nie traktować ich jako poprawnego wyniku, gdy kod wyjścia jest różny od zera.

## Obsługiwany zakres

- Krawędzie liniowe i łuki kołowe z dwoma końcami. Kierunek łuku jest ustalany
  z końców, płaszczyzny i rzeczywistej długości, nie z arbitralnego krótszego łuku.
- Ściany płaskie, cylindryczne i stożkowe; jedna pętla zewnętrzna i otwory.
- Stożek wyznaczany z rzeczywistych okręgów brzegowych; niezależnie sprawdzany
  kąt połówkowy Feature. Nie wymaga nieobecnego XAxis stożka.
- Każda ściana musi zachować pole z raportu. Po zszyciu muszą zgadzać się
  liczby ścian, krawędzi i wierzchołków. Wynik musi stanowić jedną poprawną,
  zamkniętą bryłę o dodatniej objętości. Tolerancja liniowa: 0,000001 mm.
- W tym podzbiorze wymagamy zgodności granic bryły z granicami wierzchołków;
  modele z ekstremami krzywych poza tymi granicami są odrzucane.

Brak obsługi dowolnych NURBS, torusów, kul, pełnych zamkniętych krawędzi,
powtórzonych krawędzi szwu w pętli i wielu brył. Nieznane typy lub niespójne
dane są odrzucane. Kolory nie są odtworzone. Narzędzie nie jest ogólnym
translatorem każdego ARD. Poprawność OCCT i zgodność odczytu STEP nie dowodzą
same w sobie pełnej równoważności z oryginałem Parasolid.

## Potwierdzenie dla raportu v6

Raport użytkownika zawiera 80 wierzchołków, 126 krawędzi (96 linii, 30 łuków),
48 ścian (30 płaskich, 6 cylindrycznych, 12 stożkowych) i 52 pętle.
Odtworzono jedną poprawną zamkniętą bryłę; te liczby zachowano po zapisie
oraz ponownym odczycie STEP. Granice: (0,0,0)–(400,600,18) mm.
Pole: 467098,6579819078 mm². Objętość rekonstrukcji: 3662437,657201545 mm³;
ponowny odczyt STEP: 3662437,657201594 mm³. Nie otrzymano niezależnej
objętości oryginalnego Parasolid do porównania.

Model został otwarty i obejrzany w istniejącym MiniCam w Linux/Xvfb.
Testy dodatkowo odrzuciły zmienioną długość łuku, kąt stożka, pole ściany
i nieznaną powierzchnię. Powtarzalne testy w repo używają syntetycznego
prostopadłościanu (objętość 24 mm³), błędnych danych i ochrony istniejącego pliku.
