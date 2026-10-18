# Test 7.2 — CLS + zainstalowany SolidLink 2025.4

1. Zamknij wszystkie wersje ALPHACAM.
2. Rozpakuj paczkę do nowego folderu.
3. Uruchom `URUCHOM_TEST_7_2_STEP_CLS.cmd` i pozostaw ALPHACAM zamknięty.
4. Prześlij utworzony `cls_solidlink_test_7_2_...zip` z podfolderu `solidimport`.

W środku jest kopia wcześniej przesłanego STEP-a. Próba działa na kolejnej
kopii roboczej i korzysta z obecnej instalacji SolidLinka, bez kopiowania
całego pakietu i bez zmiany jego konfiguracji.

## Dlaczego poprzednia próba się zatrzymała

Test 7.1 sprawdzał SolidLink 2025.3. Użytkownik wskazał aktualną instalację:
`C:\Program Files\Common Files\Hexagon\SolidLink 2025.4\code\bin\SolidLink.exe`.
Brak wcześniejszej wersji nie jest wynikiem odmowy CLS ani konwersji.

## Co sprawdza wersja 7.2

- Odczytuje klasę COM przez `SolidLink.Document.2025.4`.
- Wymaga, aby LocalServer32 tej klasy wskazywał dokładnie podany plik EXE;
  rejestracja InprocServer32 lub przekierowanie na inny plik przerywa próbę.
- Zapisuje skrót SHA-256 nowego EXE i sprawdza go ponownie w procesie próby
  oraz po zakończeniu. Nie używa skrótu innej wersji 2025.3.
- Odczytuje wersję pliku i wymaga wersji 2025.4.
- Przed aktywacją serwera odczytuje z pliku bibliotekę typów przez
  LoadTypeLibEx z REGKIND_NONE i sprawdza cztery używane publiczne metody:
  TranslateSolid, GetFileType, SetOutputLogFile i GetInteropResult.
- Dopiero potem otwiera i waliduje rzeczywistą sesję CLS znaną z testu 6.2,
  wywołuje zwykłą translację SolidLink, zwalnia COM, ponownie waliduje CLS
  i zamyka własne połączenie.

To publiczne COM Automation. Nie są używane prywatne adresy funkcji, układy
pamięci ani implementacja 2025.3 w nowym pliku. Trzy moduły CLS nadal muszą
odpowiadać wersjom potwierdzonym w udanym teście 6.2.

Próba nie wstrzykuje ciągów licencyjnych, nie wymusza ParasolidLicensed,
nie pobiera kodów dostępu i nie rejestruje serwera. Nie modyfikuje rejestru,
zegara, konfiguracji licencji ani plików producenta. Nie tworzy ARD.

## Wynik

Sesja CLS i konwersja mają oddzielne wyniki. Sukces konwersji wymaga
TranslateSolid=0, nowego pliku z rozpoznanym nagłówkiem Parasolid, zgodności
wejścia i modułów, potwierdzenia kontraktu publicznego API, zwolnienia COM
oraz poprawnej sesji CLS przed i po translacji i jej zamknięcia bez błędu.

Sama sesja CLS nie potwierdza przekazania uprawnień do procesu SolidLink.
To właśnie sprawdza ta próba. Nie potwierdza Core:Solids ani uprawnienia
ALPHACAM do konkretnego wydania; geometria wyjściowa wymaga osobnej kontroli.

Acam.exe jest sprawdzany przed, podczas i po próbie. Istniejący SolidLink.exe
przed próbą przerywa test, aby nie wykorzystywać wcześniejszego kontekstu.
Limit to 90 sekund; w razie przekroczenia kończony jest tylko PowerShell tej
próby, a zamknięcie CLS/COM pozostaje wtedy niepotwierdzone. Inne procesy
nie są zamykane siłowo.

ZIP wynikowy zawiera własne raporty i logi oraz converted.x_t, jeśli powstał.
Nie zawiera plików licencji, bibliotek producenta ani zawartości katalogu TEMP.

Wymagane Windows, Python x64 i zainstalowane komponenty. Bez dodatkowego pip.
Inną ścieżkę do tego samego wydania można wskazać przez `--solidlink`,
folder ALPHACAM przez `--root`, a inny STEP przez `--model`.

Sprawdzono przenośnie logikę rejestracji i warunki odrzucania błędnych wyników,
składnię Python/PowerShell/C# oraz kontrakty względem wcześniej dostarczonej
biblioteki typów. Nowy interfejs 2025.4 i wykonanie tej próby na Windows
muszą zostać potwierdzone nowym raportem użytkownika.
