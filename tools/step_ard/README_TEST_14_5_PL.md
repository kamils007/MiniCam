# TEST 14.5 — ścieżka do schematów Parasolidu

1. Zamknij ALPHACAM i pozostaw go zamkniętego do zakończenia testu.
2. Rozpakuj cały ZIP. W folderze `step_ard` uruchom
   `URUCHOM_TEST_14_5_PARASOLID_ARD.cmd`.
3. Prześlij ZIP wskazany w konsoli, niezależnie od wyniku.
4. Dopiero po zakończeniu udanego testu otwórz `STEP_TEST_14_5.ard`
   w ALPHACAM, nałóż oryginalny STEP i zapisz jako
   `STEP_TEST_14_5_po_otwarciu.ard`. Prześlij również ten plik.

Test 14.4 wykazał kod 970 po próbie zapisu. Dokładnie odpowiadający mu
PSKERNEL.dll mapuje go na `KI_schema_access_error`; warstwa PK buduje nazwę
`PK_ERROR_schema_access_error`. To błąd dostępu do pliku schematu.

Oryginalne callbacki Feature.dll szukają schematów w `Schemas` pod ścieżką
dodatku. Pełna inicjalizacja pobiera tę ścieżkę z `IFrame.PathOfThisAddin`;
nasz ograniczony host jej dotąd nie ustawiał. Test 14.5 ustawia ten sam
CString, używając funkcji MFC zaimportowanych przez oryginalną DLL.
Katalog jest wyznaczony z faktycznej lokalizacji Feature.dll:

`C:\Program Files\Hexagon\ALPHACAM 2025\Add-Ins\Feature\Schemas`

Przed importem wymagane są istniejące pliki tego katalogu i pusty pierwotny
CString konfiguracji. Test sprawdza odczyt ustawionej ścieżki, spis i sumy
plików przed i po próbie oraz przywrócenie poprzedniej wartości przed
wyładowaniem DLL. Nie kopiuje schematów do raportu. Nie zmienia plików DLL,
ich kodu, importów ani oryginalnych callbacków. Sesja CLS pozostaje bieżąca.

Poprawiono też odczyt dwóch 32-bajtowych tekstów błędu zapisanych bezpośrednio
w strukturze Parasolidu. Raport powinien teraz pokazać nazwę funkcji i błędu.
Ostatni błąd nie jest kasowany: wynik po writerze może pochodzić z wcześniejszego
Save, dlatego zawsze porównujemy stan przed i po.

Paczka zawiera rzeczywisty Parasolid otrzymany ze STEP-a w teście 13.
Konwersja STEP nie jest tutaj powtarzana. Warunki zatwierdzenia ARD nadal
wymagają poprawnego natywnego rekordu z geometrią i poprawnych pomiarów.
Ustawienie ścieżki jest hipotezą naprawy wymagającą potwierdzenia na Windows.
Pełna topologia, objętość i wizualne pokrycie nie są potwierdzone automatycznie.
