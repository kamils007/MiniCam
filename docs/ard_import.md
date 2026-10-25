# Otwieranie ARD w MiniCam

„Otwórz model”, ostatnie pliki i argument uruchomienia obsługują `.ard`.
Rozpoznane kontury 2D i jedna obsługiwana bryła 3D są otwierane razem, w źródłowym
położeniu. Nie trzeba ręcznie uruchamiać skryptów ani konwertować pliku do STEP.
Odczyt pozostaje częściowy w odniesieniu do wszystkich danych ALPHACAM.

## Stacja Windows

Dla geometrii 2D wystarcza MiniCam. ARD zawierający bryłę wymaga działającej
instalacji ALPHACAM/Feature 2025.4, bieżącej licencji CLS, Pythona 3 x64 i wcześniej
przygotowanego runtime `alphacam-export`. MiniCam używa istniejących ustawień
Python, modułu i folderu ALPHACAM (`QSettings MiniCAM/MiniCAM`, klucze
`alphacam/python`, `alphacam/runtime`, `alphacam/root`). Sprawdza się je w menu
Plik w ustawieniach ALPHACAM. Nie zmieniamy konfiguracji ani sposobu eksportu.
Sam folder z DLL przeniesiony z innego komputera nie zastępuje instalacji/licencji.

ALPHACAM musi być zamknięty podczas odczytu bryły, tak jak podczas eksportu.
Okno pokazuje postęp. Nie przerywa się pracy przez Escape ani zamknięcie okna
postępu, gdy osobny proces jest właścicielem sesji CLS: worker kończy pracę
z własnymi limitami czasu i zamyka sesję licencji. Program nie ładuje bibliotek
producenta do procesu edytora. Sprawdzane są przypięte wersje/SHA-256, tak jak
w potwierdzonym teście v6; nie rozluźniamy weryfikacji dla innych instalacji.

Skrypty odczytu są wbudowane w program jako zasoby Qt. Przy otwieraniu MiniCam
rozpakowuje je do katalogu tymczasowego i nakłada na prywatną kopię istniejącego
runtime. Nie wymaga podmiany paczki eksportowej ani nadpisywania jej plików.
Foldery zleceń i raporty są w lokalnych danych aplikacji: `ard-import/jobs/<id>`;
ścieżka pojawia się przy błędzie. Zawierają prywatny model i kopię runtime,
więc nie należy publikować całego folderu w repozytorium.

Jeżeli bryła jest nieobsługiwana, brakuje składników lub wystąpi błąd konwersji,
aktualny dokument pozostaje otwarty. Program nie zastępuje go po cichu samymi
konturami 2D. Ostrzeżenie o innych nieodtworzonych danych pozwala anulować import
przed zmianą bieżącego dokumentu. Zapis/eksport nie jest bezstratnym zapisem ARD;
źródło nie jest nadpisywane przy otwieraniu.

## Obsługiwane dane

Czytnik C++ rozpoznaje rekordy `Licom-APS File V1.94`: odcinki, łuki w obu
orientacjach, okręgi, warstwy i nazwy Windows-1250/UTF-8, RGB, poziomy Z.
Indeksowana paleta, kierunki i strona narzędzia, widoczność źródłowa oraz dane
obróbki nie są w pełni interpretowane. Dla nieodczytanych ustawień kierunków
nie pokazujemy domyślnych strzałek jako odczytanych z ARD. Listy pomocnicze i cache
są zachowywane w źródle i zgłaszane; nieznane układy rekordów są odrzucane,
a nie przeszukiwane po markerach binarnych.

Dla jednej bryły z rekordu Feature v3 wydzielany jest oryginalny binarny Parasolid.
Feature w osobnym workerze odczytuje geometrię. MiniCam sprawdza zgodność skrótów
ARD, Parasolid i JSON, liczniki oraz wymiary. Wspólny moduł
`modules/ard_import/FeatureGeometry` tworzy bezpośrednio kształt OCCT:
linie/łuki, płaszczyzny, cylindry i stożki wraz z pętlami oraz otworami.
Sprawdza pola ścian, zamknięcie i poprawność bryły oraz zachowanie liczników.
Nie używa siatki ani prostopadłościanu jako zamiennika modelu.

Nie obsługujemy jeszcze wielu brył, dowolnych NURBS, torusów, kul, zamkniętych
krawędzi bez dwóch końców i szwów z powtórzoną krawędzią w pętli. Kolory ścian
są pomijane, ponieważ getter Feature.Color wymaga nieobsługiwanych ustawień
rysunku ALPHACAM. Nieznane powierzchnie i niespójna geometria powodują błąd.
[Zakres rekonstrukcji i narzędzie offline](../tools/ard_import/occt/README.md).

## Sprawdzenie i ograniczenia dowodów

- Kompilacja aplikacji na Linux, z modułem eksportu i bez niego.
- CTest: syntetyczne kontury/warstwy ARD, rekonstrukcja bryły, walidacja raportu,
  istniejące testy warstw eksportu i aktualnego modelu STEP.
- Testy odrzucają niewłaściwy hash źródła/Parasolid/JSON, niezgodne liczniki lub
  wymiary, negatywny raport, zmieniony runtime i niedozwolony zapis.
- Na rzeczywistym raporcie v6 ten sam moduł, którego używa GUI, odtworzył poprawną
  bryłę: 48 ścian, 126 krawędzi, 80 wierzchołków, 400×600×18 mm.
- Sam natywny odczyt Feature potwierdzono w raporcie z Windows. Rekonstrukcję,
  zapis i ponowny odczyt STEP oraz widok w MiniCam sprawdzono na Linux.

Pełnego przebiegu „Otwórz ARD” w nowym GUI na Windows nie wykonano jeszcze w tej
chmurze. To wymaga sprawdzenia nowej kompilacji na stacji z istniejącym runtime.
Nie deklarujemy obsługi każdego ARD ani niezależnego potwierdzenia objętości
oryginalnego Parasolid. Prywatne modele i runtime nie są dodawane do repozytorium.

Szczegóły poprawki powielania warstw i pozostałe ograniczenia kolorów/uchwytów:
[warstwy ARD](ard_layers.md).

## Większe bryły — poprawka limitu 25.10.2026

Raport użytkownika potwierdził udany ImportAs (jedna bryła), ważną licencję CLS
oraz prawidłowe zakończenie procesów, ale ekstrakcja zatrzymała się błędem
`Geometry operation limit exceeded`. Model miał 210 ścian, 609 krawędzi i 406
wierzchołków. Spośród 250000 operacji aż 237471 stanowiły porównania IsSame;
nie powstał kompletny plik geometrii. Nie był to raport błędu licencji ani
wcześniejszego `Unsupported edge type`.

Budżet odczytu zależy teraz od zmierzonych liczników: max(250000, 8 × N²),
N = suma ścian, krawędzi i wierzchołków, z górną granicą 20000000 operacji.
Dla tego raportu wynosi 12005000. Limit czasu procesu Feature pozostaje 90 s.
Zapamiętujemy tożsamość wcześniej zweryfikowanych interfejsów; każda zapamiętana
referencja, również alias, jest utrzymywana do końca ekstrakcji, aby nie pomylić
ponownie użytego adresu COM. Pamięć i liczba referencji nadal mają limit.
Dziennik próbkuje powtarzające się IsSame, zachowując wszystkie inne operacje
oraz końcowe liczniki. Błąd natywny trafia do komunikatu odczytu zamiast znikać
za ogólnym „odczyt niepotwierdzony”.

Przeszło 15 testów czytnika Python, 26 regresji backendu, kompilacja aplikacji
i 5 testów CTest na Linux. Odtworzenie 250000 zarejestrowanych operacji przeszło
przez nowy budżet; dziennik zmalał z ok. 15,6 MB do ok. 0,81 MB. To kontrola
limitu i logowania, nie ponowne wykonanie COM. Pełnego natywnego odczytu tego
modelu po poprawce nie wykonano jeszcze na Windows. Zmiana wymaga ponownej
kompilacji MiniCam, ponieważ skrypty odczytu są wbudowane w zasoby aplikacji.
