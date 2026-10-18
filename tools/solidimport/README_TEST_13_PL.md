# Test 13 — STEP → Parasolid przez oryginalny importer

Zamknij ALPHACAM. Jeśli działa SolidLink.exe, zamknij go przed nową próbą.
Rozpakuj cały ZIP i uruchom `URUCHOM_TEST_13_STEP_PARASOLID.cmd`.
Prześlij ZIP `solidimport_step_capture_test_13_...zip` wskazany na końcu.

Paczka zawiera Twój wcześniejszy STEP jako `input_model.stp`. Skrypt pracuje
na nowej kopii `input/model.stp`, w osobnym folderze wyników. Nie zmienia
oryginału. Inny model: `--model "D:\detal.stp"`. Domyślny limit: 120 sekund.
Inna instalacja: `--root "D:\Hexagon\ALPHACAM 2025"`. Parametry katalogów
tekstów jak w teście 12: `--catalog-dir` i `--language`.

Po rzeczywistym InitAlphacamAddIn=0, na tym samym wątku MFC/STA, test wywoła
oryginalny StepInputFile. Korzysta z CLS, dwóch flag SpecialKey i rzeczywistych
tekstów instalacji sprawdzonych w teście 12. Oryginalne biblioteki prowadzą
translację i własną autoryzację SolidLink. Nie wyciągamy ani nie odtwarzamy
materiału autoryzacji i nie wywołujemy prywatnych procedur zamiast eksportów.

Sprawdzana jest rejestracja SolidLink.Document.2025.4, dokładna wersja serwera
i ścieżka faktycznie zaobserwowanego nowego procesu SolidLink.exe. Nie zmieniamy
rejestracji COM ani wersji w konfiguracji. Nie uruchamiamy samego EXE z pominięciem
COM. Przy innej lokalizacji serwera podaj `--solidlink-server "D:\...\SolidLink.exe"`.

StepInputFile wymaga `const char*`. Skrypt sprawdza Windows ACP i zgodność
pełnej ścieżki po konwersji przez oryginalny CString z bieżącej DLL. Jeśli
ścieżka się nie zgadza, próba kończy się przed wywołaniem STEP. W takim przypadku
rozpakowanie do krótkiej ścieżki ASCII, np. `C:\MiniCamTest13`, usuwa problem
nieobsługiwanych znaków, bez zmiany kodowania na chybił trafił.

Po udanej translacji oryginalny importer prosi o ActiveDrawing. Ten host
**nigdy nie zwraca rysunku**. Na tej granicy szuka dokładnie jednego pliku
Parasolid w swoim TEMP i zapisuje `captured.x_t` albo `captured.x_b`, następnie
sprząta własną sesję CLS i kończy proces kontrolowanym kodem 83. Błędy na tej
granicy też są terminalne. Zawartość ogólnego TEMP użytkownika nie jest skanowana.

Wynik wymaga nagłówka Parasolid, danych po nagłówku, zgodności skrótów pliku
przed/po kopiowaniu, niezmienionego STEP-a i jego kopii, niezmienionych plików
instalacji i katalogów PO, właściwego serwera, wątku MFC i zamknięcia CLS.
Nie może istnieć plik Parasolid w roboczym TEMP przed wywołaniem StepInputFile.
Sam powrót eksportu bez przechwyconego pliku **nie oznacza sukcesu**; eksport
nie udostępnia wiarygodnego kodu wyniku translatora w wartości zwracanej.

To kontrola przechwycenia pliku, a nie topologii i wymiarów bryły.
`geometry_verified=false` pozostaje jawne. Test nie tworzy ARD i nie importuje
nic do otwartego rysunku. Kolejnym etapem będzie sprawdzenie Parasolidu i zapis ARD.

InAPICall odpowiada rzeczywistemu wywołaniu przez API. Metody postępu prowadzą
tylko prywatny stan diagnostyczny, bez przekazywania operacji do ALPHACAM.
Pozostałe nieobsługiwane metody we wszystkich slotach bieżących IAlphaCamApp
i IFrame są terminalnymi strażnikami; to nadal częściowy host, nie kompletne
Automation. Pełne SpecialKey, klasyfikacja produktu i normalny unload DLL
pozostają niepotwierdzone. NotOptional=202 nie jest indywidualnym zwolnieniem.

Przekroczenie limitu kończy wyłącznie własny proces testowy; wtedy sprzątanie
CLS jest niepotwierdzone. Skrypt nie zabija procesów producenta. ZIP wyników
zawiera własne raporty/logi i przechwycony model, bez bibliotek producenta,
plików licencji, źródłowych PO i pozostałej zawartości TEMP.
Wykonanie nowych źródeł C#/COM na Windows pozostaje do sprawdzenia.
