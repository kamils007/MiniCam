# Test 14.5 — przywrócenie konfiguracji katalogu Schemas

Status: wykonany w Windows; zapis Parasolidu i natywnego rekordu działa,
ARD utworzony. Użytkownik potwierdził otwarcie w ALPHACAM i dokładne wizualne
pokrycie z oryginalnym STEP-em. Pełna walidacja geometrii pozostaje niepotwierdzona.

Potwierdzenie użytkownika po wskazaniu oryginalnego `Bryła_4.stp`:
„DOKŁĄDNIE SIE POKRYWAJĄ ;-)”. Jest to kontrola wizualna użytkownika;
nie wykonano tu niezależnego porównania objętości ani pełnej topologii.
Plik ponownie zapisany przez ALPHACAM nie został jeszcze otrzymany.

Raport: `step_ard_test_14_5_20261017_221222_282745.zip`, SHA256
`23db03290dfc8c552d2702b8c6a6673081081f7b7ec5f5efd9559cae80ff6211`.
Integralność ZIP i zgodność oryginalnego stdout z raportem sprawdzone.
Wszystkie cztery odczyty ostatniego błędu zwracają query=0, was_error=false
i poprawne osłony buforów. Zniknął błąd 970 obserwowany w 14.4.

Bodies.Save utworzył rzeczywisty roundtrip.x_b: 54 683 bajty, SHA256
`c3f70972fb77aa42cb21c6bac491ccd66fe5a89c16fdcdbf15b5eb8907d6c146`.
ObjectDoWrite2 zwrócił 0, fflush/fclose również 0. Rzeczywisty rekord ma
55 028 bajtów i SHA256
`a2077bb739e3d6986ca1372c6af5535eb97f9fce675b1c43f1817de59803b6a8`.
Zawiera 54 855 bajtów binarnego Parasolidu; wyodrębniony plik jest bajtowo
zgodny z tym zakresem. Oba strumienie mają nagłówek B i klucz
`SCH_3700000_36001`. Są to dwa odrębne zapisy, nie identyczne pliki.

STEP_TEST_14_5.ard: 117 970 bajtów, SHA256
`68636906a8d4839b99a00517b8cf9876f8b340569ce8a796793ec35f47fb1f1a`.
Niezależne ponowne złożenie z bazowego ARD i rzeczywistego rekordu dało
identyczne bajty. Dotychczasowe rekordy bazy i stopka pozostały bez zmian.
Sesja CLS została zweryfikowana i zamknięta, natywnych wyjątków brak,
Acam.exe nie był obserwowany w 38 próbkach. Konfiguracja ścieżki została
przywrócona, a pliki Schemas i sprawdzane pliki instalacji nie zmieniły się.

Pomiary przed zapisem nadal dają jedną bryłę 1200 × 2300 × 10 mm,
63 ściany, 158 krawędzi i 100 wierzchołków. Nie wykonano tu ponownego importu
zapisanego pliku ani pomiaru objętości. Test korzysta z dokładnego wyniku
zamkniętej konwersji STEP z testu 13; nie powtarza całej ścieżki w jednym
uruchomieniu. Otwarcie i pokrycie użytkownik potwierdził powyżej.
Do otrzymania pozostaje plik ponownie zapisany przez ALPHACAM.

Wejściowa biblioteka `pskernel.dll` ma SHA256
`c900fa3f430fe67c2adb15a50f9604d9812ed81a02c5006b66689dac28263073`,
zgodny z faktycznie załadowanym modułem w 14.4. Analiza statyczna:

- `PK_ERROR_ask_last` RVA 0x2571b0 wywołuje czytnik 0x8f7ae0;
  kopiuje on strukturę 0x74 bajtów. Funkcja i code_token to tablice 32 bajtów
  przy 0 i 0x24. Kod znajduje się przy 0x20. Nie są to wskaźniki.
- Konstruktor błędu 0x8fb270 zapisuje kod i buduje code_token przez 0x8f8510.
  Dla 970 ta ścieżka prowadzi przez 0x4d27a0 do tabeli 0x9362a0.
  Inicjalizacja tabeli przy 0x9372e9 zapisuje wskaźnik do
  `KI_schema_access_error` (RVA 0x3fec408) w slocie
  `0x443daa0 + 970*8`. Warstwa PK zamienia prefiks KI_ na PK_ERROR_.
- `PK_PART_transmit_u` RVA 0x3c7c40 przekłada wewnętrzny wynik 8 na
  kod 970 przez tablicę skoków 0x3c8768 i blok 0x3c85d1.

W dokładnym Feature.dll 2025.4.1.200 inicjalizator od RVA 0x3931d zapisuje
`IFrame.PathOfThisAddin` (slot x64 0x88) do globalnego CString przy 0x18a3b8
i dodaje końcowy backslash. Statyczny konstruktor przy 0x12e0 już konstruuje
ten CString. Oryginalne resolvery 0xc3400/0xc34c0 dodają `Schemas`, backslash
i nazwę. Callback 0xc3de0 dla rodzaju 6 korzysta z pierwszego resolvera.
Pominięcie tej części pełnego Init pozostawia pustą bazową ścieżkę.

Nowy adapter NativeSchemaPath dla sprawdzonego SHA Feature używa istniejącego
CString i jego zaimportowanego MFC operator=(wchar_t*). Wymaga pustej poprzedniej
wartości, katalogu Feature/Schemas z plikami, odczytu kontrolnego, niezmienności
plików i przywrócenia poprzedniej wartości po zwolnieniu obiektów COM.
Pełny Init nie jest uruchamiany. Callbacki i kod natywny pozostają oryginalne.
Konfiguracja może usunąć wykryty problem ścieżki; dopiero test Windows pokaże,
czy jest wystarczająca do zapisu. Kod 970 sam nie dowodzi, który plik nie został
otwarty ani czy oprócz ścieżki istnieją inne problemy dostępu.

Nowy czytnik błędów wymaga dokładnego SHA PSKERNEL przed natywnym odczytem.
Teksty są odczytywane z ograniczonych tablic, wyłącznie po query=0/was_error.
Żadne bajty tych tablic nie są dereferencjonowane jako wskaźniki.
Nie są zmieniane opcje transmisji, licencje, IAT ani error handler.

Walidacja lokalna: 62 testy adapterów i 13 STEP/ARD (75 łącznie), py_compile,
kontrola instrukcji i mapowania 970 w dokładnie dostarczonych DLL, integralność
ZIP i jego manifestu, składnia wszystkich zapakowanych plików Python,
13 testów z odizolowanej rozpakowanej paczki i uruchomienie obu CLI z --help.
Nowa konfiguracja natywna nie była tu wykonywana na Windows.
