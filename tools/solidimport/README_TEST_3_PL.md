# Test 3 — SolidImport, pełna inicjalizacja i STEP

1. Otwórz ALPHACAM Router 2025. Poczekaj, aż będzie gotowy do pracy.
2. Rozpakuj CAŁĄ paczkę ZIP do osobnego folderu.
3. Wejdź do `solidimport` i uruchom `URUCHOM_TEST_3.cmd`.
4. Po zakończeniu prześlij utworzony plik `solidimport_test_3_....zip`.
   Powstaje obok folderu wyników, także przy nieudanej próbie.

STEP jest już w paczce jako `input_model.stp`. Wymagany Python 3 x64;
bez dodatkowych pakietów pip. Pozostaw folder `alphacam_reference` obok
`solidimport` — skrypty korzystają z jego hosta COM.

## Co zmienia ta próba

Test 2 uruchomił SolidLink i rozpoznał STEP (typ 11), ale TranslateSolid
zwrócił 1, bez pliku wynikowego. Analiza tego pliku wykonywalnego wskazuje
wczesny powrót przed translacją, gdy sesja komponentu nie została przygotowana.
To wyjaśnienie wymaga teraz sprawdzenia pełnej ścieżki producenta.

Test 3 ładuje zainstalowaną, sprawdzoną skrótem SolidImport.dll i wywołuje
jej eksporty InitAlphacamAddIn oraz StepInputFile. Zachowuje oryginalną
inicjalizację translatora. ProgramLevel, SpecialKey i ewentualne License
pochodzą z rzeczywiście uruchomionego ALPHACAM-u. Dane licencyjne nie są
zapisywane w raporcie. Rejestracja funkcji importu/eksportu trafia do prywatnego
hosta testu. Oryginalna metoda Frame.ReadTextFile służy do odczytu komunikatów.

Próba zatrzymuje osobny proces przy żądaniu ActiveDrawing po translacji,
PRZED zwróceniem wskaźnika rysunku. Zachowuje wtedy plik pośredni Parasolid.
Do działającego ALPHACAM-u przekazywane są wyłącznie wymienione odczyty;
rysunek i rejestracja funkcji UI nie są przekazywane. TEMP/TMP zmienia się
wyłącznie w procesie testu i wskazuje nowy folder wyników.

`parasolid_captured: true` wymaga udanej inicjalizacji, kontrolowanego
zatrzymania w tym punkcie, zgodności kopii, pełnego nagłówka Parasolid
z danymi oraz niezmienionych plików źródłowych i DLL/EXE. To nie potwierdza
jeszcze liczby brył, wymiarów, zapisu ARD ani pracy przy zamkniętym ALPHACAM-ie.
Normalne zwalnianie biblioteki również nie jest przedmiotem tej próby.

Limit wynosi 120 sekund. Przy przekroczeniu zatrzymywany jest proces testowy;
osobny serwer SolidLink nie jest przymusowo zamykany. Prześlij ZIP także wtedy,
gdy widzisz błąd, brak pliku albo komunikat producenta. Jeśli komunikat wymaga
kliknięcia, zapisz jego dokładną treść.

Jeśli automatyczne wyszukanie DLL zawiedzie, można wskazać ją jawnie:

```bat
URUCHOM_TEST_3.cmd --dll "C:\Program Files\Hexagon\ALPHACAM 2025\Add-Ins\SolidImport\SolidImport.dll"
```

Ścieżka powyżej jest przykładem, nie potwierdzoną lokalizacją instalacji.
Natywne wywołania zostały przygotowane na podstawie analizy dostarczonego
wydania DLL. Test Windows nadal wymaga wykonania na komputerze użytkownika.
