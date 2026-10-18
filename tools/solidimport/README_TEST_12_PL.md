# Test 12 — rzeczywiste komunikaty z katalogów PO

Zamknij ALPHACAM, rozpakuj cały ZIP i uruchom
`URUCHOM_TEST_12_KATALOG_TEKSTOW.cmd`. Prześlij ZIP
`solidimport_text_catalog_test_12_...zip`, którego ścieżkę wypisze test.
Wymagania: Windows, Python x64, ta sama instalacja ALPHACAM 2025.4.

Skrypt szuka dokładnie `SolidImport.po` i opcjonalnie `aedit.po` w katalogu
instalacji. Odczytuje rzeczywiste tłumaczenia UTF-8 w kontekście katalogu,
np. `solidimport|`, klucz `10-1`. Preferuje język `pl-PL`; jeśli istnieje
tylko jeden jednoznaczny inny katalog, raport zapisze wybrany język.
Różniące się katalogi tego samego języka powodują zatrzymanie z raportem.

Opcjonalne argumenty uruchamiacza:

- `--root "D:\Hexagon\ALPHACAM 2025"` — inna instalacja.
- `--catalog-dir "D:\folder\Polish"` — konkretny folder z katalogami językowymi.
- `--language en-US` — inny jawnie wybrany język.

Test 11 potwierdził przejście oryginalnego importera przez licencję i dwie
badane flagi SpecialKey. Ten test zachowuje ten przebieg i dodaje usługę
ReadTextFile, zwracającą rzeczywiste teksty do oryginalnego InitAlphacamAddIn.
Nie podstawia treści przykładowych, numerycznych identyfikatorów ani pustych
tekstów przy braku tłumaczenia. Pomija niezatwierdzone wpisy `fuzzy` i formy
mnogie; duplikaty klucza są błędem. Brak katalogu lub wpisu zostaje zapisany.

Prywatny host może obsłużyć żądanie `aedit.ctx` przez rzeczywisty wpis o tym
samym numerze w znalezionym `aedit.po`. To jawna mapa zasobów tego hosta,
nie implementacja dekodowania CTX ani dowód pełnej zgodności z czytnikiem
ALPHACAM. Bez odpowiedniego wpisu próba zatrzymuje się. Inne nazwy katalogów
i inne nieobsługiwane metody hosta nadal kończą proces kontrolowanym stopem.

Raport rozdziela przyjęcie tekstów przez oryginalny importer od rzeczywistego
zakończenia InitAlphacamAddIn. Pełny powrót inicjalizatora wymaga kodu 0.
Kontrolowany stop 80 po odczytach może potwierdzić obsługę katalogu, lecz
nie zakończenie inicjalizacji. Przy każdym zwróconym tekście zapisywane są
identyfikator i SHA256 wartości oraz pliku źródłowego; surowe teksty nie trafiają
do ZIP raportu. Źródła są sprawdzane ponownie przed odczytami i po próbie.

Kontrola CLS, profil CORE=14, projekcja SpecialKey 0x12 oraz rozróżnienie
NotOptional=202 i indywidualnego zwolnienia 201/207 pozostają jak w teście 11.
Nie jest to ustalenie pełnego SpecialKey ani poziomu licencji całej aplikacji.
Proces Acam.exe ma pozostać zamknięty; kontrolowane są próbki procesów i skróty
pięciu plików instalacji. Limit procesu: 90 s; po wymuszonym zakończeniu
sprzątanie CLS nie jest potwierdzone. Normalne wyładowanie DLL niezweryfikowane.

Test nie wywołuje StepInputFile i nie tworzy Parasolidu ani ARD. Nie zmienia
instalacji. ZIP zawiera wyłącznie własne raporty i logi, bez bibliotek producenta,
plików licencji, źródłowych katalogów PO i zawartości TEMP. Nowe źródła C#/COM
wymagają jeszcze rzeczywistej próby na Windows.
