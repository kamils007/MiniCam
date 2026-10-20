# Potwierdzony odczyt bryły z testFAz.ard przez Feature

Źródło: dostarczony przez użytkownika `raport_odczytu.zip` z rzeczywistego testu
Windows, schemat `minicam-ard-feature-read-v1`. Raport ponownie sprawdzono
walidatorami CLS, odczytu i pomiarów. Ekstrakcję porównano z pierwotnym ARD
przechowywanym poza repozytorium.

- ARD SHA-256: `13b01c7743bf2e97ca1b503362a476b08706ce562a49c17f3ce1a22c8974e93f`.
- Parasolid: 36 320 bajtów, SHA-256
  `841144d6d6fb91c3d6edb2b67e30a99934f288692bef8d312322f93d024af571`.
- Feature ImportAs: HRESULT 0, wynik 1, Bodies.Count = 1.
- Min XYZ = (0, 0, 0), Max XYZ = (400, 600, 18); profil API w milimetrach,
  bez dopasowania skali lub automatycznego wyrównania.
- Faces.Count = 48, Edges.Count = 126, Vertices.Count = 80.
- Proces Feature i właściciel CLS zakończyły się kodem 0. Licencja pozostała
  ważna do końca testu; CloseLicenseConnection potwierdzone.
- Nie zaobserwowano Acam.exe w 30 próbkach monitora. Nie było wywołania Save
  ani serializacji ARD. Źródło i sprawdzane pliki runtime pozostały niezmienione.

**Potwierdzono ARD → wydzielenie oryginalnego Parasolid → import do Feature →
pomiar bryły i odczyt liczników topologii.** To nie było wywołanie ObjectRead3;
użyto rozpoznanej ścieżki ImportAs na osadzonym X_B.

Nie potwierdzono konwersji do OCCT/STEP, wyświetlenia w MiniCam, pełnej
poprawności topologii, objętości ani zgodności każdego elementu z oryginałem
ALPHACAM. Raport jawnie zawiera converted_to_occt=false, geometry_verified=false
i volume_verified=false. Następny nierozwiązany etap to przekazanie rzeczywistej
geometrii do OCCT. Nie powtarzać diagnostyki ścieżek lub istnienia metod jako
zamiennika tego etapu.

## Raport geometrii v4 — blokada Options

Kolejny raport użytkownika: ARD SHA-256
`20ffc7d04de2e31c833624bc9d7f6855864e230d950322453ba3a7ac12089a5b`.
ImportAs ponownie zwrócił 1, Bodies.Count = 1. Podczas pobierania szczegółów
Feature wywołał IDrawing.Options (PROPERTYGET, DISPID 95, slot x64 0x350).
Host nie obsługuje tej metody i zgodnie z zasadą przerwania nieznanego wywołania
zakończył proces kodem 80. Brak feature_geometry.json; brak potwierdzenia pełnego
odczytu geometrii. CLS zamknięty prawidłowo, wejście i runtime niezmienione.
Sam raport nie określa, który getter Feature wywołał Options. Nie wolno zgadywać
wartości ustawień ani traktować importu jako udanej konwersji do OCCT.

Paczka v5 dodaje geometry_calls.jsonl z zapisem i flush przed każdym getterem,
Item i IsSame oraz checkpoint pomiarów przed geometrią. To diagnostyka przyczyny,
nie implementacja Options i nie potwierdzona naprawa. Dziennik zostaje włączony
do raportu nawet przy natychmiastowym zakończeniu procesu przez host.
Sprawdzono 13 testów Python, pyflakes i integralność ZIP. Natywny test wymaga Windows.

## Raport v5 i zmiana v6

Dziennik geometry_calls.jsonl kończy się na operacji 32539: get Color.
Host zgłasza wtedy IDrawing.Options i kończy proces kodem 80. Przed tym
wykonano pętle odczytu wierzchołków i krawędzi oraz rozpoczęto ściany.
Nie powstał kompletny feature_geometry.json; brak potwierdzonej konwersji.

W v6 nie wywołujemy Color. Pole color ma null, a property_errors zawiera
jawny powód host_dependency_not_implemented. Nie zgadujemy koloru ani ustawień
rysunku. Pozostały odczyt geometrii i zapis dziennika pozostają aktywne.
Testowa atrapa Color kończy proces przez SystemExit, co sprawdza pominięcie
niebezpiecznego gettera także poza zwykłym przechwytywaniem Exception.
13 testów Python i pyflakes przeszły; ZIP zweryfikowany. Dalszy odczyt wymaga
potwierdzenia na Windows, kolory nadal pozostają nieodczytane.

## Raport v6 — odczyt geometrii i odtworzenie w OCCT potwierdzone

Raport v6 ma read_confirmed=true i geometry_snapshot_available=true. Proces
Feature i CLS zakończyły się kodem 0, stop_reason=completed_measurement_only,
CloseLicenseConnection potwierdzone, źródło i runtime bez zmian.
SHA-256 feature_geometry.json:
`d64e38c69a3350f80cb283137ea89679c3fee08cb39f1347f28e287806f8b9d4`.
Hash sprawdzono niezależnie, ponownie uruchomiono validate_geometry i porównano
liczniki z pomiarem. 80 wierzchołków, 126 krawędzi, 48 ścian. 60 jawnych
braków właściwości: 48 pominiętych Color i 12 pustych XAxis stożków.

Na podstawie rzeczywistych danych zbudowano narzędzie offline
[occt/README.md](../../tools/ard_import/occt/README.md). Odtworzyło ono poprawną
zamkniętą bryłę OCCT z 30 płaszczyzn, 6 cylindrów i 12 stożków, bez aproksymacji
siatką. Zachowano liczniki topologii i pola ścian. Wymiary 400×600×18 mm,
objętość rekonstrukcji 3662437,657201545 mm³. Ponowny odczyt STEP potwierdził
poprawność, liczniki i objętość (różnica ok. 0,000000049 mm³). Wynik otwarto
w istniejącym GUI MiniCam na Linux/Xvfb i sprawdzono widok z otworami,
zaokrągleniami i fazami. Prywatny model oraz zrzut ekranu pozostają poza repo.

Nie zmieniamy flag oryginalnego raportu: sam odczyt Feature nie wykonywał
konwersji. Powyższe potwierdzenie OCCT dotyczy osobnego, późniejszego testu.
Nie porównano objętości z niezależnym wynikiem oryginalnego Parasolid.
Bezpośrednie otwieranie ARD z tą konwersją w GUI nie jest jeszcze podłączone;
nie potwierdzono obsługi wszystkich innych typów brył i plików ARD.

## Podłączenie do otwierania ARD

Rekonstrukcja została wydzielona do `modules/ard_import/FeatureGeometry`.
`MainWindow::openFile` uruchamia dla ARD z jedną bryłą `readArdSolid` z nowego
`app/ArdSolidImport.cpp`. Wbudowany driver i nakładki są rozpakowywane do
katalogu tymczasowego. Konfiguracja jest pobierana z istniejących ustawień;
worker działa na prywatnej kopii runtime. GUI weryfikuje powiązanie źródła,
Parasolid i geometrii oraz liczniki i granice, po czym buduje kształt bez STEP.
Nie zastępuje dokumentu przed udanym odczytem i akceptacją pozostałych ograniczeń.

Cztery testy CTest przeszły, w tym wspólna rekonstrukcja i testy kontraktu GUI;
13 testów odczytu Python i 26 regresji backendu przeszło. Sprawdzono kompilację
z eksportem ON i OFF na Linux. Prawdziwy raport v6 przeszedł przez tę samą
`shapeFromReadResult`, z zachowaniem poprawnej bryły, liczników i wymiarów.
Testy konfiguracji stacji Python były pominięte na Linux (wymagają Windows).
Pełny przebieg nowego okna „Otwórz ARD” z natywnym Feature na Windows nadal
wymaga potwierdzenia nową kompilacją. Nie deklarować zakończonego testu Windows.
