# TEST 14.1 — Feature.dll 2025.4.1.200, Parasolid z testu 13 → ARD

1. Rozpakuj cały ZIP do nowego folderu na Pulpicie.
2. Zamknij ALPHACAM i zostaw zamknięty do zakończenia próby.
3. W folderze `step_ard` uruchom `URUCHOM_TEST_14_1_PARASOLID_ARD.cmd`.
4. Prześlij utworzony obok wyników `step_ard_test_14_1_....zip`.
5. Jeżeli konsola pokaże **UTWORZONO**, otwórz `STEP_TEST_14_1.ard`
   w ALPHACAM. Wstaw oryginalny `Bryła_4.stp` do tego samego dokumentu,
   sprawdź pokrycie brył i zapisz `STEP_TEST_14_1_po_otwarciu.ard`.
   Prześlij także ten zapisany dokument.

Ta poprawka obsługuje rzeczywistą przesłaną Feature.dll 2025.4.1.200,
SHA256 5bbfbdbd41c32dde36dfc81db3a7a022b9e3688622f7005130cc207bc6638438.
Sprawdzono jej eksporty, identyczną bibliotekę typów COM, nowy adres tabeli
tekstów, importy CString/CRT, układ kontekstu hosta i oryginalny zapis.
To nie jest samo wyłączenie kontroli hasha. Inna biblioteka zatrzyma test.

Nowa tabela tekstów ma 585 obiektów CString, w tym dwa dodatkowe w grupie 40.
W dostarczonym katalogu PO brakuje pięciu identyfikatorów Feature i siedmiu
tekstów innych katalogów. Mają poprawne puste CString; raport wymienia braki.
Wymagany komunikat postępu importu jest obecny i sprawdzany.

Wymagane: ta sama instalacja i Python x64 co poprzednio. Domyślny katalog:
`C:\Program Files\Hexagon\ALPHACAM 2025`. Inny katalog można wskazać przez
`--root "D:\...\ALPHACAM 2025"`.

Próba używa 80 273 bajtów Parasolidu przechwyconego w teście 13 i sprawdza
jego hash. Nie uruchamia ponownie translacji STEP ani SolidLink.
Wymaga jednej bryły, odczytuje obwiednię XYZ oraz liczniki ścian, krawędzi
i wierzchołków. Sześć ustawień wyrównania jest wyłączanych i odczytywanych
przez API. Wymiary mają odpowiadać 1200 × 2300 × 10 mm z tolerancją 0,02 mm;
nie dopasowujemy skali. Inna jednostka API da raport niezgodności.

Własne zwykłe połączenie CLS pozostaje otwarte podczas pracy Feature.
Rzeczywisty wynik GetAccessCode(348) jest zachowany, false → 0, true → 16.
Tylko na czas oryginalnego Bodies.Save udostępniamy maskę 0x10 sprawdzaną
przez tę DLL. Pozostałe fazy nie mają SpecialKey. Nie odtwarzamy pełnego
SpecialKey ani historycznej wartości 1 i nie zmieniamy kodu sprawdzającego.
Odrzucenie zapisu przez DLL oznacza błąd próby. Warunkiem ARD są także
poprawne zamknięcie własnej sesji CLS i niezmienione wejścia.

Zapis wykorzystuje oryginalne Bodies.Save oraz ObjectDoWrite2. Natywny rekord
jest opakowany w przetestowaną bazę ARD. Jej istniejące elementy 2D i miniatura
pozostają zachowane; nie stanowią dowodu zgodności nowej bryły.
Wymiary i liczniki nie potwierdzają pełnego kształtu, topologii ani objętości.
Pełny świeży STEP → ARD w jednym uruchomieniu pozostaje kolejnym etapem.

Test obserwuje Acam.exe przed, podczas i po próbie. Nie zmienia instalacji
ani rejestru. Ma własny folder roboczy i limit Feature 90 sekund. Wynikowy
ZIP zawiera wyłącznie znane raporty, logi i wyniki, bez instalacyjnych DLL,
prywatnego TEMP i materiału autoryzacji.

Sprawdzono statycznie nowy build oraz testami przenośnymi obsługę obu układów
pamięci. Wykonanie testu 14.1 na Windows i nowy ARD pozostają niepotwierdzone
do otrzymania wyniku tej próby.
