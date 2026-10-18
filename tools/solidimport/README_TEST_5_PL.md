# Test 5 — zależności inicjalizacji bez ALPHACAM-a

Zamknij wszystkie wersje ALPHACAM. Rozpakuj całą paczkę i uruchom
`URUCHOM_TEST_5_BEZ_ALPHACAMA.cmd`. Prześlij nowy ZIP `standalone_test_5_...zip`.
Nie trzeba wybierać modelu ani otwierać aplikacji.

**Ten test jeszcze nie wykonuje konwersji STEP ani nie tworzy ARD.**
Sprawdza brakujący element przed przygotowaniem konwersji całkowicie
bez `Acam.exe`.

W udanym teście 3.2 pełna inicjalizacja SolidImport korzystała z rzeczywistego
obiektu `IAlphaCamApp.License`. W ścieżce inicjalizacji dla poziomu 14
wywołuje on `ILicense.IsFeatureLicensed` dla `Core:Solids`. To odczytane
zachowanie oryginalnego kodu; nie zastępujemy go zapisaną wartością `true`.
Poprzedni odczyt Path / ProgramLetter / SpecialKey nie zawiera obiektu License
i nie może go zastąpić po zamknięciu programu.

Biblioteka typów ALPHACAM 2025 opisuje osobną klasę License, CLSID
`28AA001D-94E1-44AB-9571-E5E4E3BDB103`, i ILicense, IID
`214DE706-D211-4A06-84C3-6CB3044A4685`. Sam wpis w bibliotece typów nie
potwierdza jej samodzielnej dostępności. Odczyt rejestracji pokaże, czy
instalacja wskazuje komponent DLL, proces EXE, czy nie ma takiego wpisu.

Test czyta rejestrację tych klas oraz Router i SolidLink w obu widokach
rejestru. W folderach instalacji zbiera ograniczony zestaw nazw zależności,
skrótów i eksportów DLL związanych z inicjalizacją. Nie ładuje kodu producenta,
nie aktywuje COM, nie zapisuje rejestru i nie odczytuje treści plików licencji.
ZIP zawiera raport, bez DLL/EXE producenta i bez modelu.

`discovery_completed = true` oznacza wykonanie odczytu. Nie oznacza
konwersji, poprawnej licencji ani działania niezależnego translatora.
W raporcie `conversion_attempted`, `parasolid_created` i `ard_created`
pozostają jawnie `false`.

Domyślna ścieżka pochodzi z poprzedniego rzeczywistego odczytu API 2025.
Jeśli instalacja została przeniesiona, można podać ją jawnie:

```bat
URUCHOM_TEST_5_BEZ_ALPHACAMA.cmd --root "C:\Program Files\Hexagon\ALPHACAM 2025"
```

Wymagany Windows i Python x64; bez dodatkowych pakietów pip.
Kod odczytu sprawdzono przenośnie. Wykonanie testu na Windows pozostaje
do sprawdzenia na instalacji użytkownika.
