# Sprawdzenie mozliwosci odczytu bryly ARD

1. Rozpakuj ZIP na stacji Windows, na ktorej dziala zapis ARD z MiniCam.
2. Uruchom `SPRAWDZ_ODCZYT_3D.cmd` (bez uprawnien administratora).
3. Wskaz folder instalacji ALPHACAM uzywany przy zapisie. Jesli skrypt zapyta
   o SolidLink.exe, wskaz ten sam plik co w ustawieniach eksportu MiniCam.
4. Przeslij plik `translator_metadata.json` z nowego folderu `wynik_...`.

To diagnostyka **przed implementacja konwersji odwrotnej**, nie gotowy importer
bryly. Nie uruchamia ALPHACAM ani metod konwersji, nie zmienia rejestru,
instalacji ani licencji. Odczytuje wersje, skroty i deklaracje publicznych metod
z zasobow bibliotek. Nie zawiera bibliotek producenta ani modelu uzytkownika.
Nie kazda natywna DLL ma biblioteke typow; taki blad zostanie zapisany w raporcie.

Potwierdzony zapis STEP -> Parasolid nie potwierdza odczytu Parasolid -> STEP.
Nawet obecna metoda o odpowiedniej nazwie nie dowodzi udanej konwersji. Po
ustaleniu dostepnego interfejsu potrzebna bedzie rzeczywista proba konwersji
i sprawdzenie geometrii wynikowej. Nie zmieniaj instalacji lub polityki
uruchamiania PowerShell, gdy skrypt jest blokowany; przeslij komunikat.

Pakiet przygotowano w chmurze Linux. Czesci Windows nie mozna bylo tutaj
uruchomic; wynik tego kroku sluzy ustaleniu kolejnego testu.
