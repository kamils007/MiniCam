# Test 6.1 — prawdziwa sesja CLS bez uruchomionego ALPHACAM-a

1. Zamknij wszystkie wersje ALPHACAM.
2. Rozpakuj paczkę do nowego folderu.
3. Uruchom `URUCHOM_TEST_6_1_CLS.cmd` i pozostaw ALPHACAM zamknięty.
4. Prześlij utworzony `cls_test_6_1_...zip` z podfolderu `solidimport`.

**Ta próba sprawdza otwarcie i walidację rzeczywistego połączenia CLS.
Nie sprawdza jeszcze Core:Solids ani konwersji STEP → Parasolid → ARD.**

## Poprawka po wyniku testu 6

Test 6 z 17.10.2026 zatrzymał się przed wywołaniem SetClsPath i przed
otwarciem połączenia CLS. Skrypt szukał Product jako typu najwyższego poziomu.
Tabela NestedClass oryginalnej biblioteki potwierdza, że Product, ShowError
i oba typy wyników są publicznymi typami zagnieżdżonymi w PlanitCls.License.
Test 6.1 używa GetNestedType i pełnych nazw przy weryfikacji sygnatur metod.
Nie zmienia plików producenta ani zasad uznania połączenia za poprawne.

## Co wykonuje

LicManaged.dll 2025.1 udostępnia publiczną klasę PlanitCls.License.
Próba korzysta tylko z jej metod publicznych, w osobnym procesie Windows
PowerShell x64 / .NET Framework 4. Wersja każdej z trzech bibliotek jest
sprawdzana skrótem przed wywołaniem.

- Ustawia folder zainstalowanego CLS przez SetClsPath.
- Odczytuje bieżący identyfikator sesji przez GetCurrentSessionId.
- Wywołuje OpenVersionedLicenseConnection jako `MiniCam CLS diagnostic`,
  dla rzeczywistego produktu Alphacam i wersji 2025.3, z trybem komunikatów Silent.
- Po oryginalnym wyniku Ok sprawdza ValidLicenseConnection i GetClsProduct.
- Zamyka własne połączenie przez CloseLicenseConnection, także po błędzie otwarcia.

Mogą zostać użyte normalne procesy lub usługi CLS obsługujące licencję.
Test nie aktywuje aplikacji ALPHACAM przez COM, a obecność Acam.exe jest
sprawdzana przed próbą, w trakcie i po niej. Obserwacja procesu unieważnia wynik.

Próba używa istniejącej konfiguracji licencji. Nie aktywuje ani nie importuje
licencji, nie zmienia serwera i nie wywołuje GetAccessCode, ReleaseAccessCode,
CloseAllLicenseSessions ani funkcji aktualizacji. Nie buduje zastępczego ILicense
i nie wpisuje wyniku true dla Core:Solids.

ZIP wynikowy zawiera tylko raporty i logi próby. Nie zawiera plików producenta,
plików licencji ani zawartości katalogu TEMP utworzonej przez CLS.

## Znaczenie wyniku

Oryginalne enumy mają OpenLicenseConnectionResult.Ok = **1** i
ValidLicenseConnectionResult.Ok = **101**. Nie są to HRESULT-y; kod 0 lub samo
zakończenie PowerShella nie oznacza sukcesu. ExistingConnection również nie
jest traktowane jako potwierdzenie świeżego połączenia tej próby.

`cls_session_confirmed = true` wymaga zgodnych wyników obu metod, produktu,
wersji żądania, niezmienionych bibliotek, braku zaobserwowanego Acam.exe
i powrotu z zamknięcia połączenia bez błędu. Nie dowodzi zwolnienia stanowiska
po stronie serwera ani uprawnienia do konkretnej funkcji produktu.

Po limicie 90 sekund kończony jest wyłącznie proces tej próby. Wtedy poprawne
zamknięcie sesji pozostaje niepotwierdzone. Nie są zamykane inne procesy.

Domyślna instalacja: `C:\Program Files\Hexagon\ALPHACAM 2025`.
Inny folder można wskazać przez `--root` w wywołaniu CMD.
Wymagany Python x64; dodatkowe pakiety pip nie są potrzebne.

## Walidacja przygotowania

Sygnatury metod i wartości enumów odczytano statycznie z dostarczonej,
niezmienionej LicManaged.dll. Opis jest w cls_managed_api_reference.json.
Sprawdzono przenośnie warunki odrzucania fałszywego sukcesu. Natywne wykonanie
Windows i zamknięcie rzeczywistej sesji wymagają wyniku użytkownika.
