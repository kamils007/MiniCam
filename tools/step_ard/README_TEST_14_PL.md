# TEST 14 — Parasolid z testu 13 → pomiar bryły → ARD

1. Rozpakuj cały ZIP do nowego folderu na Pulpicie.
2. Zamknij ALPHACAM i zostaw go zamkniętego do zakończenia próby.
3. Otwórz `step_ard` i uruchom `URUCHOM_TEST_14_PARASOLID_ARD.cmd`.
4. Prześlij utworzony obok folderu wyników `step_ard_test_14_....zip`.
5. Jeżeli konsola pokaże **UTWORZONO**, dopiero wtedy otwórz
   `STEP_TEST_14.ard` w ALPHACAM. Wstaw oryginalny `Bryła_4.stp` do tego
   samego dokumentu i sprawdź pokrycie brył. Zapisz dokument jako
   `STEP_TEST_14_po_otwarciu.ard` i prześlij także ten plik.

Potrzebna jest ta sama instalacja ALPHACAM 2025 i Python x64 co w poprzednich
testach. Domyślna lokalizacja: `C:\Program Files\Hexagon\ALPHACAM 2025`.
Inny katalog można podać jako `--root "D:\...\ALPHACAM 2025"`.
Próba weryfikuje dokładne wersje przez SHA256; niezgodność trafi do raportu.

## Co wykonuje

- Używa rzeczywistych 80 273 bajtów `captured.x_t` z udanego testu 13.
  W tym teście nie uruchamia ponownie translacji STEP ani SolidLink.
- Otwiera własne zwykłe połączenie CLS. Bieżący wynik `GetAccessCode(348)`
  pozostaje niezmieniony: `false` daje 0, `true` daje 16. Połączenie jest
  utrzymywane do zakończenia procesu Feature i normalnie zamykane.
- Tylko na czas `Bodies.Save` udostępnia maskę 0x10, którą sprawdza ten
  dokładny build Feature.dll. Nie używa starego `SpecialKey=1` i nie twierdzi,
  że odtworzono pełne SpecialKey. Pozostałe fazy nie mają tej właściwości.
  Jeżeli oryginalna DLL odrzuci zapis, test zgłosi błąd.
- Importuje Parasolid przez API Feature.dll, wyłącza sześć ustawień
  automatycznego wyrównania i odczytuje ich faktyczne wartości.
- Wymaga jednej bryły. Odczytuje Min/Max XYZ oraz liczby ścian, krawędzi
  i wierzchołków. Porównuje wymiary z 1200 × 2300 × 10 mm, tolerancja 0,02 mm.
  Nie dopasowuje skali. Inna jednostka API spowoduje raport niezgodności.
- Zapisuje Parasolid przez oryginalne `Bodies.Save`, natywny rekord przez
  `ObjectDoWrite`, a potem opakowuje rekord w przetestowaną bazę ARD.
  Warunkiem utworzenia wyniku są także poprawne zamknięcie CLS, niezmienione
  wejścia i brak zaobserwowanego Acam.exe w próbkach przed, podczas i po próbie.

Zgodność wymiarów i dodatnie liczniki nie są pełnym testem topologii ani
kształtu. Objętość i pełna geometria pozostają niepotwierdzone.
Miniatura i istniejące elementy 2D w bazowym ARD są zachowane; ich wygląd nie
potwierdza nowej bryły. Pełny świeży STEP → ARD w jednym uruchomieniu będzie
osobnym etapem po wyniku tej próby.

Test nie zmienia plików instalacji ani rejestru. Własne pliki robocze powstają
w nowym folderze. Wynikowy ZIP zawiera wyłącznie znane raporty, logi i wyniki,
bez prywatnego TEMP i bibliotek instalacji. Limit procesu Feature: 90 s.

Pakiet przygotowano i sprawdzono statycznie oraz testami przenośnymi.
Wykonanie testu 14 na Windows i otwarcie nowego ARD wymagają wyniku tej próby.
