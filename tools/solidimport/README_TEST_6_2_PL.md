# Test 6.2 — sesja CLS z domyślną kontrolą producenta

1. Zamknij wszystkie wersje ALPHACAM.
2. Rozpakuj paczkę do nowego folderu.
3. Uruchom `URUCHOM_TEST_6_2_CLS.cmd` i pozostaw ALPHACAM zamknięty.
4. Prześlij utworzony `cls_test_6_2_...zip` z podfolderu `solidimport`.

Ta próba nadal sprawdza tylko otwarcie i walidację połączenia CLS.
Nie konwertuje STEP i nie potwierdza uprawnień Core:Solids ani wydania 2025.3.

## Co wykazał test 6.1

Biblioteka została poprawnie wczytana, sygnatury metod były zgodne, a SetClsPath
zwrócił true. OpenVersionedLicenseConnection zwróciło UpdateExpiry (10).
Nie wywołano walidacji sesji. CloseLicenseConnection wróciło bez błędu.
Acam.exe nie był zaobserwowany w 55 próbkach ani przed i po próbie.

## Co zmienia test 6.2

Test korzysta z publicznej metody PlanitCls.License.OpenLicenseConnection
z argumentami checkMonth=-1 i checkYear=-1. Takie domyślne wartości występują
w inicjalizacji dostarczonego Acam.exe 2025. Oryginalna biblioteka zastępuje je
wbudowanymi wartościami kontroli; nie pomija sprawdzenia licencji.

Odczyt statyczny potwierdził te same surowe wartości domyślne (4 i 125)
w dostarczonym LicManaged.dll i Acam.exe. Poprzednia metoda wersjonowana dla
argumentów 2025 i 3 przekazywała do wewnętrznej kontroli inne wartości:
4 i 2525. Znaczenie tej reprezentacji wersji nie jest jeszcze potwierdzone.
Wynik UpdateExpiry sam w sobie nie rozstrzyga o ważności licencji użytkownika.

To pojedyncza próba z domyślnymi wartościami producenta. Nie próbuje kolejnych
lat ani wersji i nie zmienia zegara, licencji, plików producenta ani serwera.

## Warunek potwierdzenia

Połączenie uznajemy za potwierdzone wyłącznie przy oryginalnym Open=Ok (1),
Valid=Ok (101), produkcie Alphacam (2), zgodnym kontrakcie API, niezmienionych
modułach, braku zaobserwowanego Acam.exe i zamknięciu połączenia bez błędu.
Kod 10, ExistingConnection, timeout i samo zakończenie procesu nie są sukcesem.

Własne połączenie jest zamykane również po odmowie otwarcia. Nie są wywoływane
CloseAllLicenseSessions, GetAccessCode, ReleaseAccessCode ani zmiany licencji.
Próba może korzystać z normalnych procesów lub usług obsługujących CLS.
Zamknięcie bez wyjątku nie dowodzi zwolnienia stanowiska po stronie serwera.

Limit to 90 sekund. W razie przekroczenia kończony jest tylko proces testu;
poprawne zamknięcie połączenia pozostaje wtedy niepotwierdzone.

ZIP wynikowy zawiera tylko własne raporty i logi, bez plików licencji,
bibliotek producenta i zawartości jego plików tymczasowych.

Wymagany Windows, Python x64 oraz niezmieniona instalacja ALPHACAM 2025.
Dodatkowe pakiety pip nie są potrzebne. Domyślny folder instalacji:
`C:\Program Files\Hexagon\ALPHACAM 2025`. Inny można wskazać przez `--root`.

Sygnatury, zagnieżdżone typy, wartości domyślne i składnię skryptu sprawdzono
lokalnie. Wykonanie tej wersji na Windows wymaga nowego wyniku użytkownika.
