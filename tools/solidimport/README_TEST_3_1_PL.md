# Test 3.1 — wątek MFC i import STEP

1. Otwórz **ALPHACAM Router 2025** i poczekaj na gotowość programu.
2. Rozpakuj całą NOWĄ paczkę do osobnego folderu.
3. Uruchom **URUCHOM_TEST_3_1.cmd** z głównego folderu paczki.
4. Po zakończeniu prześlij nowy **solidimport_test_3_1_....zip**.
   ZIP powstaje w folderze `solidimport`, także przy błędzie.

Twój STEP jest w paczce jako `input_model.stp`. Wymagany Python 3 x64;
bez instalowania dodatkowych pakietów. Folder `alphacam_reference` musi pozostać
obok `solidimport`. Nie trzeba kopiować DLL ani zmieniać ustawień instalacji.

## Co już potwierdził test 3

Raport z 16.10.2026 wskazuje faktycznie uruchomiony ALPHACAM **2025**,
właściwą SolidImport.dll i zakończone InitAlphacamAddIn z kodem **0**.
Wywołanie StepInputFile zgłosiło odczyt spod adresu `0x80`, przed pierwszym
kolejnym wywołaniem hosta. Plik Parasolid nie powstał. Źródło, kopia STEP,
SolidLink.exe i SolidImport.dll zachowały skróty.

Początek eksportu StepInputFile pobiera bieżący CWinThread przez AfxGetThread
i czyta pole pod przesunięciem 0x80, bez kontroli pustego wskaźnika. To wskazuje
na brak kontekstu MFC w zwykłym wątku Pythona; raport nie zawierał adresu
instrukcji błędu, więc test 3.1 ma sprawdzić to rozpoznanie.

## Poprawka

Wywołania SolidImport są teraz wykonywane w wątku utworzonym przez oryginalne
MFC `AfxBeginThread`. Stan MFC jest sprawdzany przed Init oraz przed STEP.
Aplikacja COM, jej ramka i prywatny host są tworzone/uzyskiwane w tym samym
wątku; nie przenosimy surowych interfejsów pomiędzy apartamentami COM.

Sygnatury i numery publicznych eksportów AMD64 zweryfikowano na oficjalnej
MFC 14.44.35211.0 oraz odpowiadającym jej publicznym PDB Microsoftu.
MFC eksportuje je przez numery, bez nazw. Lokalny runtime jest odczytywany
z rzeczywiście załadowanej zależności SolidImport; import AfxGetThread musi
pasować do sprawdzonego eksportu. Informacja o wersji referencyjnej i skróty
są w `mfc_thread_api_reference.json`. Nie dołączamy runtime'u ani PDB do paczki.

Dodano `mfc_calls.jsonl`, `mfc_thread_ready.json` oraz
`native_exceptions.jsonl`. Ostatni log zapisuje instrukcję i stos przy błędzie
natywnym, zachowując zwykłą obsługę wyjątku Windows.

Dalsza ścieżka próby jest taka sama: oryginalna inicjalizacja SolidImport,
rzeczywiste odczyty z uruchomionego ALPHACAM-u, prywatna rejestracja funkcji
oraz zatrzymanie przed zwróceniem jakiegokolwiek rysunku do importu.
TEMP/TMP dotyczą tylko procesu testowego. Nie zapisujemy rysunku ARD w tej próbie.

`parasolid_captured: true` oznacza przechwycony, rozpoznany plik pośredni,
udaną inicjalizację i potwierdzony wątek MFC. Liczba brył, wymiary,
normalne zwalnianie DLL i działanie przy zamkniętym ALPHACAM-ie pozostają
osobnymi etapami weryfikacji.

Limit: **120 sekund**. Po jego przekroczeniu zatrzymywany jest proces testu;
osobny serwer SolidLink nie jest przymusowo zamykany. Prześlij wynikowy ZIP
również wtedy, gdy nie powstanie Parasolid. Przy komunikacie producenta
zapisz także dokładną treść komunikatu.

Przygotowanie sprawdzono 17 testami przenośnymi, kontrolą nagłówków PE
rzeczywistego runtime'u Microsoftu i porównaniem jego publicznych symboli.
**Wykonanie Windows testu 3.1 nadal wymaga sprawdzenia u użytkownika.**

Dokumentacja mechanizmu:
- https://learn.microsoft.com/en-us/cpp/mfc/reference/application-information-and-management
- https://learn.microsoft.com/en-us/cpp/mfc/reference/cwinthread-class
