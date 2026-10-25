# Proba konwersji calej bryly ARD do STEP

Wersja 3 sprawdza ladowanie SPAXParasolid.dll i SPAXStep.dll z pelnych sciezek
w instalacji SolidLink. Zapisuje dokladny blad Windows, jesli ladowanie sie nie
powiedzie. Ustawienie wyszukiwania DLL dotyczy tylko osobnego procesu testowego.
Zachowuje pelna oryginalna funkcje konwersji C++ z wersji 2.

1. Rozpakuj cala paczke do nowego folderu.
2. Uruchom `BULK_TEST.cmd` i wybierz ARD z jedna bryla.
3. Poczekaj na koniec. Proces konwersji ma limit 90 sekund.
4. Przeslij `raport_konwersji.zip` z otwartego folderu `wynik_...`.

ALPHACAM nie musi byc uruchomiony. Potrzebna jest jego dzialajaca instalacja
SolidLink 2025.4 oraz Python 3 x64, tak jak w MiniCam. Skrypt probuje odnalezc
Python automatycznie, a w razie potrzeby prosi o wskazanie python.exe.
Nie trzeba pobierac repozytorium ani zmieniac ustawien wykonywania PowerShell.

Test nie zmienia ARD, MiniCam, instalacji producenta ani jej ustawien.
Wyjmuje Parasolid z ARD do nowego folderu i uruchamia oddzielny proces.
Mierzy ladowanie bibliotek, przygotowanie obiektow, konwersje i sprzatanie.
Przy sukcesie raport ZIP zawiera rowniez nowy `converted.step` z geometria
wybranej bryly, aby mozna bylo sprawdzic ja w OCCT. Pliki stdout/stderr
pozostaja tylko lokalnie; nie sa automatycznie pakowane. Oryginalny dziennik
translatora `translator_native.log` tez pozostaje lokalnie. Raport zawiera
wylacznie stale liczniki slow diagnostycznych z dziennika, bez kopiowania
surowego tekstu ustawien czy danych kontekstu producenta. Dodatkowo wykrywa
konkretne ostrzezenia o niezaladowaniu SPAXParasolid/SPAXStep.

To pierwsza proba tego kierunku konwersji, nie gotowa poprawka MiniCam.
Kompletny plik STEP i powodzenie funkcji producenta sa sprawdzane osobno;
poprawnosc geometrii wymaga pozniejszego porownania z oryginalem.
Niepowodzenie testu nie dowodzi, ze ALPHACAM nie potrafi eksportowac STEP.

Test wymaga dokladnie zbadanych wersji SPAIInterop.dll i SPAIInteropC.dll:
sprawdza ich SHA-256 przed zaladowaniem. Uzywa oryginalnych konstruktorow
dokumentu i konwertera oraz GetData; nie odczytuje pol klas C++ ani nie
pozyskuje/odtwarza materialu autoryzacyjnego. Oryginalny konstruktor zachowuje
przygotowanie kontekstu producenta. Natywne wywolania sa eksperymentalne,
ograniczone do osobnego procesu testowego. Nie testowano ich w Linux.
