# Test 3.2 — poprawny argument ścieżki STEP

1. Otwórz **ALPHACAM Router 2025** i poczekaj na gotowość programu.
2. Rozpakuj całą NOWĄ paczkę do osobnego folderu.
3. Uruchom **URUCHOM_TEST_3_2.cmd** z głównego folderu paczki.
4. Prześlij utworzony **solidimport_test_3_2_....zip** z folderu `solidimport`.

Twój STEP jest już w środku. Wymagany Python 3 x64; bez dodatkowych pakietów.
Pozostaw foldery `solidimport` i `alphacam_reference` obok siebie.
Nie trzeba przenosić bibliotek ani zmieniać instalacji ALPHACAM-u.

## Wynik 3.1 i poprawka

Test 3.1 potwierdził wątek MFC, inicjalizację z kodem 0 i wywołanie eksportu
STEP bez błędu pamięci. Nie powstał jednak plik Parasolid.

W skrypcie był niepoprawny drugi argument StepInputFile: UTF-16 zamiast
oczekiwanego `char*`. Eksport używa konstruktora CStringW **z tekstu ANSI**,
import MFC ordinal 290, IAT RVA 0x183C8. Przy UTF-16 ścieżka `C:\...` kończyła
się dla tego konstruktora po pierwszym znaku. To odpowiada wczesnemu wyjściu
po nieudanym otwarciu wejścia, przed wywołaniem translatora.

W 3.2 argument jest przekazywany jako wąski, zakończony zerem ciąg bajtów,
zakodowany przez Windows `mbcs`, bez zastępowania nieobsługiwanych znaków.
Przed właściwym wywołaniem ta sama oryginalna funkcja MFC konwertuje ścieżkę
na Unicode. Test sprawdza pełną zgodność nazwy, istnienie pliku i jego skrót.
Wynik tego sprawdzenia trafia do **step_argument.json**.

Raport nie traktuje przypadkowej zawartości rejestru EAX po zakończeniu
StepInputFile jako kodu powodzenia translacji. Sukces nadal wymaga przechwycenia
rzeczywistego pliku pośredniego przy granicy importu do rysunku.

Pozostają: wątek MFC, odczyty z rzeczywistej aplikacji, prywatna rejestracja
funkcji, oryginalna inicjalizacja SolidImport, osobny TEMP/TMP i zapis miejsca
błędu natywnego. Test kończy się przed udostępnieniem rysunku do importu.
Normalne zwalnianie DLL, geometria i zapis ARD nadal wymagają osobnych prób.

Limit: **120 sekund**. Prześlij ZIP także przy błędzie lub braku Parasolidu.
Jeżeli pojawi się okno producenta, zachowaj dokładną treść komunikatu.

Walidacja przygotowania: 19 testów przenośnych, w tym odrzucanie UTF-16,
sprawdzenie polskich nazw w CP1250 i odrzucanie sukcesu bez potwierdzonej
ścieżki. Sygnatury importów potwierdzono publicznymi symbolami Microsoftu.
**Natywne wykonanie poprawki 3.2 wymaga jeszcze próby na Twoim Windowsie.**
