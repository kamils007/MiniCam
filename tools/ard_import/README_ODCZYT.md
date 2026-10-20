# Test rzeczywistego wczytania bryły z ARD

1. Zamknij ALPHACAM na stacji, na której działa zapis ARD z MiniCam.
2. Uruchom `TEST_ODCZYTU_BRYLY.cmd` i wybierz plik ARD z jedną bryłą.
3. Prześlij `raport_odczytu.zip` ze wskazanego folderu `odczyt_...`.

Folder instalacji, Python i runtime są odczytywane z ustawień eksportu MiniCam.
Paczka nie zawiera bibliotek producenta, prywatnego Feature.po ani modelu.
Wymaga dotychczasowego działającego runtime eksportu i normalnej licencji CLS.
Nie zmienia instalacji, rejestru, ustawień eksportu ani polityki PowerShell
na stałe. Zezwolenie PowerShell dotyczy wyłącznie procesu testu.

Test obsługuje rozpoznane rekordy ARD V1.94 z jednym obiektem Feature v3;
wersja Feature 2025.4.1.200 pozostaje przypięta do potwierdzonego skrótu DLL.
Nie jest wymagany niezmieniony testFAz.ard. Czytnik sprawdza strukturę i granice
rekordów oraz końcówkę pliku, bez wyszukiwania znacznika w surowych danych.
Wydziela oryginalne bajty
Parasolid, kopiuje runtime do nowego katalogu i uruchamia istniejącą ścieżkę
inicjalizacji Feature/CLS z dodatkowym trybem tylko do pomiaru. Wyłącza
automatyczne wyrównanie, wymaga jednej bryły, mierzy granice XYZ i odczytuje
Faces.Count, Edges.Count oraz Vertices.Count. Sprawdza zgodność skrótów wejścia,
poprawne zamknięcie CLS i brak wywołań zapisu. Nie wywołuje ObjectRead3 z
nieustaloną sygnaturą. Nie wywołuje Bodies.Save ani zapisu rekordu ARD.

**To jeszcze nie konwersja do OCCT ani wyświetlenie bryły w MiniCam.**
Liczniki i wymiary nie stanowią pełnej kontroli topologii lub objętości.
Nie są również niezależnym dowodem zgodności z oryginalną bryłą ALPHACAM.

W chmurze Linux sprawdzono ekstrakcję z próbki i walidację raportów.
Natywny przebieg Windows pozostaje do wykonania na stacji użytkownika.
Raport ZIP zawiera wyniki i logi; nie zawiera skopiowanego runtime ani bryły.
Kopia runtime i wydzielony model zostają lokalnie w folderze testu.
