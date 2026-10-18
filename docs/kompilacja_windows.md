# Kompilacja na różnych stacjach Windows

Na każdej stacji używamy tych samych presetów `station-*` i tej samej wersji
vcpkg z `tools/windows/toolchain.lock.json`. Wersje bibliotek pozostają określone
przez `vcpkg.json`: nie zmieniamy baseline ani wersji Open CASCADE.

## Pierwsze uruchomienie

1. Zainstaluj **Git for Windows** oraz **Visual Studio 2022 17.14 lub nowsze z linii 2022**
   (Community, Professional, Enterprise albo Build Tools).
2. W **Visual Studio Installer → Więcej → Importuj konfigurację** wybierz `.vsconfig`
   z katalogu projektu. Plik wymaga narzędzi C++ x64 **MSVC 14.44**, **Windows SDK
   10.0.26100.0** i narzędzi CMake. Nie musisz mieć tej samej edycji VS ani katalogu
   instalacji co na innym komputerze.
3. Pobierz projekt i uruchom **`KOMPILUJ.cmd`** z jego głównego katalogu.

Skrypt sam wykrywa Visual Studio, uruchamia środowisko kompilatora x64, przygotowuje
osobny vcpkg, konfiguruje CMake, buduje program i uruchamia testy CTest.
Pierwsze pobieranie i kompilacja Qt / Open CASCADE mogą potrwać długo i wymagają
internetu oraz wolnego miejsca na dysku. Domyślnie pracują dwa zadania równolegle.

Wynik domyślny: **`out\build\station-debug\minicam.exe`**.

Zwykła konsola PowerShell również wystarczy; nie trzeba otwierać Developer Prompt:

```powershell
.\KOMPILUJ.cmd
.\KOMPILUJ.cmd -Configuration Release
.\KOMPILUJ.cmd -Configuration DebugDLL -Jobs 4
```

Wariant DLL przyspiesza linkowanie. Do uruchomienia wymaga bibliotek DLL
kopiowanych przez vcpkg i wtyczki Qt obok programu. Warianty Debug i Release
korzystają z bibliotek statycznych, tak jak dotychczasowe presety projektu.

## Praca w Visual Studio

Uruchom **`PRZYGOTUJ_KOMPILACJE.cmd`**, potem otwórz katalog projektu w VS.
Wybierz **`station-debug`**, **`station-release`** lub **`station-debug-dll`**.
Presety wskazują narzędzia przygotowane przez skrypt; VS inicjalizuje środowisko
MSVC x64 z toolsetem 14.44. Jeżeli IDE wybierze inny kompilator, konfiguracja
zgłosi czytelny błąd — wtedy użyj `KOMPILUJ.cmd` albo popraw wybrany toolset w VS.

Stare presety `x64-debug`, `x64-release`, `x64-debug-dll` nadal działają jak wcześniej.
Przejście na `station-*` daje osobny katalog budowania i nie używa starego
`CMakeCache.txt`. Samo wybranie starego presetu nadal może korzystać z vcpkg
dołączonego do Visual Studio.

Skrypt nie nadpisuje `CMakeUserPresets.json` i nie zmienia globalnego PATH ani
globalnej zmiennej `VCPKG_ROOT`. Ścieżki konkretnego komputera zapisuje wyłącznie
w ignorowanym przez Git `out\station\environment.json`. Można tam sprawdzić
wykryte wersje narzędzi; tego pliku nie przenosimy między stacjami.
Po przeniesieniu repozytorium, aktualizacji narzędzi albo zmianie pliku lock
uruchom ponownie przygotowanie. Kolejne uruchomienia nie pobierają vcpkg od nowa.

## Pobrane pakiety i praca na kolejnych stacjach

Zarządzany vcpkg znajduje się w
`%LOCALAPPDATA%\MiniCamTools\vcpkg\<pierwsze-12-znaków-commitu>`; nie modyfikujemy vcpkg z instalacji VS.
Domyślna pamięć podręczna to **`%LOCALAPPDATA%\MiniCamTools\cache`**:

| Podkatalog | Zawartość |
| --- | --- |
| `downloads` | Pobrane archiwa i narzędzia pomocnicze |
| `assets` | Kopie plików pobieranych przez vcpkg, adresowane ich skrótem |
| `binaries` | Gotowe biblioteki zgodne z wybranym kompilatorem i konfiguracją |

W firmowej sieci można wskazać wspólny, zapisywalny katalog (ze spacjami też działa):

```powershell
.\PRZYGOTUJ_KOMPILACJE.cmd -CacheRoot '\\SERWER\MiniCam\cache'
.\KOMPILUJ.cmd
```

Wybrana ścieżka jest zapamiętywana dla tego repozytorium. Można też skopiować cały
katalog `cache` z działającej stacji na drugą i wskazać jego lokalizację przez
`-CacheRoot`. Nie kopiuj `out\build`, `vcpkg_installed` ani lokalnego pliku środowiska.
W ścieżce cache nie używaj przecinka, średnika, `#` ani `%`.

Przypięcie wersji nie zabezpiecza samo w sobie przed usunięciem plików z internetu.
Zachowanie pobranych archiwów i cache jest potrzebne również wtedy, gdy producent
usunie starą paczkę z serwera. Zachowaj też działający katalog vcpkg i instalatory
VS/SDK, jeśli potrzebujesz przygotowania stanowiska bez dostępu do sieci.
Skrypt nie jest pełnym instalatorem offline. Wersje poprawek MSVC mogą się różnić;
vcpkg sprawdza zgodność cache i w razie potrzeby przebudowuje biblioteki. CMake
pochodzi z wykrytego VS (minimum 3.28), Ninja ma minimum 1.11 — zapisujemy ich
wersje, ale nie obiecujemy identycznych binariów między wszystkimi komputerami.
Źródła C++ i literały tekstowe kompilujemy jako UTF-8 niezależnie od języka Windows.

## Kontrola i diagnostyka

Workflow **Windows build** w `.github/workflows/windows-build.yml` uruchamia ten
sam skrypt na Windows Server 2022 w GitHub Actions. Buduje Release oraz testy,
zachowuje cache i dołącza logi CMake/CTest oraz `minicam.exe` jako artefakty.
Można go uruchomić ręcznie z zakładki Actions po włączeniu workflow w repozytorium.

Do samego przygotowania albo konfiguracji bez kompilowania:

```powershell
.\PRZYGOTUJ_KOMPILACJE.cmd
.\KOMPILUJ.cmd -ConfigureOnly
```

Jeśli przygotowanie wykryje zmodyfikowany zarządzany vcpkg, przerwie pracę bez
resetowania plików. Zachowaj własne zmiany i przenieś ten katalog poza
`MiniCamTools\vcpkg\<skrót-commitu>`, następnie uruchom przygotowanie ponownie.

Pierwotny błąd pobierania `msys2-runtime-3.5.4-2` pochodził ze starszych skryptów
vcpkg w VS, a nie z braku `cl.exe`. Obecny lock wskazuje nowsze skrypty. Dostępność
archiwów MSYS2 sprawdzono podczas przygotowania zmiany; późniejszą dostępność
chroni zachowany cache, a nie sam numer wersji.

Kompilacja modułu ALPHACAM nie wymaga instalacji ALPHACAM, SolidLink ani Pythona.
Te zależności są potrzebne dopiero przy korzystaniu z eksportu brył; opisuje je
[instrukcja modułu](../modules/alphacam/README.md).
