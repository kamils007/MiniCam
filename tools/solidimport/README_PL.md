# SolidImport 2025 — diagnostyka 1

Sprawdzona statycznie SolidImport.dll: **2025.3.0.100, Windows x64**,
SHA-256 `b88bef3244cac2cc38bfe41df2089f0a1b6e7b62aea08ae443b95cd47dcd8860`.

Funkcja StepInputFile jest w tej DLL, lecz przekazuje konwersję komponentowi
SolidLink. Kod tworzy identyfikator COM `SolidLink.Document.2025.3` w domyślnej
konfiguracji. Rejestracja na danym komputerze wskaże rzeczywisty plik serwera.

## Uruchomienie

1. Rozpakuj ZIP do nowego katalogu.
2. Uruchom **ODCZYTAJ_SOLIDLINK.cmd** na komputerze z ALPHACAM 2025.
3. Prześlij **solidlink_environment.json** z nowego folderu `solidlink_diag_…`.

Wymagany Python 3, bez pakietów pip. ALPHACAM może pozostać zamknięty.
Nie trzeba wskazywać DLL ani modelu. Oryginalne pliki nie są zmieniane.

Skrypt odczytuje tylko rejestrację klas `SolidLink.Document.*`, powiązane
ścieżki serwerów COM i bibliotek typów oraz rozmiary i skróty wskazanych plików.
Odczytuje widoki 64- i 32-bitowy, bo instalacje mogą mieć obie rejestracje.
Nie uruchamia translatora, nie ładuje DLL, nie odczytuje ustawień licencji
ani nie zapisuje niczego w rejestrze. Zapisuje nowy raport w swoim katalogu.

**To diagnostyka brakującego komponentu, jeszcze nie test konwersji STEP.**
Raport pozwoli wskazać dokładny plik translatora i rozpoznać jego interfejs,
zanim zaczniemy go wywoływać. Przesłana Bryła_4.stp jest już dostępna do próby;
nie trzeba dołączać jej ponownie.
