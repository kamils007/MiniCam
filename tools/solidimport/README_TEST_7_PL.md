# Test 7 — STEP → Parasolid przy otwartej sesji CLS

1. Zamknij wszystkie wersje ALPHACAM.
2. Rozpakuj tę paczkę do nowego folderu.
3. Uruchom `URUCHOM_TEST_7_STEP_CLS.cmd` i pozostaw ALPHACAM zamknięty.
4. Prześlij utworzony `cls_solidlink_test_7_...zip` z podfolderu `solidimport`.

W paczce jest kopia wcześniej przesłanego STEP-a. Oryginał pozostaje bez zmian.
Nie trzeba kopiować ani przesyłać całego SolidLinka. Używana jest jego obecna
instalacja na komputerze. Paczka nie zawiera bibliotek producenta.

## Co sprawdza ta próba

Test 6.2 potwierdził rzeczywistą sesję CLS bez zaobserwowanego Acam.exe.
Test 7 pozostawia taką sesję otwartą i w tym czasie wywołuje zwykłą konwersję
przez publiczny COM SolidLink 2025.3:

1. Sprawdza zgodność trzech modułów CLS i zarejestrowanego SolidLink.exe.
2. Otwiera i waliduje sesję CLS tak samo jak udany test 6.2.
3. Tworzy obiekt SolidLink, rozpoznaje STEP i wywołuje TranslateSolid.
4. Zwalnia obiekt COM, ponownie sprawdza sesję CLS i zamyka własne połączenie.
5. Sprawdza kod konwersji, nowy plik Parasolid i niezmienność wejścia/modułów.

Nie zakładamy, że sama sesja CLS przekazuje uprawnienia do osobnego procesu
SolidLink. Właśnie ten wariant sprawdza próba. Nie dostarcza jeszcze adaptera
ILicense dla SolidImport.dll i nie wywołuje SolidImport.dll ani Feature.dll.
Nie podaje ciągów licencyjnych ani nie wymusza potwierdzenia ParasolidLicensed.

## Znaczenie wyników

Sesja CLS i konwersja są oceniane oddzielnie. Możliwy jest wynik:
sesja potwierdzona, konwersja niepotwierdzona. GetInteropResult=0 lub samo
wyjście procesu z kodem 0 nie wystarcza do uznania konwersji za udaną.

Potwierdzenie konwersji wymaga TranslateSolid=0, nowego pliku z rozpoznanym
nagłówkiem Parasolid, zgodności plików wejściowych i modułów, poprawnego
zwolnienia COM, walidacji CLS przed i po próbie oraz powrotu z zamknięcia CLS
bez błędu. Acam.exe jest sprawdzany przed, podczas i po próbie.

Plik wynikowy wymaga późniejszej weryfikacji geometrii. Ten test nie tworzy
ARD ani nie potwierdza wydania 2025.3 lub Core:Solids.

Jeżeli przed próbą pozostał SolidLink.exe z innej pracy, test przerwie działanie.
Nie zamyka cudzych procesów i nie używa istniejącego obiektu translatora.
Po zakończeniu zapisuje informację o pozostałych procesach SolidLink.

Limit wynosi 90 sekund. Przy przekroczeniu kończony jest tylko PowerShell
tej próby; wtedy zamknięcie połączeń CLS/COM pozostaje niepotwierdzone.
Backend CLS i osobny serwer SolidLink nie są zamykane siłowo.

ZIP wynikowy zawiera własne raporty oraz converted.x_t, jeśli powstał.
Nie zawiera plików licencji, bibliotek producenta, wejściowego STEP-a ani
zawartości plików tymczasowych i logów utworzonych przez producenta.

Wymagany Windows, Python x64 i obecna instalacja ALPHACAM 2025/SolidLink 2025.3.
Nie są potrzebne dodatkowe pakiety pip. Folder ALPHACAM można wskazać przez
`--root`, inny plik STEP przez `--model`. Rejestr i konfiguracja licencji nie
są zmieniane. Test nie pobiera kodów dostępu ani nie zamyka innych sesji CLS.

Sprawdzono lokalnie składnię, warunki odrzucania błędnych wyników i zgodność
publicznych metod z dostarczonymi metadanymi. Wykonanie tej nowej próby na
Windows pozostaje do potwierdzenia wynikiem użytkownika.
