# Test 11 — dwie flagi SpecialKey z rzeczywistego CLS

Zamknij ALPHACAM, rozpakuj cały ZIP i uruchom `URUCHOM_TEST_11_SPECIAL_KEY.cmd`.
Prześlij ZIP `solidimport_special_key_test_11_...zip`, którego ścieżkę wypisze test.
Wymagania: Windows, Python x64, ta sama instalacja ALPHACAM 2025.4 co w teście 10.
Inny katalog instalacji: `--root "D:\inna\lokalizacja"`. Limit: 90 sekund.

Test 10 potwierdził, że oryginalny importer przyjmuje odpowiedź adaptera CLS.
Teraz oryginalny InitAlphacamAddIn dostanie również dwie flagi, które odczytuje
z SpecialKey. Dla tego konkretnego pliku DLL inicjalizator czyta SpecialKey
raz i sprawdza wyłącznie maskę 0x12 (bity 2 i 16). Kontrolowany jest SHA256
całej biblioteki i dodatkowo bajty instrukcji sprawdzającej tę maskę.

Mapowanie z bieżącego Acam.exe i jego dostawcy CLS:

| Flaga | Rzeczywiste zapytanie publicznego CLS |
| --- | --- |
| 16 | GetAccessCode(348) |
| 2 | GetAccessCode(347), a tylko po false — GetAccessCode(349) |

To konkretne zapytania znalezione w kodzie producenta, w tej samej kolejności
i z tym samym krótkim warunkiem OR. Każda odpowiedź, również false, trafia do
obliczenia flag bez zmiany. CLS jest walidowane przed i po każdym zapytaniu.
Zapytania wykonują się dopiero wtedy, gdy oryginalny importer odczyta SpecialKey,
na tym samym wątku MFC/STA, po rzeczywistym pozytywnym sprawdzeniu Core:Solids.

**Jest to projekcja dwóch flag używanych przez Init, nie pełna wartość SpecialKey.**
Pozostałe bity pozostają nieustalone i nie są wystawiane jako potwierdzone.
Nie używamy historycznej wartości 1. Ta implementacja nie nadaje się do dowolnych
klientów API ani innych wydań importera. Nie wywołuje StepInputFile.
Nie potwierdza bieżącego poziomu pełnej aplikacji: CORE=14 jest jawnym profilem
hosta diagnostycznego, prowadzącym przez oryginalną kontrolę Core:Solids.

Raport rozdziela przyjęcie dwóch flag przez oryginalny importer i rzeczywisty
powrót z InitAlphacamAddIn. Nieobsługiwana usługa hosta nadal zatrzymuje test
po zapisaniu nazwy i zamknięciu własnego połączenia CLS. Przewidywaną kolejną
granicą jest ReadTextFile; wtedy zapisujemy tylko nazwę katalogu tekstów,
numer sekcji i indeks. Nie zwracamy fikcyjnego tekstu. Kontrolowany stop 80
może potwierdzić dwie flagi, ale nie pełne zakończenie inicjalizatora.

Wyniki wszystkich przyznanych zapytań są osobno przekazywane do ReleaseAccessCode,
a następnie zamykana jest własna sesja CLS. NotOptional=202 nie jest indywidualnym
zwolnieniem; pozostaje jawnie odrębne od 201/207. Nie ma potwierdzenia zwolnienia
stanowiska po stronie serwera. Po limicie czasu sprzątanie jest niepotwierdzone.

Nie uruchamia Acam.exe, nie zmienia bibliotek producenta i nie odczytuje materiału
autoryzacji SolidLink. ZIP raportu zawiera tylko własne raporty i logi, bez modułów
producenta, plików licencji i zawartości TEMP. Kontrola obejmuje pięć skrótów plików
i próbki procesów. Host COM pozostaje częściowy, normalny unload DLL niepotwierdzony.
Test nie tworzy Parasolidu ani ARD. Uruchomienie nowych źródeł na Windows pozostaje
do sprawdzenia; lokalna weryfikacja nie jest dowodem wykonania nowej próby.
