# TEST 14.3 — oryginalny zapis rekordu ARD

1. Rozpakuj cały ZIP do nowego folderu na Pulpicie.
2. Zamknij ALPHACAM i pozostaw zamknięty do zakończenia próby.
3. W folderze `step_ard` uruchom `URUCHOM_TEST_14_3_PARASOLID_ARD.cmd`.
4. Prześlij utworzony `step_ard_test_14_3_....zip`.
5. Jeśli pojawi się **UTWORZONO**, otwórz `STEP_TEST_14_3.ard` w ALPHACAM,
   nałóż oryginalny STEP i zapisz `STEP_TEST_14_3_po_otwarciu.ard`.
   Prześlij też ten dokument.

Wynik 14.2 potwierdził jedną bryłę o wymiarach 1200 × 2300 × 10 mm:
63 ściany, 158 krawędzi i 100 wierzchołków. CLS działał i własne połączenie
zamknięto normalnie. Nie powstał ARD: Bodies.Save zwróciło 1, ale oczekiwany
pomocniczy roundtrip.x_b nie istniał, więc poprzedni test zatrzymał się przed
wywołaniem natywnego writera ARD.

Kod oryginalnego backendu zwraca liczbę dopasowanych brył i ignoruje wynik
transmisji Parasolidu. Dlatego sama liczba 1 nie potwierdza istnienia pliku.
Test 14.3 sprawdza oryginalne ObjectQueryWrite2 i ObjectDoWrite2, które tworzą
rekord ARD bezpośrednio z bryły w pamięci. Oddzielny zapis roundtrip pozostaje
diagnostyką; nie jest oznaczany jako udany, gdy brakuje pliku.

Warunki zapisu ARD:

- Rzeczywisty import jednej bryły i pomiar przez API; sześć ustawień wyrównania
  wyłączonych i odczytanych. Wymiary 1200 × 2300 × 10 mm, tolerancja 0,02 mm.
- Prawdziwa bieżąca sesja CLS, pomyślny test argumentów, walidacje 101 oraz
  normalne zamknięcie po zakończeniu procesu Feature. Wynik GetAccessCode(348)
  jest zachowany jako 0/16 wyłącznie na czas oryginalnego Bodies.Save.
- Oryginalne Bodies.Save musi zaakceptować wywołanie i zwrócić 1. Jeżeli DLL
  je odrzuci, writer nie zostanie wywołany. Nie zmieniamy kontroli w DLL.
- Query musi zwrócić 1 i właściwą nazwę obiektu; zapis, flush i close muszą
  zakończyć się kodem 0. Rzeczywiste bajty i hash rekordu muszą odpowiadać logowi.
- Rekord musi zawierać rozpoznany Parasolid binarny i metadane jednej bryły.
  Z rekordu wydzielany jest `native/ard_geometry.x_b`, którego nagłówek i hash
  są sprawdzane osobno. Dopiero potem rekord jest opakowywany w bazę ARD.
- Brak zaobserwowanego Acam.exe, niezmienione wejścia i czyste diagnostyki.

Feature.dll 2025.4.1.200 i pozostałe moduły są nadal sprawdzane przez SHA256.
Domyślny katalog: `C:\Program Files\Hexagon\ALPHACAM 2025`; można użyć `--root`.
Wymagany Python x64. Tabela tekstów ma 585 CString; pięć brakujących tekstów
Feature i siedem z innych katalogów jest jawnie raportowanych jako puste.

Wejściem pozostaje Parasolid z testu 13. Ta próba nie powtarza translacji STEP.
Wymiary i liczniki nie dowodzą pełnej poprawności topologii, kształtu ani
objętości. Miniatura i stare elementy 2D bazy ARD są zachowane. Wykonanie 14.3
na Windows i otwarcie nowego ARD wymagają wyniku tej próby.
