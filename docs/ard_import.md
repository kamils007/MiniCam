# Otwieranie ARD w MiniCam

„Otwórz model”, ostatnie pliki i argument uruchomienia obsługują `.ard`.
Rozpoznane kontury 2D i jedna obsługiwana bryła 3D są otwierane razem, w źródłowym
położeniu. Nie trzeba ręcznie uruchamiać skryptów ani konwertować pliku do STEP.
Odczyt pozostaje częściowy w odniesieniu do wszystkich danych ALPHACAM.

## Stacja Windows

Dla geometrii 2D wystarcza MiniCam. ARD zawierający bryłę wymaga działającej
instalacji składników ALPHACAM/SolidLink 2025.4, bieżącej licencji CLS i Pythona
3 x64. MiniCam używa istniejących ustawień Python, folderu ALPHACAM i pliku
SolidLink.exe (`QSettings MiniCAM/MiniCAM`, klucze `alphacam/python`,
`alphacam/root`, `alphacam/solidlink`). Sprawdza się je w menu Plik
w ustawieniach ALPHACAM. Odczyt nie wymaga już runtime `alphacam-export`;
runtime i dotychczasowy mechanizm zapisu pozostają potrzebne przy eksporcie.
Sam folder z DLL przeniesiony z innego komputera nie zastępuje instalacji/licencji.

ALPHACAM musi być zamknięty podczas odczytu bryły, tak jak podczas eksportu.
Okno pokazuje postęp. Nie przerywa się pracy przez Escape ani zamknięcie okna
postępu, gdy osobny proces jest właścicielem sesji CLS: worker kończy pracę
z własnymi limitami czasu i zamyka sesję licencji. Program nie ładuje bibliotek
producenta do procesu edytora. Sprawdzane są przypięte wersje/SHA-256, tak jak
w potwierdzonym teście konwersji v4; nie rozluźniamy weryfikacji dla innych instalacji.

Skrypty odczytu są wbudowane w program jako zasoby Qt. Przy otwieraniu MiniCam
rozpakowuje je do katalogu tymczasowego. Nie wymaga podmiany paczki eksportowej
ani nadpisywania jej plików. Aktualizacja tej ścieżki wymaga przebudowania MiniCam.
Foldery zleceń i raporty są w lokalnych danych aplikacji: `ard-import/jobs/<id>`;
ścieżka pojawia się przy błędzie. Zawierają prywatny model i wynikowy STEP,
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
Osobny proces utrzymuje zwykłą sesję CLS, a worker wywołuje oryginalny
`SLInterop::RunInteropStandard`, który konwertuje **cały dokument Parasolid na
STEP**. Nie uruchamia ALPHACAM, nie korzysta z jego API i nie odczytuje krawędzi
przez Feature. Nie wraca automatycznie do rekonstrukcji analitycznej.

MiniCam odrzuca ujemne/częściowe wyniki, awarię procesu i niepotwierdzone zamknięcie
CLS. `modules/ard_import/WholeBodyImport` sprawdza zgodność skrótów ARD, X_B i STEP,
pełny raport konwersji, a następnie używa tego samego `camcore::importModel`
co zwykłe otwieranie STEP. OCCT musi zwrócić jedną poprawną bryłę, zamknięte
powłoki, skończone wymiary i dodatnią objętość, bez dodatkowych obiektów poza bryłą.
STEP jest ponownie sprawdzany po odczycie, aby wykryć zmianę pliku w trakcie.
Nie ma dopasowywania położenia, skalowania do oczekiwanych wymiarów ani zamiany
modelu na siatkę. Kolory dostępne w STEP przechodzą przez istniejący czytnik XCAF.

Obsługiwany zakres powierzchni zależy teraz od konwersji SolidLink i czytnika STEP
OCCT. Nie stosujemy listy dopuszczonych powierzchni dawnego czytnika analitycznego.
Wiele brył pozostaje nieobsługiwane. Ograniczenia parsera rekordów ARD, geometrii
2D i danych CAM pozostają niezależne od konwersji bryły.

## Sprawdzenie i ograniczenia dowodów

- Kompilacja aplikacji na Linux, z modułem eksportu i bez niego.
- CTest: walidacja wyniku całej konwersji, zachowanie położenia i wymiarów STEP,
  odrzucenie niezgodnych źródeł/wyników, błędów CLS, częściowego wyniku i dodatkowej
  geometrii; istniejące testy ARD i eksportu.
- Raport Windows v4 potwierdza sukces oryginalnego SLInterop, niezmienione wejścia
  i poprawne zakończenie CLS bez uruchamiania ALPHACAM. Cały test trwał 3,596927 s,
  a sama funkcja konwersji 0,071709 s. To czasy tego jednego testu, nie pomiar GUI.
- Ten sam moduł C++ co GUI odczytał powstały STEP w OCCT: jedna poprawna bryła,
  63 ściany, 158 krawędzi, 100 wierzchołków, 1200×2300×10 mm; 64 wpisy kolorów.
  Objętość wyniku wynosi 18140400,761265665 mm³. Nie porównano jej z niezależnym
  pomiarem oryginalnego Parasolid; poprawność OCCT nie zastępuje takiego porównania.

Pełnego przebiegu „Otwórz ARD” w nowym GUI na Windows nie wykonano jeszcze w tej
chmurze. To wymaga sprawdzenia nowej kompilacji na stacji z zainstalowanymi składnikami.
Nie deklarujemy obsługi każdego ARD ani niezależnego potwierdzenia objętości
oryginalnego Parasolid. Prywatne modele i runtime nie są dodawane do repozytorium.

Szczegóły poprawki powielania warstw i pozostałe ograniczenia kolorów/uchwytów:
[warstwy ARD](ard_layers.md).

## Historia poprzedniej ścieżki odczytu

Poniższe sekcje opisują wcześniejszy czytnik Feature i rekonstrukcję analityczną.
Narzędzia oraz testy offline zachowano, ale aktualne „Otwórz ARD” używa konwersji
całej bryły opisanej powyżej. [Narzędzie historyczne](../tools/ard_import/occt/README.md).

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


## Pełne okręgi i przecięcia walców — 25.10.2026

Nowszy raport tego samego modelu potwierdził ukończenie odczytu Feature po
poprawce limitu: 1991409 operacji przy budżecie 12005000. Import zatrzymywał
się potem w OCCT na `Ambiguous circular arc`. Raport zawiera 20 pełnych okręgów
oraz 6 krawędzi FeatureSplineEdge, które w tym modelu są odcinkami elips na
przecięciu płaszczyzny z walcem.

Poprawiono rozróżnienie pełnego okręgu i zwykłego łuku, wybór półokręgu według
jawnego zakresu sąsiedniej powierzchni oraz analityczne przecięcia plane/cylinder.
Elipsy nie są zgadywane z końców ani zamieniane na łamane: wynik przecięcia OCCT
musi zgadzać się z oboma końcami i długością, z jednym pasującym odcinkiem.
Nie oznacza to obsługi dowolnego splajnu. Pętle walców zachowują natywną kolejność
wierzchołków i dwa przeciwne użycia krawędzi szwu; ich krzywe UV są uzgadniane
przez przesunięcia o pełny okres, bez zmiany krzywych przestrzennych. Pozostają
kontrole pól każdej ściany, poprawności, zamknięcia, liczników i wymiarów.

SHA-256 geometrii nowego raportu:
`25e52457584621b39a4d75015848f6e059b1953cbff54c2cedd378c069e6c3e6`.
Wspólny moduł GUI odtworzył z niego poprawną zamkniętą bryłę o 210 ścianach,
609 krawędziach i 406 wierzchołkach, 169×300×18 mm. Objętość rekonstrukcji
523792,0497333667 mm³; ponowny import STEP 523792,0497333675 mm³. Nie porównano
jej z niezależnym pomiarem oryginalnego Parasolid.

Przeszło pięć testów CTest (w tym sześć kontroli rekonstrukcji: modele syntetyczne,
niezgodne raporty i odmowa nadpisania), 15 testów czytnika Python oraz kompilacja
aplikacji na Linux. Osobno sprawdzono poprzedni model 48/126/80 z płaszczyznami,
walcami i stożkami: rekonstrukcja i ponowne otwarcie STEP nadal poprawne.
Pełny odczyt ARD w nowej kompilacji GUI na Windows nadal wymaga sprawdzenia
na stacji użytkownika. Modele i pełne raporty pozostają poza repozytorium.

## Przyspieszenie wyszukiwania tożsamości — zgłoszone 30 sekund

W odczycie raportu modelu 210/609/406 było 1951808 porównań IsSame.
Dla świeżego interfejsu czytnik najpierw sprawdza teraz obiekty o zbliżonych
cechach: współrzędnych wierzchołka, typie/długości krawędzi albo typie/polu
ściany. To kolejność przeszukiwania, a nie sposób ustalania tożsamości.
Każde trafienie nadal musi zostać potwierdzone przez natywne IsSame; po
nietrafieniu sprawdzane są pozostałe obiekty. Dwa różne wierzchołki o tych
samych współrzędnych nie są scalane. Brak właściwości lub zmiana jej wartości
nie wyłącza pełnego wyszukiwania. Zapamiętane interfejsy pozostają utrzymywane
przez AddRef do końca odczytu, jak w poprzedniej poprawce.

Odtworzenie dokładnej kolejności referencji na rzeczywistym JSON z raportu
zwróciło identyczne identyfikatory i połączenia wszystkich 1449 obiektów.
Liczba porównań spadła z 1951808 do 350067 (o 82,1%). Bazowy licznik w tej
symulacji jest równy licznikowi natywnemu z raportu. To wynik liczby operacji,
nie pomiar przyspieszenia czasu wykonania na Windows.

W nowym raporcie `timings_seconds` mierzy wydzielenie danych, przygotowanie
prywatnego runtime, kontrolę plików instalacji, odczyt Feature z zamknięciem CLS
i walidację wyniku. `geometry_snapshot.geometry_read_seconds` mierzy samą
ekstrakcję geometrii, a `identity_stats` podaje liczbę porównań i potwierdzonych
trafień wyszukiwania. Nie zmieniamy limitów ani kontroli modelu, nie odtwarzamy
modelu z poprzedniego odczytu i nie zmieniamy ścieżki eksportu.

Przeszło 19 testów czytnika/wyszukiwania Python, 26 regresji backendu,
kompilacja MiniCam i pięć testów CTest na Linux. Testy obejmują kolizję cech
geometrycznych, zmienioną/nieobecną wskazówkę, zwalnianie interfejsów i zgodność
identyfikatorów przy licznych świeżych interfejsach tego samego obiektu.
Czas nowej kompilacji na Windows pozostaje do zmierzenia; zmiana wymaga
ponownej kompilacji, ponieważ skrypty są w zasobach Qt.
