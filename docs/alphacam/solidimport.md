# Rozpoznanie importu STEP — SolidImport 2025

Stan 16.10.2026: test 3.2 potwierdził STEP → Parasolid przy otwartym ALPHACAM 2025.

## Dostarczone pliki

- SolidImport.dll: 158 288 bajtów, wersja 2025.3.0.100, x64, SHA-256
  `b88bef3244cac2cc38bfe41df2089f0a1b6e7b62aea08ae443b95cd47dcd8860`.
- SolidImport.po: katalog komunikatów, m.in. osobne komunikaty o nieudanym
  tłumaczeniu i nieudanym imporcie. Nie zawiera implementacji translatora.
- Bryła_4.stp: 91 684 bajty, SHA-256
  `88541daf8ce0f68780ab615550c87a0417933083577ca2d63d6f40a1722735d7`.
  Nagłówek wskazuje Inventor 2025 i AP242. Odczyt przez Open CASCADE:
  jeden przeniesiony korzeń, jedna poprawna topologicznie bryła,
  obwiednia około 1200 × 2300 × 10 mm. To kontrola wejścia, nie próba SolidLink.

Pliki producenta i model użytkownika nie są publikowane w repozytorium.

## Ustalenia z kodu

| Miejsce w SolidImport.dll | Obserwacja |
| --- | --- |
| eksport StepInputFile, RVA 0xC8B0 | Dwa użyte argumenty rejestrowe: kontekst aplikacji i ścieżka; QueryInterface na wskaźniku z kontekstu; następnie wspólna procedura z etykietą STEP |
| RVA 0x9420 | Wspólna ścieżka tworzy pliki pośrednie .x_t i .txt; przygotowuje obiekt translatora i uruchamia tłumaczenie |
| RVA 0x77E0 | Ustawia produkt Alphacam i wersję 2025.3; domyślna wersja komponentu także 2025.3, z opcją wyboru innej wersji |
| RVA 0x11D80 | Buduje nazwę z formatu `SolidLink.Document.%s` i próbuje utworzyć obiekt przez MFC COleDispatchDriver |
| RVA 0x11740 | Składa nazwy ustawień wersji; nazwa ustawienia obejmuje produkt i wersję |
| RVA 0x10AB0 | Sprawdza rejestrację COM, w tym ścieżkę CLSID/LocalServer32, dla wyboru wersji |
| RVA 0x10510 | Wspólne tłumaczenie wywołuje metody IDispatch komponentu SolidLink |

Sam eksport StepInputFile nie jest prostą funkcją `STEP → plik X_B`.
Kontrakt wywołania eksportów odtworzono z dostarczonej DLL; test 3 używa
kontekstu VARIANT x64. Test 3.2 koryguje drugi argument na ścieżkę ANSI
`const char*`; wcześniejsze użycie UTF-16 było błędem testu.

Zależności statyczne SolidImport obejmują MFC, runtime MSVC i biblioteki
systemowe. Feature.dll i PSKERNEL.dll nie występują w jej zwykłej tablicy
importów. Nie oznacza to braku zależności pośrednich: translacja korzysta z COM,
a dalsza ścieżka importu może korzystać z hosta i innych komponentów.

## Wynik diagnostyki 1 i interfejs SolidLink

Raport z Windows wskazał klasę `SolidLink.Document.2025.3`, CLSID
`80C33292-C478-4543-9DC1-5381A5156F05` i istniejący serwer:

```text
C:\Program Files\Common Files\Hexagon\SolidLink 2025.3\code\bin\Solidlink.exe
```

Przesłany SolidLink.exe ma 1 276 496 bajtów i SHA-256
`283a9ce685944a12c66c0be8568e48f23eddc9efe63456deafdf308faf24ee4d`.
Jego zasób TYPELIB/1/1033 zawiera 26 metod ISolidLink,
IID `B9E54437-A0AD-48A3-82A7-EE8FEAA81F63`.
Kontrakt jest w `tools/solidimport/solidlink_api_2025.json`.
CLSID coclass zapisany w TYPELIB różni się od zarejestrowanego CLSID wersji;
próba korzysta z rzeczywistej, sprawdzonej rejestracji użytkownika.

| Metoda | DISPID | Parametry Automation i wynik |
| --- | --- | --- |
| TranslateSolid | 1 | BSTR input, BSTR output, BSTR application → I4 |
| GetInteropResult | 10 | Bez parametrów → I4 |
| GetFileType | 12 | BSTR input → I4 |
| SetOutputLogFile | 8 | VARIANT_BOOL → void |

Statyczne zależności serwera obejmują SLInterop.dll, PMFrustum.dll,
PSKERNEL.dll, MFC i runtime MSVC. Dlatego test uruchamia zainstalowany
serwer przez COM, z jego istniejącym otoczeniem.

### Test 2

[Instrukcja próby konwersji](../../tools/solidimport/README_TEST_2_PL.md).
Python przygotowuje kopię wejścia i osobny proces PowerShell x64 STA,
który wywołuje zwykłe metody Automation. Limit czasu i raport nadrzędny
pozwalają odróżnić aktywację COM, zakończenie metody i utworzenie wyniku.
Próba nie ustawia stanu licencji i nie wysyła ciągów autoryzacyjnych.

Kod 0 z TranslateSolid jest traktowany jako powodzenie zgodnie z gałęzią
SolidImport.dll 0x1095D. Raport dodatkowo wymaga niepustego pliku z pełnym
nagłówkiem Parasolid i zachowania skrótów wejścia, kopii i serwera.
To nadal nie zastępuje dalszego importu i porównania geometrii przez Feature.dll.
Wynik testu 2 z Windows: aktywacja COM poprawna, GetFileType = 11,
TranslateSolid = 1, GetInteropResult = 0, brak wyjściowego pliku. Wejście,
wersja robocza i serwer zachowały skróty. GetInteropResult = 0 nie dowodzi
konwersji — w tym przebiegu stan komponentu pozostał początkowy.

### Test 3 — oryginalna inicjalizacja SolidImport

[Instrukcja](../../tools/solidimport/README_TEST_3_PL.md),
[implementacja](../../tools/solidimport/test_solidimport_init.py).

W SolidLink.exe procedura tłumaczenia RVA 0x332C0 sprawdza na wejściu
stan przygotowania sesji i przy braku tego stanu natychmiast zwraca 1.
To pasuje do wyniku testu 2, ale nie jest jeszcze potwierdzeniem przyczyny
w wykonaniu z debugerem. Następna próba korzysta z całego oryginalnego
InitAlphacamAddIn (RVA 0x7A30), a potem StepInputFile (0xC8B0).
Nie wywołuje prywatnych procedur autoryzacji ani nie podmienia ich stanu.

Prywatny host odsyła rzeczywiste ProgramLevel, SpecialKey, License, Path
oraz ProgramLetter przez wybrane gettery otwartego ALPHACAM-u. Init zachowuje
własną kontrolę License dla poziomu 14. Prywatna ramka zapisuje rejestracje
funkcji lokalnie i przekazuje tylko ReadTextFile do rzeczywistej ramki.
Pozostałe nieobsługiwane metody zatrzymują proces z logiem.

Po translacji procedura 0x9070 zaczyna import od ActiveDrawing (app +0x48).
W tym miejscu test kopiuje kompletny plik pośredni i kończy proces kodem 83,
bez udostępnienia rysunku. Osobny TEMP/TMP jest ustawiany przed startem procesu.
Raport rozróżnia przechwycenie Parasolidu, walidację geometrii i pełny ARD;
dwa ostatnie etapy pozostają niepotwierdzone. Nie sprawdzamy normalnego
zwolnienia DLL, zamkniętego ALPHACAM-u ani natywnego wykonania testu 3 tutaj.

Walidacja przenośnej części: 11 testów odczytu konfiguracji, odrzucania
fałszywego sukcesu oraz zachowania/przechwycenia plików. Próba Windows testu 3
jest do wykonania na instalacji użytkownika.

### Wynik testu 3 i poprawka 3.1

Raport użytkownika z 16.10.2026 potwierdza połączenie z instalacją **2025**,
zgodną SolidImport.dll i InitAlphacamAddIn = **0**. Następny etap kończy się
`access violation reading 0x0000000000000080`, bez nowego QueryInterface
w logu i bez pliku pośredniego. Skróty źródła, kopii i obu bibliotek zachowane.

StepInputFile przed QueryInterface wywołuje import MFC z IAT RVA 0x18558,
po czym bezwarunkowo czyta `[rax+0x80]` (RVA 0xC8CB). Ten import to ordinal
2222, AfxGetThread. Adres 0x80 pasuje do pustego wskaźnika CWinThread, ale
pierwszy raport nie ma RIP/stosu, więc jest to diagnoza do potwierdzenia.

[Test 3.1](../../tools/solidimport/README_TEST_3_1_PL.md) tworzy wątek przez
publiczne AfxBeginThread i sprawdza AfxGetThread przed oboma eksportami.
Własny apartament STA i wszystkie interfejsy COM powstają na tym wątku.
Odczyt ścieżek w wątku startowym korzysta z osobnego, zamykanego STA.
Dodano sprawdzony wcześniej rejestrator pierwszego błędu natywnego; nie zmienia
rejestrów ani obsługi wyjątku.

Oficjalny runtime MFC 14.44.35211.0 z pakietu Microsoftu i publiczny PDB
wskazują AfxGetThread = ordinal **2222**, worker AfxBeginThread = **2149**,
a jego inne przeciążenie z CRuntimeClass = **2150**. Runtime ma 14 103
niepuste eksporty i zero eksportów nazwanych. Wybieramy wariant z callbackiem
UINT(void*), nie przeciążenie CRuntimeClass. Źródło symboli, tożsamość PDB,
sygnatury i skróty są w `mfc_thread_api_reference.json`. Nie przypisujemy tej
wersji runtime'u instalacji użytkownika; jego rzeczywisty moduł i skrót będą
w nowym logu. Nie wpisujemy pól CWinThread, TLS ani flag translatora.

Walidacja poprawki: **17 testów przenośnych**, odczyt eksportów rzeczywistej
MFC z Microsoftu oraz zgodność wybranych ordinałów z dopasowanym PDB.
Wykonania Windows 3.1 jeszcze nie potwierdzono.

### Wynik testu 3.1 i poprawka argumentu w 3.2

Raport z 16.10.2026: wątek MFC poprawny przed Init i STEP (ten sam identyfikator),
Init = 0, brak natywnych wyjątków i brak Parasolidu. Eksport STEP wszedł do
QueryInterface i wrócił bez dalszych wywołań hosta. Wartość odczytana z EAX
po jego zakończeniu nie jest dowodem powodzenia tłumaczenia.

Ponowna kontrola IAT i oficjalnego PDB MFC wykazała błąd wcześniejszego testu:
StepInputFile używa konstruktora CStringW z **char*** (MFC ordinal 290,
IAT RVA 0x183C8), a nie konstruktora wchar_t* (ordinal 286, RVA 0x18678).
Przekazany wcześniej bufor UTF-16 dawał nazwę `C` zamiast `C:\...`.
Procedura 0x9420 ma wczesne wyjście przy nieudanym otwarciu źródła, przed
konfiguracją translatora i następnym wywołaniem hosta. To pasuje do logów;
potwierdzenie pełnej ścieżki wykonania wymaga kolejnego uruchomienia.

[Test 3.2](../../tools/solidimport/README_TEST_3_2_PL.md) przekazuje `c_char_p`
z kodowaniem mbcs/Windows ACP i odrzuca stratną konwersję. Przed eksportem
uruchamia ten sam konstruktor MFC, odczytuje długość i pełną nazwę Unicode,
sprawdza plik i skrót, a następnie niszczy CString oryginalnym destruktorem.
`step_argument.json` jest wymaganym dowodem w raporcie przechwycenia.
Importy długości i destruktora: ordinale 5674 i 1033, IAT 0x18380 i 0x18A68.
Publiczne sygnatury zapisano w mfc_thread_api_reference.json.

Walidacja przygotowania: 19 testów przenośnych; natywne wykonanie 3.2 oraz
konwersja STEP pozostają niepotwierdzone.


### Wynik testu 3.2 — potwierdzona konwersja STEP

Raport z Windows i rzeczywiste pliki sprawdzono 16.10.2026. Init = 0,
wątek MFC i ścieżka poprawne, brak natywnych wyjątków. Przechwycenie nastąpiło
przy ActiveDrawing, kod kontrolowanego zakończenia 83. `converted.x_t`
ma 99 728 bajtów, SHA-256
`ab2fdda5033b63c2097c243c970211b59cdcf3229e4392601a1ad644997017c1`.
Jest identyczny z pośrednim FE STEP0001.x_t. Nagłówek: FORMAT=text,
Parasolid 37.0.185. Log Spatial InterOp Connect 2025 1.0.0 zawiera:
Translation Result = Success, BRep Conversion Result = Success,
No. of Solid Bodies = 1, pozostałe rodzaje brył = 0.

Zgodność raportu, skrótów wejścia/wyjścia i zachowanie źródłowych DLL/EXE
potwierdzono ponownie. To dowód wykonania konwersji, nie pełny pomiar geometrii.
ALPHACAM pozostawał otwarty dla rzeczywistych odczytów hosta w inicjalizacji.

[Test 4](../../tools/step_ard/README_PL.md) używa nowego Parasolidu jako wejścia
niezmienionego wzorca Feature 4.9 przy zamkniętym ALPHACAM-ie. Po potwierdzonym
imporcie jednej bryły, zapisie i natywnym rekordzie tworzy diagnostyczny ARD
na bazie użytkownika. Algorytm opakowania odtworzył poprzedni TEST_6 bajt w bajt.
Nowy ARD i jego geometria wymagają dopiero wykonania i otwarcia na Windows.


### Wynik próby ARD 4 i rozszerzenie hosta 4.1

Przy zamkniętym ALPHACAM-ie Feature.dll weszła w import nowego X_T, ale proces
zatrzymał się kontrolowanym kodem 80 na IAlphaCamApp.DisableUndo PROPERTYGET
(DISPIDs 57, vtable 0x2B8, VARIANT_BOOL*). W logu poprzedza to etykieta
Auto-Wyrównanie Części. Brak feature_object.bin był skutkiem niedokończonego
importu. DLL i wejście niezmienione, bez natywnych wyjątków.

Host Python i pomocnik C++ 4.1 zgłaszają brak historii Undo przez DisableUndo
= true. Setter 0x210 przyjmuje tylko ponowne wyłączenie. Próba włączenia nadal
jest nieobsługiwana. Kontrola importu/zapisu pozostaje wymagana. Raport etapu
jest sprawdzany przed próbą odczytania pliku natywnego.

Przeszło 16 przenośnych testów hosta i 4 testy raportowania/opakowania.
Wykonanie 4.1 na Windows, kompilacja zmiany C++ i nowy ARD niepotwierdzone.
