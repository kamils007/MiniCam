# Rozpoznanie importu STEP — SolidImport 2025

Stan 17.10.2026: test 13 potwierdził powstanie świeżego pliku Parasolid z STEP
przez oryginalny importer 2025.4 bez zaobserwowanego procesu Acam.exe.
Plik przechwycono przed oddaniem rysunku; sprawdzono rzeczywisty nagłówek
i hash kopii, geometrii jeszcze nie zweryfikowano. Wcześniejszy test 4.1
potwierdził Parasolid → ARD przy zamkniętej aplikacji, na innym pliku pośrednim.
Pełny bieżący przebieg STEP → ARD bez procesu ALPHACAM nadal wymaga sprawdzenia.

Test 14.2 potwierdził odczyt konkretnego Parasolidu testu 13 przez aktualną
Feature.dll: jedna bryła, 1200 × 2300 × 10 mm, 63 ściany, 158 krawędzi,
100 wierzchołków, poprawna sesja CLS i jej zamknięcie, brak Acam.exe w 38
próbkach. Pomocniczy roundtrip.x_b nie powstał mimo Save=1. Natywnego rekordu
ARD jeszcze nie zapisano; [test 14.3](feature_test_14_3.md) sprawdza bezpośrednio
oryginalny writer rekordu i jego rzeczywiste bajty. Pełny kształt nadal wymaga
nałożenia nowego ARD na STEP; zgodność wymiarów nie jest takim dowodem.

Przygotowano [test 14](feature_test_14.md): konkretny Parasolid z testu 13 →
Feature.dll, rzeczywisty pomiar obwiedni i liczników, zapis natywnego rekordu
i ARD. Używa bieżącego odczytu CLS dla jedynego sprawdzanego bitu konsumenta
Save, bez historycznego SpecialKey. Pakiet sprawdzono przenośnie; nowy ARD
i wykonanie Windows pozostają niepotwierdzone do otrzymania wyniku.

Test 14 zatrzymał się na kontroli wersji Feature.dll przed wywołaniem natywnym.
Po otrzymaniu aktualnej biblioteki 2025.4.1.200 przygotowano
[test 14.1](feature_test_14_1.md) z osobno sprawdzonymi adresami, importami
i nową tabelą 585 CString. Przeszło 59 testów przenośnych; wykonanie Windows
nowego profilu i zapis ARD nadal oczekują na wynik użytkownika.

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

### Wynik 4.1 — ARD zapisany i ponownie otwarty

Raport z 17.10.2026 i pliki z ZIP sprawdzono: proces Feature zakończył się
kodem 0, import oraz zapis jednej bryły zakończone, bez natywnych wyjątków.
Wejście X_T ma skrót z testu 3.2. Nowy rekord ma 67 544 bajty, a ARD
130 486 bajtów, SHA-256
`8885e4955c5abdae5dfa95dfda5291674bd8f96cc7298f4b62e6f6c4ce178186`.
Niezależnie odtworzono cały ARD bajt w bajt z wejściowej bazy i rekordu.
W kontrolowanych punktach tego etapu nie obserwowano procesu Acam.exe.

Użytkownik otworzył i ponownie zapisał ARD. Otrzymany plik ma 358 634 bajty,
SHA-256 `96832774525d8aad010ed61ea2a67ca0fbc7a530da4ae3bf3506122d4128dcc7`.
Zawiera natywny rekord jednej bryły ze zgodnym odniesieniem do wejściowego X_T.
ALPHACAM ponownie serializował Parasolid i dodał atrybuty; binarne dane geometrii
nie są identyczne. Użytkownik podał, że po wstawieniu dokumentów do siebie
bryły pokrywają się w 100%. To potwierdzenie porównania użytkownika, bez
niezależnego pomiaru wymiarów lub objętości w nowym ARD.

### Test 5 — rozpoznanie zależności bez procesu ALPHACAM

W logu udanego 3.2 Init wywołuje ProgramLevel, a następnie License.
Oryginalny kod robi to drugie wyłącznie dla poziomu 14 i sprawdza
ILicense.IsFeatureLicensed dla `Core:Solids`. Nie zastępujemy tej odpowiedzi
stałą ani nie zmieniamy poziomu, aby ominąć tę gałąź.

TYPELIB z dostarczonego Acam.exe 2025 opisuje klasę License,
CLSID `28AA001D-94E1-44AB-9571-E5E4E3BDB103`, oraz ILicense,
IID `214DE706-D211-4A06-84C3-6CB3044A4685`. Brakuje danych z Windows,
czy klasa ma samodzielny serwer DLL. [Test 5](../../tools/solidimport/README_TEST_5_PL.md)
czyta tę rejestrację i metadane zależności bez aktywacji COM lub ładowania DLL.
Nie jest próbą konwersji. Wynik z Windows potrzebny do wyboru następnej ścieżki
inicjalizacji. Nie dowodzi jeszcze, że niezależna aktywacja jest dostępna.

### Wynik testu 5 i analiza dostarczonych bibliotek

Raport z 17.10.2026: odczyt zakończony, brak procesu Acam.exe w punktach
kontrolnych, brak rejestracji serwera klasy License w widoku x64 i x86.
Router jest zarejestrowany przez Acam.exe /R /Auto; SolidLink przez własny EXE.
Skan folderów osiągnął limit, więc nie jest kompletnym spisem instalacji.

Dostarczone następnie CLS2005Interface.dll i SWLicense.dll są natywne x86;
CLS.dll jest x64 C++/CLI, wersja 2025.1.2507.3. Ta ostatnia udostępnia
ClsCommand, ale nie fabrykę ILicense. Odtworzono prototyp eksportu i wskazano
konkretną implementację Core:Solids w Acam.exe. Nie ustalono jeszcze kontraktu
komunikatów CLS/EPA i niezależnego tworzenia stanu funkcji produktu.
[Szczegóły analizy](cls_initialization.md).

### Wynik testu CLS 6.2 — bez procesu ALPHACAM

Publiczne OpenLicenseConnection z domyślną kontrolą producenta zwróciło
Ok (1), ValidLicenseConnection zwróciło Ok (101), produkt Alphacam (2).
Proces próby zakończył się 0, zamknięcie własnego połączenia bez wyjątku.
Nie zaobserwowano Acam.exe w 37 próbkach ani przed i po; trzy biblioteki
pozostały niezmienione. Warunki potwierdzenia sprawdzono niezależnie z ZIP.

Ten wynik potwierdza jedynie rzeczywistą sesję CLS. Nadal nie potwierdza
wydania 2025.3, Core:Solids, inicjalizacji importera ani STEP → Parasolid
bez Acam.exe. Dane zapisano w verification.json / cls_connection_6_2.

### Przygotowany test 7

[Instrukcja próby STEP → Parasolid przy otwartej sesji CLS](../../tools/solidimport/README_TEST_7_PL.md).
Publiczny COM SolidLink jest wywoływany w czasie między dwiema walidacjami
rzeczywistej sesji CLS. Nie jest to jeszcze niezależny host SolidImport.dll.
Bezpośrednie wywołanie ma sprawdzić, czy istniejąca sesja pozwoli na zwykłą
translację. Wykonanie tej próby na Windows nie jest jeszcze potwierdzone.

### Wynik 7.2 — CLS prawidłowe, bezpośrednia translacja nieudana

SolidLink 2025.4.2616.108 uruchomił się przez COM i potwierdził kontrakt
czterech używanych metod. STEP rozpoznany jako typ 11, TranslateSolid=1,
GetInteropResult=0, brak wyjściowego pliku. CLS pozostało ważne przed i po
translacji, połączenia zamknięte bez błędu. Nie zaobserwowano Acam.exe.
Późniejsza analiza zgodnego EXE wyjaśnia kod 1 jako wczesny powrót przy
niezainicjalizowanej autoryzacji SolidLink; nie jest to dowód odmowy CLS.
Szczegóły w cls_initialization.md i verification.json / cls_solidlink_7_2.

### Aktualny importer 2025.4

Dostarczony SolidImport(1).dll ma wersję 2025.4.1.100. Jego oryginalna ścieżka
STEP wykonuje SendLicenseString przed TranslateSolid oraz oddzielne
ConfirmParasolidLicensed. Init zachowuje kontrolę Core:Solids przez License
hosta przy rzeczywistym ProgramLevel=14. Odtworzenie tej sekwencji wymaga
poprawnego kontekstu hosta; sama sesja CLS z 6.2 go nie zastępuje.
Nie potwierdzono nowej konwersji. Szczegóły i zakres dowodów w
cls_initialization.md oraz solidimport_2025_4_initialization_reference.json.

### Wynik adaptera CLS → COM, test 9

Na Windows potwierdzono natywne wywołanie IsFeatureLicensed ze slotu 0x48
naszego lokalnego interfejsu ILicense. Rzeczywiste CLS przyznało 390,
adapter zwrócił HRESULT=0 i VARIANT_BOOL=-1; nieobsługiwane nazwy odrzucone.
Sesja CLS została zamknięta bez wyjątku, bez zaobserwowanego Acam.exe.
To weryfikacja adaptera, jeszcze bez wywołania oryginalnego SolidImport.
Pełny host, STEP i ARD pozostają niepotwierdzone dla tej ścieżki.
