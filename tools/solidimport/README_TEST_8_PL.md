# Test 8 — oryginalne zapytanie CLS o uprawnienie 390

ALPHACAM musi pozostać zamknięty. Rozpakuj cały ZIP i uruchom
`URUCHOM_TEST_8_CLS_SOLIDS.cmd`. Prześlij ZIP `cls_solids_test_8_...zip`,
którego pełną ścieżkę wypisze test.

Test korzysta z zainstalowanego ALPHACAM 2025 oraz Pythona x64, tak jak 6.2.
Nie wymaga kopiowania SolidLinka. Ścieżkę instalacji można podać przez
`--root "D:\inna\lokalizacja"`. Limit czasu wynosi 90 sekund.

Próba otwiera rzeczywistą sesję CLS, sprawdza jej ważność, wykonuje dokładnie
jedno publiczne `GetAccessCode(390)`, ponownie sprawdza sesję i zwalnia uzyskane
uprawnienie przez `ReleaseAccessCode(390)`. Potem zamyka własne połączenie.
Odmowa jest zapisywana bez prób innych kodów lub zmieniania kontroli.

Numer 390 wynika z analizy dostarczonego Acam.exe 2025.4.1.104: jedna
z oryginalnych ścieżek wyznaczania Core:Solids odpytuje ten kod. Istnieją także
ścieżki wykorzystujące zapisany stan modułu i inne warunki produktu. Dlatego
sam wynik tego testu nie zastępuje pełnego `ILicense.IsFeatureLicensed`;
odmowa nie dowodzi braku licencji na bryły w normalnej aplikacji.

**Ta próba nie konwertuje STEP i nie tworzy Parasolidu ani ARD.** Nie tworzy
obiektu ILicense, nie wywołuje SendLicenseString ani ConfirmParasolidLicensed.
Udany wynik potwierdza tylko rzeczywiste przyznanie i zwolnienie jednego
uprawnienia CLS w procesie diagnostycznym. Nie potwierdza uprawnienia do
konkretnego wydania ani gotowego samodzielnego hosta importera.

Acam.exe jest odczytywany do porównania SHA-256, bez uruchamiania i ładowania
jego kodu. Test wymaga dokładnie dostarczonej wersji EXE i znanych trzech
modułów CLS. Monitoruje brak Acam.exe oraz niezmienność tych czterech plików.
ZIP raportu zawiera wyłącznie własne raporty i logi, bez plików producenta,
licencji, modeli i zawartości katalogu TEMP. Przekroczenie czasu pozostawia
sprzątanie sesji jako niepotwierdzone.

## Wynik otrzymany 17.10.2026

CLS przyznało 390 (GetAccessCode=true). Indywidualne zwolnienie zwróciło
NotOptional (202), a zamknięcie własnego połączenia wróciło bez wyjątku.
Ścisły zbiorczy wynik przesłanego testu pozostaje niepotwierdzony, ponieważ
nie uzyskano potwierdzenia indywidualnego zwolnienia. To nie jest odmowa
przyznania kodu 390. Nie trzeba powtarzać próby dla samej korekty komunikatu.
