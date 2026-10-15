# SolidLink 2025 — test 2: STEP → Parasolid

Test dla zarejestrowanego `SolidLink.Document.2025.3` i dokładnego pliku
SolidLink.exe SHA-256
`283a9ce685944a12c66c0be8568e48f23eddc9efe63456deafdf308faf24ee4d`.
Uruchamia translator z istniejącej instalacji. Nie kopiuj EXE poza jego katalog.
Biblioteki producenta nie są dołączone do paczki.

## Uruchomienie

1. Rozpakuj całą paczkę do nowego katalogu z prawem zapisu.
2. Zamknij ALPHACAM i uruchom **URUCHOM_KONWERSJE_STEP.cmd**.
3. W paczce jest już kopia przesłanej `Bryła_4.stp` pod nazwą `input_model.stp`.
   Nie trzeba wybierać modelu. Test pracuje na kolejnej kopii w nowym folderze.
4. Prześlij **solidlink_step_result.json** i **converted.x_t**, jeśli powstał,
   z nowego katalogu `solidlink_test_2_…`.
5. Jeśli wystąpi błąd, dołącz też **worker_stderr.txt** oraz
   **worker_calls.jsonl**, jeżeli istnieją. Wszystkie dane próby są w tym folderze.

Wymagany Python 3 i standardowy Windows PowerShell x64; bez pakietów pip.
Skrypt sam wybiera PowerShell 64-bitowy. Nie zmienia trwałej polityki uruchamiania
PowerShell ani rejestracji COM. Nazwa pliku wynikowego `.x_t` oznacza tekstowy
Parasolid — to format wejściowy obsługiwany przez naszą dalszą próbę Feature.dll.

## Co robi próba

- Sprawdza rejestrację i SHA-256 zainstalowanego translatora.
- Uruchamia COM w osobnym procesie PowerShell z limitem 90 sekund.
- Wywołuje GetFileType, SetOutputLogFile, TranslateSolid(input, output, "Alphacam")
  i GetInteropResult przez zwykłe COM Automation.
- Zachowuje błędy, kody wyników oraz kolejne punkty wykonania.
- Sprawdza utworzony plik i nagłówek Parasolid; porównuje skróty oryginału,
  kopii wejściowej i zainstalowanego EXE przed i po próbie.

Używa normalnej obsługi uprawnień translatora. Nie wywołuje SendLicenseString
ani ConfirmParasolidLicensed i nie podaje własnych wartości autoryzacji.
Jeśli zwykłe wywołanie nie wystarczy do konwersji, raport zachowa wynik.
Po limicie czasowym kończony jest proces próbny PowerShell; osobny serwer
SolidLink nie jest przymusowo kończony.

**To nowy test na Windows; jego powodzenie nie zostało jeszcze potwierdzone.**
Kod 0 z TranslateSolid i poprawny nagłówek potwierdzą utworzenie pliku
Parasolid. Dopiero dalszy import przez Feature.dll i porównanie geometrii
potwierdzą liczbę brył, wymiary i zgodność z wejściem. Ten test nie tworzy ARD.
