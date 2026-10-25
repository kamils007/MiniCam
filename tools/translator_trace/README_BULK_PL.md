# Proba konwersji calej bryly ARD do STEP

Wersja 4 wywoluje oryginalna funkcje SLInterop.RunInteropStandard z warstwy
SolidLink, lacznie z jej wewnetrznym przygotowaniem i ustawieniem opcji.
Przez caly czas utrzymuje zwykla sesje licencyjna CLS i zamyka ja po probie.
Nie uruchamia ALPHACAM, nie uzywa jego API i nie odbudowuje bryly po krawedziach.

1. Rozpakuj cala paczke do nowego folderu.
2. Uruchom `BULK_TEST.cmd` i wybierz ARD z jedna bryla.
3. Poczekaj na koniec. Proces konwersji ma limit 90 sekund, a otwarcie i
   zamkniecie sesji licencyjnej potrzebuja dodatkowego czasu.
4. Przeslij `raport_konwersji.zip` z otwartego folderu `wynik_...`.

ALPHACAM pozostaw zamkniety. Potrzebna jest jego dzialajaca instalacja,
skladniki SolidLink 2025.4, CLS i Python 3 x64, tak jak w MiniCam. Skrypt probuje odnalezc
Python automatycznie, a w razie potrzeby prosi o wskazanie python.exe.
Moze tez poprosic o folder instalacji ALPHACAM zawierajacy podfolder CLS.
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

To pierwsza proba pelnej funkcji SLInterop w tym kierunku, nie gotowa poprawka MiniCam.
Kompletny plik STEP i powodzenie funkcji producenta sa sprawdzane osobno;
poprawnosc geometrii wymaga pozniejszego porownania z oryginalem.
Niepowodzenie testu nie dowodzi, ze ALPHACAM nie potrafi eksportowac STEP.

Test wymaga dokladnie zbadanych wersji SLInterop.dll, SPAIInterop.dll,
SPAIInteropC.dll i CLS: sprawdza ich SHA-256. Uzywa oryginalnych konstruktorow,
destruktorow i funkcji konwersji, z kontrola adresow eksportow oraz importow MFC.
Nie pozyskuje/odtwarza materialu autoryzacyjnego i nie zmienia kodu producenta.
Nie dopisuje uprawnien ani nie wymusza pozytywnych wynikow licencji. Nie jest
potwierdzone, czy ten przebieg zapewni uprawnienia potrzebne do zapisu STEP.
Natywne wywolania sa eksperymentalne, ograniczone do osobnego procesu testowego.
W chmurze sprawdzono tylko przenosne testy i zgodnosc symboli z plikami DLL.
