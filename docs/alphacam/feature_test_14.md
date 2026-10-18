# Test 14 — zatrzymany przed wywołaniem Feature

Wynik `step_ard_test_14_20261017_212624_978459.zip` (SHA256
6f8bfdbee45a79bfc317cc09815eb4dd7d050112d2022c2ee53047204cd99151)
zawiera tylko raport: preflight odrzucił inną zainstalowaną Feature.dll.
Jej hash wynosi
5bbfbdbd41c32dde36dfc81db3a7a022b9e3688622f7005130cc207bc6638438.
Pozostałe siedem plików odpowiadało oczekiwanym hashom. Wszystkie osiem
rzeczywistych hashy było identycznych przed i po: `inputs_unchanged=false`
w tym raporcie oznacza niezgodność profilu, nie wykrytą zmianę plików podczas
próby. Nie uruchomiono CLS ani natywnego Feature; ARD nie powstał, pomiary
nie zostały wykonane. Potrzebny jest aktualny zainstalowany plik Feature.dll
do sprawdzenia nowego kontraktu; nie wolno tylko podmienić oczekiwanego hasha.

Poniżej opis przygotowanego testu dla wcześniejszego buildu.

Wejściem jest konkretny captured.x_t z testu 13: 80 273 bajty,
SHA256 bec7767b6fdd940965e79153975ea4db993510dd9abf3b74dabbf19801a0dca2.
Nie jest to historyczny Parasolid z testu 3.2, wykonanego przy otwartym Acam.
Test 14 nie powtarza translacji STEP; sprawdza kolejny etap na tym pliku.

Feature.dll pozostaje przypięta do zanalizowanej wersji 2025.3.0.200,
SHA256 4bd2cbb8606a8a5c9dc5c44b9fb0de1e34ecfb01438b6a1b13898873f6878fec.
Nie zakładamy zmiany jej wersji na podstawie wersji Acam/SolidLink.
Zmiana pliku zatrzymuje próbę przed wywołaniem kodu natywnego.

## Bieżący konsument SpecialKey

Oryginalne implementacje zapisu Bodies/Body Save/SaveACIS w tej Feature.dll
(RVA 0x875e0, 0x877a0, 0x8c330, 0x8c500) odczytują SpecialKey przez slot
0x1c8 i sprawdzają wyłącznie maskę 0x10. Ustawiony bit prowadzi do E_NOTIMPL.
Miejsca maskowania: 0x87694, 0x87854, 0x8c3e4, 0x8c5b4.
Nie każdy call [vtable+0x1c8] w pliku jest odczytem SpecialKey; inne interfejsy
mają własne układy tablic. Nie rozszerzamy tego wniosku na nieznanych konsumentów.

W zanalizowanym Acam.exe 2025.4.1.104 getter SpecialKey (0x33d330) przez
slot dostawcy 0x98 wywołuje 0x20e770, który ładuje kod 348 (0x15c),
następnie rzeczywisty GetAccessCode dostawcy (slot 0x10, 0x63f6a0 → 0x7b53f0).
Acam i trzy moduły CLS muszą odpowiadać dokładnie zapisanym hashom.

Własny proces PowerShell otwiera normalne połączenie publicznego PlanitCls,
wykonuje jeden GetAccessCode(348), waliduje 101 i utrzymuje sesję podczas
pracy Feature. Odpowiedź jest zachowana bez wymuszania: false → 0, true → 16.
Nie przyjmujemy historycznego SpecialKey=1. Tylko na czas oryginalnego
Bodies.Save udostępniana jest ta projekcja. Inne fazy nie mają tej właściwości
i nadal zatrzymują się na nieobsługiwanych żądaniach. Pełne SpecialKey=false.
Nie zmieniamy kodu sprawdzającego licencję, nie odtwarzamy materiału autoryzacji.

Po zakończeniu dziecka wymagane są ValidLicenseConnection=101 oraz normalne
CloseLicenseConnection. Dla true zwalniany jest tylko rzeczywiście przyznany
348. Wynik NotOptional/202 jest osobno oznaczony i nie oznacza indywidualnego
zwolnienia. Awaria dziecka także sygnalizuje właścicielowi sesji zamknięcie.

## Pomiary i granice wyniku

Przez rzeczywisty IDispatch wyłączane i odczytywane jest sześć publicznych
ustawień wyrównania. Po imporcie wymagane są: jedna bryła, faktyczne Min/Max
XYZ oraz dodatnie Faces.Count, Edges.Count i Vertices.Count. Porównanie wymiarów
z 1200 × 2300 × 10 mm używa tolerancji 0,02 mm bez dopasowywania skali.
Jednostki API są jawnie profilem milimetrowym instalacji; rozbieżność zatrzyma
tworzenie ARD i zachowa surowe wartości w logach.

Nie wywołujemy mutującego CheckAndRepair. Liczniki i wymiary nie dowodzą
poprawności topologicznej ani pełnego kształtu; geometry_verified i
volume_verified pozostają false. Po udanym zapisie użytkownik ma nałożyć
oryginalny STEP na nowy ARD i zwrócić zapisany dokument.

Wrapper ARD i natywny writer pochodzą z udanej próby 4.1, ale ich działanie
na nowym pliku musi potwierdzić wynik testu 14. Istniejące 2D i miniatura bazy
są zachowane. Pakiet nie zawiera instalacyjnych DLL; pliki użytkownika i PO
wchodzą tylko do prywatnego ZIP, nie do repozytorium.

Walidacja przygotowania: 10 testów kontrolnych STEP/ARD, 47 testów hosta,
warstw, katalogu, śladu i serializacji; Python kompiluje się, składnia PowerShell
przechodzi parser. Nie jest to wykonanie Windows ani dowód utworzenia nowego ARD.
