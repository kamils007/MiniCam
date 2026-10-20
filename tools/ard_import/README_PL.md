# Sprawdzenie mozliwosci odczytu bryly ARD

1. Rozpakuj ZIP na stacji Windows, na ktorej dziala zapis ARD z MiniCam.
2. Uruchom `SPRAWDZ_ODCZYT_3D.cmd` (bez uprawnien administratora).
3. Sciezki ALPHACAM i SolidLink zostana odczytane z zapisanych ustawien
   MiniCam, na tym samym koncie Windows. Nie trzeba ponownie wybierac folderow.
   Gdy zapisanych ustawien brakuje albo wskazane pliki nie istnieja, raport
   poda konkretny blad; skrypt nie zgaduje innej instalacji.
4. Przeslij plik `translator_metadata.json` z nowego folderu `wynik_...`.

To diagnostyka **przed implementacja konwersji odwrotnej**, nie gotowy importer
bryly. Nie uruchamia ALPHACAM ani metod konwersji, nie zmienia rejestru,
instalacji ani licencji. Odczytuje wersje, skroty i deklaracje publicznych metod
z zasobow bibliotek. Nie zawiera bibliotek producenta ani modelu uzytkownika.
Nie kazda natywna DLL ma biblioteke typow; taki blad zostanie zapisany w raporcie.

Potwierdzony zapis STEP -> Parasolid nie potwierdza odczytu Parasolid -> STEP.
Nawet obecna metoda o odpowiedniej nazwie nie dowodzi udanej konwersji. Po
ustaleniu dostepnego interfejsu potrzebna bedzie rzeczywista proba konwersji
i sprawdzenie geometrii wynikowej. Plik CMD ustawia `ExecutionPolicy Bypass`
wylacznie dla uruchamianego procesu PowerShell; nie zmienia trwale ustawien
komputera ani konta. Jesli polityka firmowa nadal blokuje uruchomienie,
przeslij komunikat zamiast zmieniac te ustawienia.

Pakiet przygotowano w chmurze Linux. Czesci Windows nie mozna bylo tutaj
uruchomic; wynik tego kroku sluzy ustaleniu kolejnego testu.
