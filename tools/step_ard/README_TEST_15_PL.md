# TEST 15 — STEP → Parasolid → ARD w jednym uruchomieniu

1. Rozpakuj cały ZIP.
2. Zamknij ALPHACAM. Zamknij też SolidLink, jeżeli pozostał uruchomiony po
   poprzedniej próbie. Oba programy muszą być zamknięte na początku testu;
   skrypt sam uruchomi potrzebny SolidLink.
3. W folderze `step_ard` uruchom `URUCHOM_TEST_15_STEP_ARD.cmd`.
   Dwuklik przetworzy Twój oryginalny STEP z paczki — to te same bajty co
   `Bryła_4.stp`. Aby wskazać inny model, przeciągnij jego plik `.stp`/`.step`
   na starter CMD w rozpakowanym folderze.
4. Poczekaj na zakończenie obu etapów przy zamkniętym ALPHACAM-ie.
5. Prześlij **jeden wynikowy ZIP** wskazany przy `PRZESLIJ ZIP:`.
6. Po powodzeniu otwórz utworzony `STEP_TEST_15.ard` w ALPHACAM, porównaj go
   z wybranym STEP-em i zapisz jako `STEP_TEST_15_po_otwarciu.ard`.

Wymagania: Windows, Python x64, ta sama sprawdzona instalacja ALPHACAM
2025.4 i SolidLink 2025.4 co w udanych testach 13 oraz 14.5.
Nie trzeba uruchamiać ALPHACAM-a do samej konwersji.

Pierwszy etap tworzy nowy Parasolid z wybranego STEP-a, wykorzystując oryginalne
SolidImport/SolidLink i bieżącą sesję CLS. Po potwierdzeniu pliku i zamknięciu
tej sesji drugi etap otwiera własną bieżącą sesję CLS, importuje dokładnie ten
Parasolid do Feature.dll i zapisuje natywny rekord geometrii oraz ARD.
Folder `Schemas` jest konfigurowany tak jak w udanym teście 14.5.
W paczce **nie ma gotowego Parasolidu z wcześniejszych testów**.

Każdy przebieg tworzy osobny folder wyników. Nie nadpisuje STEP-a ani plików
instalacji. Raport obejmuje oba etapy, sumy plików, pomiary bryły, błędy
Parasolidu i stan zamknięcia sesji CLS. Prywatne pliki tymczasowe, DLL i cache
licencji nie trafiają do wynikowego ZIP-a. ARD jest uznany za gotowy tylko po
przejściu kontroli całego przebiegu.

Zakres tej wersji: **jedna bryła**. Złożenia i modele wielobryłowe zatrzymają
test bez zatwierdzenia ARD. Dla znanego `Bryła_4.stp` automatycznie sprawdzane
są wymiary 1200 × 2300 × 10 mm. Inne modele mają zapisywane rzeczywiste pomiary;
nie oznacza to porównania z niezależnie zmierzonym źródłowym STEP-em.
Oczekiwane wymiary można opcjonalnie podać przez CLI:

```text
python build_step_ard_15.py "C:\modele\detal.step" --expected-size 100 200 18
```

ARD nadal korzysta ze sprawdzonego dokumentu bazowego `1_test.ard`; jego
dotychczasowe rekordy 2D są zachowywane. To test konwersji geometrii,
nie pełna integracja interfejsu MiniCam ani uniwersalny zapis całego formatu ARD.
Pełna topologia, objętość i otwarcie nowego pliku wymagają osobnej kontroli.
Oba etapy były wcześniej potwierdzone osobno. To pierwszy wspólny przebieg,
którego działanie na Windows musi potwierdzić wynik tego testu.
