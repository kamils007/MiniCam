# TEST 14.2 — poprawka przekazywania argumentów CLS

1. Rozpakuj cały ZIP do nowego folderu na Pulpicie.
2. Zamknij ALPHACAM i pozostaw zamknięty do zakończenia próby.
3. W folderze `step_ard` uruchom `URUCHOM_TEST_14_2_PARASOLID_ARD.cmd`.
4. Prześlij utworzony `step_ard_test_14_2_....zip`.
5. Jeżeli pojawi się **UTWORZONO**, otwórz `STEP_TEST_14_2.ard`
   w ALPHACAM. Nałóż oryginalny STEP i zapisz
   `STEP_TEST_14_2_po_otwarciu.ard`. Prześlij też ten dokument.

Test 14.1 potwierdził zgodność wszystkich plików, ale zatrzymał się przed
otwarciem sesji CLS i przed uruchomieniem Feature. PowerShell przekazał
opakowany PSObject zamiast System.String do SetClsPath.

W 14.2 ścieżka ma jawny typ string. Mały adapter C# usuwa opakowania PSObject
z argumentów refleksji i wymaga dokładnych typów zadeklarowanych w publicznym
API. Nie zmienia wartości ani wyników CLS. Przed SetClsPath uruchamia test
opakowanego tekstu, enum i liczby; niewłaściwy typ enum jest odrzucany.
Raport podaje wynik tego testu, nazwę wywołania API i numer linii przy błędzie.

Nadal obowiązuje dokładnie Feature.dll 2025.4.1.200 oraz wcześniej sprawdzone
moduły CLS i Acam.exe. Standardowy katalog to
`C:\Program Files\Hexagon\ALPHACAM 2025`; inny można podać przez `--root`.
Wymagany Python x64. W tym teście nie należy uruchamiać ALPHACAM przed końcem.

Wejściem jest rzeczywisty Parasolid z testu 13. Nie powtarzamy translacji STEP.
Po otwarciu zwykłej sesji CLS odczytywany jest rzeczywisty GetAccessCode(348).
Wynik false/true pozostaje zachowany jako maska 0/16 wyłącznie na czas
oryginalnego Bodies.Save. Pełne SpecialKey nie jest odtwarzane. Sesja pozostaje
otwarta przez czas procesu Feature i jest normalnie zamykana.

Feature ma odczytać jedną bryłę, Min/Max XYZ oraz liczniki ścian, krawędzi
i wierzchołków. Wyłącza sześć ustawień wyrównania przez publiczne API.
Wymiary mają odpowiadać 1200 × 2300 × 10 mm z tolerancją 0,02 mm, bez
dopasowywania skali. Oryginalny zapis Parasolidu i natywnego rekordu musi się
powieść, zanim powstanie ARD. Pełny kształt, topologia i objętość nadal wymagają
osobnego sprawdzenia; zgodność wymiarów ich nie dowodzi.

Nowy profil tabeli 585 CString i adresów DLL jest ten sam co w 14.1. Pięć
brakujących tekstów Feature i siedem tekstów innych katalogów pozostają jawnymi,
poprawnymi pustymi CString; komunikat postępu jest obecny. Miniatura i stare
elementy 2D bazowego ARD są zachowane i nie potwierdzają nowej bryły.

Test nie zmienia instalacji ani rejestru. Wynikowy ZIP zawiera znane raporty,
logi i wyniki. Nie pakuje instalacyjnych DLL ani prywatnego TEMP. Limit Feature
wynosi 90 sekund. Pakiet przeszedł sprawdzenie składni i testy przenośne;
nowy adapter argumentów i zapis ARD wymagają wyniku wykonania na Windows.
