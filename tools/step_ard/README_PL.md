# Test 4.1 — bryła z Twojego STEP-a do ARD

**Konwersja STEP → Parasolid z testu 3.2 zakończyła się sukcesem.**
Log Spatial InterOp podaje 1 bryłę stałą, bez brył powierzchniowych i krzywych.
Przechwycony `converted.x_t` ma 99 728 bajtów. Zgodność pliku z raportem
sprawdzono skrótem SHA-256. Pełnych wymiarów po konwersji jeszcze nie sprawdzono.

## Uruchomienie

1. **Zamknij ALPHACAM** przed uruchomieniem tego testu.
2. Rozpakuj całą paczkę do osobnego folderu.
3. Uruchom **URUCHOM_TEST_4_1_ARD.cmd** z głównego folderu paczki.
4. Jeśli powstanie `STEP_test.ard`, otwórz go w ALPHACAM **2025**.
5. Sprawdź bryłę, wymiary i położenie; zapisz jako `STEP_test_po_otwarciu.ard`.
6. Prześlij wygenerowany **step_ard_test_4_1_....zip** i zapisany przez ALPHACAM
   plik `STEP_test_po_otwarciu.ard`.

ZIP z raportem powstaje w folderze `step_ard` również przy błędzie.
Próba wymaga Python 3 x64 i zainstalowanej, wcześniej sprawdzonej Feature.dll
2025.3.0.200. Nie wymaga instalowania pakietów pip ani kopiowania DLL.

## Co zawiera i sprawdza paczka

W środku są: Parasolid z Twojego STEP-a, Twój plik bazowy `1_test.ard`,
katalog Feature.po i wcześniejszy rzeczywisty odczyt ustawień hosta 2025.
Snapshot zawiera klasyfikację hosta SpecialKey; nie jest autoryzacją licencji.
Każdy istotny plik i Feature.dll są sprawdzane skrótami.

Używamy sekwencji probe'a Feature 4.9, który wcześniej zaimportował Parasolid,
zapisał binarny Parasolid i fragment `feature_object.bin`. Tym razem wejściem
jest `converted.x_t` z udanej konwersji STEP. Dopiero poprawny import 1 bryły,
zapis i zgodny natywny fragment pozwalają utworzyć ARD.

Fragment jest opakowywany tak samo jak w poprzednim TEST_6: rekord 0x22,
LicomUKRJMParaSolid, liczba obiektów, długość, zero i surowe bajty z DLL.
Pozostałe rekordy Twojego pliku bazowego, w tym miniatura i geometrie 2D,
pozostają zachowane. Dlatego miniatura nie jest potwierdzeniem nowej bryły.

Ten sposób opakowania odtworzył poprzedni TEST_6 bajt w bajt.
Nowy model i wynikowy ARD wymagają dopiero próby Windows oraz otwarcia
w ALPHACAM-ie. Raport odróżnia utworzenie pliku od sprawdzenia geometrii
oraz zaakceptowania dokumentu przez ALPHACAM.

**Etap STEP → Parasolid w teście 3.2 korzystał z otwartego ALPHACAM-u.**
Test 4 sprawdza kolejne etapy przy zamkniętym programie. Samodzielny import
STEP z MiniCam i integracja całej ścieżki w C++ pozostają dalszym zadaniem.

Limit: 90 sekund dla procesu Feature, 150 dla jego uruchomienia i raportu.
Jeżeli ścieżka Feature.dll jest inna, można ją wskazać:

```bat
URUCHOM_TEST_4_1_ARD.cmd --dll "C:\Program Files\Hexagon\ALPHACAM 2025\Add-Ins\Feature\Feature.dll"
```


## Poprawka 4.1 po raporcie Windows

W poprzednim teście proces Feature zatrzymał się kontrolowanym kodem **80**
na **IAlphaCamApp.DisableUndo**, getter VARIANT_BOOL* pod offsetem **0x2B8**.
Było to podczas automatycznego wyrównywania części. Import nie zakończył się,
więc feature_object.bin jeszcze nie mógł powstać.

Host wsadowy nie prowadzi historii cofania. Zgłasza teraz DisableUndo = true
(-1 jako 16-bitowy VARIANT_BOOL), przyjmuje idempotentne wyłączenie przez
setter **0x210** i odmawia włączenia historii, której nie implementuje.
Nie implementujemy operacji Undo ani nie zgłaszamy fikcyjnego importu lub zapisu.

Raportowanie sprawdza przyczynę zakończenia Feature przed odczytem pliku
rekordu. Następna nieobsługiwana metoda będzie wymieniona bezpośrednio
w step_ard_result.json oraz w komunikacie okna.

Sprawdzenie przenośne: 16 testów hosta i 4 testy raportu/opakowania.
Wykonanie Windows wersji 4.1 i wynikowy ARD nadal wymagają próby.
W C++ dodano analogiczną politykę do pomocniczego FeatureHost; tej zmiany
nie kompilowano ani nie wykonywano na Windows w tej sesji.
