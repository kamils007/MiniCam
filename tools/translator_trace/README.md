# Ślad uruchamiania konwertera SolidLink

1. Rozpakuj paczkę i uruchom `TRACE.cmd`.
2. Gdy pojawi się komunikat „TERAZ”, otwórz STEP w ALPHACAM tak jak zwykle.
3. Po 60 sekundach prześlij `raport_translatora.zip` z nowego folderu `wynik_...`.

Test obserwuje procesy przez WMI/CIM i odczytuje wybrane pola konfiguracji
`ie_config.cfg`, jeśli ustali jej położenie z argumentów procesu. Nie wykonuje
sam konwersji, nie ładuje DLL, nie zapisuje konfiguracji instalacji i nie steruje
ALPHACAM. Nie kopiuje EXE/DLL, modeli ani pełnej konfiguracji. Bypass dotyczy
wyłącznie uruchamianego procesu PowerShell. Nie zmienia polityki systemowej.

To obserwacja przez próbkowanie, nie debugger i nie kompletne przechwycenie
wywołań. Bardzo krótki proces, argumenty bezwzględne bez cudzysłowów ze spacjami
lub inny folder konfiguracji mogą nie zostać rozpoznane. Brak rekordu nie dowodzi
braku translatora. Nie potwierdzamy na podstawie samego śladu obsługi eksportu
STEP/IGES ani poprawności geometrii. Test wymaga Windows PowerShell x64 i CIM;
w chmurze sprawdzono zawartość paczki, ale nie wykonano natywnego monitorowania.
