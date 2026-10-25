# Oddzielne konwertery SolidLink — analiza plików 25.10.2026

Użytkownik wskazał `Common Files/Hexagon/SolidLink 2025.4/converters/parasolid`
i dostarczył dwa EXE. Zbadano pliki statycznie; nie uruchamiano ich na Linux.
Pliki producenta i ich dezasemblacja pozostają poza repozytorium.

| Plik | Bajty | SHA-256 |
| --- | ---: | --- |
| ParasolidImportConverter.exe | 1301144 | 9200a06ad1ba18701af22d4604a4055a81cdf8e5f60c3fed5a34393477891255 |
| ParasolidExportConverter.exe | 1283736 | 00cbd47722c37239eaff6d9fd027385cf597f8098e0b04ae678a489207b7d039 |

Oba są natywnymi PE32+ AMD64, podsystem Windows GUI, z metadanymi Hexagon /
SolidLink i oznaczeniem wersji 2025.4.2616.108. Importują PSKERNEL.dll, w tym
PK_PART_receive_u i PK_PART_transmit_u. Nie są to samodzielne skrypty konsolowe;
nie ustalono przełączników typu `--input`/`--output` i nie należy ich zgadywać.

## Potwierdzone odwołania i ograniczenia

- Oba pliki czytają dane pod kluczami IE_SP_IE_DIR i IE_SP_FILE_TO_CONVERT;
  odnaleziono również plik ie_config.cfg i sekcję [PARAMS]. Nie dowodzi to,
  że są to zmienne środowiskowe: odczyt w kodzie korzysta z obiektu konfiguracji.
- Import zawiera odwołania do pie_import_structure.xml, `_prt.xml`, `.x_b`,
  `.m_b`, `.m_t` i nazw struktur TMParts/TMPart/TMTransformation.
- Eksport zawiera odwołania do ie_export_structure.xml i
  ie_export_parasolid.x_b oraz tych samych elementów roboczej struktury.
- W ścieżce odczytującej IE_SP_FILE_TO_CONVERT w importerze występuje wywołanie
  PK_PART_receive_u. W eksporterze występuje zapis Parasolid przez
  PK_PART_transmit_u. Są to przesłanki kierunku obu etapów; nie wykonano
  jeszcze całej translacji i nie potwierdzono formatu roboczych plików.
- To nie jest dowód bezpośredniej konwersji Parasolid→STEP. Nazwy Import/Export
  odnoszą się do etapu względem Parasolid, nie automatycznie do STEP lub OCCT.

## Następne potrzebne dane

Potrzebna jest lista pozostałych folderów `converters`, zwłaszcza STEP/IGES,
a następnie odpowiedni konwerter eksportu i sposób przygotowania konfiguracji
przez oryginalny SolidLink. Robocza hipoteza: ParasolidImportConverter → dane
pośrednie → konwerter eksportu STEP/IGES. Kontrakt plików i uruchomienia trzeba
potwierdzić na stacji. Obsługa formatu w oknie eksportu ALPHACAM nie potwierdza
jeszcze działania tych EXE samodzielnie ani dostępności tego samego wyjścia
przez publiczny TranslateSolid.

Nie wykonano konwersji, nie przygotowano wywołania z domyślnymi parametrami,
nie pozyskiwano ani nie odtwarzano materiału autoryzacyjnego. W razie śledzenia
procesu należy obserwować oryginalny przebieg konfiguracji i plików, zachowując
normalną obsługę licencji. Plan debugowania całego Acam został wstrzymany po
wskazaniu tych konkretnych narzędzi przez użytkownika.

## SpaImportConverterC.exe

Dostarczony plik: 1295000 bajtów, SHA-256
`41795630b92eb21a5f6fdc3ddca09eaf8d84d4168ae381034e72859e2c2e0e96`,
PE32+ AMD64/Windows GUI, oznaczenie 2025.4.2616.108.

Importuje SPAIop, SPAIInterop, SPAIParasolid, SPAIParasolidKernel i SPAAcisDs.
Występuje interfejs SPAIConverter::Convert z dokumentem źródłowym i docelowym,
SPAIDocument::SetType, SPAIDocument i SPAIParasolidDocument. W tabeli formatów
są m.in. STEP, IGES, PARASOLID i ACIS. W kodzie są osobne wywołania Convert
z dokumentem ogólnym i dokumentem Parasolid jako celem, a także nazwy
`si_import_step.stp` i `ie_import_parasolid.x_b`. To uzasadnia badanie konwersji
całej bryły w oryginalnym silniku Spatial; nie potwierdza samodzielnego działania
Parasolid→STEP ani zgodności z konfiguracją/licencją stacji.

Nie odtwarzamy ABI klas C++ ani nie wywołujemy funkcji na podstawie samych nazw.
Następny krok to obserwacja oryginalnego procesu i wybranych pól CFG.
`tools/translator_trace/TRACE.cmd` uruchamia 60-sekundowe próbkowanie CIM i odczyt
wyłącznie dozwolonych pól konfiguracji wskazanej w argumentach procesu. Nie
uruchamia sam konwertera i nie kopiuje plików producenta ani danych licencyjnych.
To test obserwacyjny, nie debugger i nie weryfikacja konwersji. Natywnego
monitorowania nie wykonano w Linux.

## PLMProductDS.mask

Dostarczony plik ma 14406 bajtów, SHA-256
`43df1b7b3838c2b167b93236ebe81c58a54a0773ace4df2116bf6b216a5699da`.
To tekstowa maska atrybutów encji PLMProductDS/PLMProductInstanceDS i pokrewnych:
nazwy, rewizje, daty, właściciel i operacje paneli Create/Read/Write/Query.
Nie znaleziono w nim ustawień uruchomienia konwertera STEP/Parasolid. Nie
jest używany jako instrukcja dla asystenta i nie jest dodawany do repozytorium.

## Pierwszy raport obserwacyjny

Raport użytkownika zarejestrował SolidLink.exe, SHA-256 zgodny z profilem
`71d48d786a76aa16031be28c9ebbe4c9b95555643fd79f475b5a6e76587ce83c`.
Nie zarejestrowano błędów, konfiguracji ani procesów konwerterów. Nie wiadomo,
czy procesy były krótkotrwałe, czy SolidLink korzystał bezpośrednio z DLL.
Wersja 2 testu dodaje zdarzenia startu procesów i obserwację modułów SolidLink,
z jawnym raportowaniem niedostępnego źródła danych. To wciąż ślad obserwacyjny,
a nie potwierdzenie wywołań czy udanej konwersji Parasolid→STEP.

## Drugi raport obserwacyjny

Raport v2 z 25.10.2026 obejmuje 60,18 s. Zarejestrowano jeden SolidLink.exe,
ze skrótem zgodnym z profilem powyżej, oraz 49 załadowanych modułów.
Wśród nich są SLInterop.dll (2025.4.2616.108), PSKERNEL.dll (37.00.214),
SPAIInterop.dll, SPAIInteropC.dll, SPAIop.dll, SPAIopC.dll, SPAXStep.dll,
SPAXStepSplitter.dll i SPAXParasolid.dll (rodzina Spatial 6.427).
Potwierdza to obecność tych bibliotek w procesie, nie ich wywołanie podczas
otwierania modelu. Wszystkie obserwacje modułów pochodzą z pierwszej próbki;
raport nie ustala, które biblioteki załadowano dopiero w związku z importem.

Subskrypcja zdarzeń startu procesów zakończyła się `Access denied`;
`process_events_enabled=false`. Zwykłe próbkowanie pozostało aktywne, lecz nie
znalazło procesów Converter. Pusta lista zdarzeń nie dowodzi ich nieobecności.
Nie przechwycono konfiguracji ani pomiaru czasu samej konwersji. Nie ma dowodu
udanej konwersji Parasolid→STEP i nie zmieniono ścieżki importu MiniCam.

Kolejny krok to statyczne zbadanie SLInterop.dll, SPAIInteropC.dll i SPAIopC.dll
z `SolidLink 2025.4/code/bin`: pierwsza łączy SolidLink z translacją, a dwie
pozostałe pozwolą sprawdzić dostępność interfejsów C. Do dalszej analizy potrzebne
są rzeczywiste pliki. Nazwa DLL ani eksportowanej funkcji nie wystarczy do
ustalenia kontraktu; przed uruchomieniem trzeba potwierdzić typy, wymagane
przygotowanie oraz kierunek konwersji. Nie ponawiamy tego samego testu
obserwacyjnego tylko po to, by zebrać ponownie listę modułów.

## Analiza czterech bibliotek z tej instalacji

Otrzymano SLInterop.dll, SPAIInterop.dll, SPAIopC.dll, a następnie również
SPAIInteropC.dll. Skróty, wersje, zależności i wybrane eksporty zapisano w
[`spatial_interop_2025_4_reference.json`](../../tools/solidimport/spatial_interop_2025_4_reference.json).
Pliki DLL nie są częścią repozytorium; nie uruchamiano ich w Linux.

Potwierdzone statycznie:

- SLInterop::RunInteropStandard tworzy dokument wejściowy i wyjściowy, ustawia
  jednostki i opcje, a następnie wywołuje SPAIConverter::Convert. Nie przechodzi
  w tej funkcji przez obiekty Feature reprezentujące kolejne krawędzie bryły.
- SPAIInterop.dll jest warstwą C++ nad SPAIInteropC.dll. Część C udostępnia
  fabryki dokumentów i konwertera, SetType, Convert, obsługę wyników oraz
  zwalnianie nieprzezroczystych uchwytów. Nie trzeba odtwarzać układu klas C++,
  aby opisać sam kontrakt C. Nazwy eksportów są mimo to dekorowane przez MSVC.
- `SPAICConverter_Convert` ma trzy argumenty typu `void*`: konwerter, źródło
  i cel. Zwraca 32-bitowy wynik. Kierunek argumentów potwierdzono w kodzie
  wywołującym SPAIConverterImpl::Convert, nie tylko z nazwy funkcji.
- Oryginalny konstruktor dokumentu C++ przygotowuje kontekst produktu przed
  użyciem fabryki C. Samo System_Initialize + Document_Create + Convert nie
  odtwarza całego przebiegu producenta. Nie przygotowano takiego uproszczonego
  testu, który pomijałby znany etap i dawał trudny do interpretacji błąd.
- SPAIopC.dll udostępnia osobną warstwę obiektów, m.in. PartExporter,
  DocumentDataImporter i struktury wyników. To nie zamiennik SPAIInteropC.dll;
  nie wolno mieszać uchwytów między nimi na podstawie podobieństwa nazw.
- **RunInteropIntermediateStep korzysta z tymczasowego pliku SAT**:
  wzorzec `%s%s.sat` ma odwołanie w instrukcji RVA 0x22d89, a dwa wywołania
  Convert są przy RVA 0x22e61 i 0x22ef8. „Step” w nazwie metody nie dowodzi
  eksportu do formatu STEP.

Wynik to opis mechanizmu konwersji całego dokumentu i konkretnego interfejsu,
nie działający importer Parasolid→STEP. Nie ustalono jeszcze pełnego,
obsługiwanego przygotowania samodzielnej sesji ani dostępności wyjścia STEP
dla tego kierunku na stacji. Nie pozyskiwano materiału autoryzacyjnego i nie
zmieniano konfiguracji ani kodu bibliotek. Nie ma nowego pomiaru wydajności.
Przed podłączeniem do MiniCam potrzebna jest natywna próba z właściwym
przygotowaniem, a następnie odczyt wyniku przez OCCT i porównanie bryły ze
źródłem. Dotychczasowy czytnik MiniCam i eksport ALPHACAM pozostają bez zmian.

## Próba konwersji całej bryły — paczka v1

`tools/translator_trace/bulk_conversion_probe.py` przygotowuje samodzielny test
Windows. Używa oryginalnego SPAISystem::Initialize(true) oraz konstruktorów
SPAIDocument/SPAIConverter, zachowując przygotowanie producenta. Uchwyty
otrzymuje wyłącznie przez eksport GetData; właściwą konwersję wykonuje
SPAICConverter_Convert. Nie wywołuje funkcji pozyskujących materiał
autoryzacyjny i nie odtwarza go ze stałych ani raportów.

Nie stosuje odtworzonej definicji klasy C++. Oryginalny konstruktor otrzymuje
24-bajtową pamięć z obszarami kontrolnymi. Rozmiar potwierdza konstruktor
SPAIObject przy RVA 0x9e30 (trzy pola po 8 bajtów), brak dodatkowych pól
w konstruktorach SPAIDocument i SPAIConverter oraz odstępy 24 bajtów pomiędzy
obiektami w kodzie SLInterop. GetData ma RVA 0x9e90, konstruktory dokumentu
UTF-16/konwertera 0x5ea0/0x4a40, a destruktory 0x6040/0x4bc0. Wszystkie
wywołania dotyczą wyłącznie przypiętego SHA-256 wersji x64 z tego raportu;
brak nagłówków SDK ogranicza przenośność tego eksperymentalnego adaptera.

Rodzic strukturalnie wyjmuje X_B z ARD, uruchamia proces testowy i daje mu
90 sekund. Każdy etap zapisuje osobny punkt kontrolny i czas. Wynik wymaga
powodzenia bez częściowego wyniku/wyjątku, nowego kompletnego STEP, zgodnego
skrótu wyjścia, niezmienionego źródła oraz poprawnego zakończenia procesu.
`geometry_verified` pozostaje false: nagłówek STEP nie potwierdza bryły.
Raport ZIP obejmuje JSON i, tylko przy takim wyniku, nowy STEP do niezależnego
sprawdzenia. Pakiet nie zawiera bibliotek producenta ani modelu testowego.

Przenośne testy korzystają z atrap natywnych funkcji; sprawdzają kolejność
oryginalnego tworzenia/sprzątania obiektów, odrzucenie częściowego wyniku,
kontrolę pamięci i korelację wyjętego X_B z ARD. Nie wykonano wywołań Windows
w chmurze. Nie potwierdzono jeszcze, że samodzielne przygotowanie wystarczy
do konwersji Parasolid→STEP na stacji, i nie zmieniono importu w MiniCam.

## Raport próby v1 i korekta v2

Pierwsza próba dotarła do Convert, lecz zakończyła się `0x01000001` / `Failure`.
Całość trwała 0,421139 s, samo nieudane Convert 0,023641 s. Biblioteki,
inicjalizacja, oryginalne konstruktory, ustawienie obu typów i sprzątanie
zakończyły się bez zgłoszonego wyjątku. ARD i wyjęty X_B pozostały bez zmian.
Nie powstał zaakceptowany STEP; te czasy nie mierzą udanej konwersji.
Raport nie ustala, czy przyczyną był format, brak przygotowania, dostępność
komponentu czy inny warunek środowiska. Nie wyciągamy wniosku o licencji.

Dokładniejsze przejście przez SPAIConverter::Convert (SPAIInterop RVA 0x50a0)
i dalsze bloki DoConvert (0x5230–0x5344) ujawniło pomijane w v1 przygotowanie:
`SPAICConverter_CacheAcisSessionValues` przy 0x52a6, wywołanie pomocnicze
przy 0x52d2 oraz obsługę stanu sesji i błędów. Ich pominięcie jest potwierdzone;
nie dowodzi to jeszcze, że odpowiadało za wynik Failure.

Wersja 2 wywołuje **całą oryginalną metodę C++ Convert** na oryginalnych
obiektach, zamiast bezpośrednio SPAICConverter_Convert. Kontrakt MSVC x64:
RCX=konwerter, RDX=ukryty wskaźnik wyniku, R8=źródło, R9=cel; wynik jest
4-bajtowym SPAIResult, a RAX zwraca ten sam wskaźnik. Potwierdza to kod
wywołujący w SLInterop, konstruktor wyniku przy RVA 0xa8f0 oraz zapis/zwrócenie
wyniku przy 0x5334/0x5336 i 0x51fa. Adapter sprawdza zwrócony adres.

Dodano oryginalne StartLog/StopLog i konstruktor/destruktor SPAIFile.
Surowy dziennik pozostaje na stacji; raport obejmuje jedynie jego wielkość
i liczniki stałych słów diagnostycznych. Słowa nie są traktowane jako diagnoza.
Wynik v2 wymaga ponownej próby Windows. W chmurze sprawdzono kontrakty eksportów
z dostarczonymi DLL oraz 7 przenośnych kontroli z atrapami funkcji natywnych.

## Wynik próby v2

Raport użytkownika potwierdza przejście przez `original_SPAIConverter_Convert_CPP`
na tym samym ARD i X_B co w v1 (zgodne skróty wejść). Convert nadal zwróciło
`0x01000001` / `Failure`, bez zgłoszonego wyjątku lub częściowego powodzenia.
StartLog, StopLog i sprzątanie zakończyły się poprawnie. Cały test trwał
0,303470 s, a nieudane Convert 0,013661 s. Nie powstał zaakceptowany STEP.
Źródła pozostały bez zmian. Poprawa wywołania nie rozwiązała problemu;
przyczyna nie jest jeszcze znana.

Powstał lokalny `translator_native.log`, 1045 bajtów. ZIP zawiera jedynie JSON
z licznikami słów (m.in. jedno `failure`), bez treści dziennika. Na podstawie
takiego podsumowania nie można ustalić przyczyny ani wykluczyć problemu
komponentu czy przygotowania sesji. Następny krok to odczyt istniejącego
dziennika z tej próby, bez ponawiania tego samego testu lub tworzenia kolejnej
wersji paczki przed poznaniem nowych danych. Nie zmieniono czytnika MiniCam.

## Treść dziennika v2 i próba v3

Dostarczony dziennik zawiera konkretny komunikat:
`Warning : SPAXParasolid library not loaded.` Źródło jest opisane jako
PARASOLID, cel jako STEP; wynik pozostaje Failure. To potwierdza zgłoszenie
niezaładowanej biblioteki przez translator. Nie rozstrzyga, czy plik nie
istnieje, brakuje zależności, czy zawiodło inne przygotowanie komponentu.
Wcześniejsza obserwacja procesu SolidLink pokazała SPAXParasolid.dll w
`code/bin`; nie oznacza to jej dostępności w procesie testowym.

W v2 `os.add_dll_directory` i flagi LoadLibraryEx dotyczyły naszych wywołań.
Nie zapewniało to takiej samej ścieżki dla ewentualnych wywołań zwykłego
LoadLibrary wewnątrz bibliotek producenta. W v3 osobny proces testowy ustawia
SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS), rejestruje oryginalny
folder SolidLink i po inicjalizacji jawnie ładuje SPAXParasolid.dll oraz
SPAXStep.dll po pełnych ścieżkach. Nie zmienia PATH ani ustawień Windows.
Niezaładowany moduł zatrzymuje próbę przed konwersją i zapisuje istnienie pliku,
ścieżkę, WinError oraz opis błędu. Uchwyt załadowanej biblioteki pozostaje
przechowywany do końca procesu testowego. Raport logu rozpoznaje teraz także
konkretne ostrzeżenia o tych dwóch modułach, bez kopiowania surowego tekstu.

Hipoteza dotycząca wyszukiwania DLL wymaga sprawdzenia na Windows. Samo jawne
załadowanie DLL nie dowodzi poprawnej rejestracji komponentu ani działania
konwersji. W chmurze przeszło 10 przenośnych testów, w tym obsługa błędu
zależności Windows i odczyt dokładnego ostrzeżenia. Nie wykonano natywnego
ładowania i nie zmieniono ścieżki importu w MiniCam.

## Wynik próby v3 — licencja niedostępna w procesie testowym

Raport użytkownika potwierdza załadowanie obu bibliotek po pełnych ścieżkach:

| Biblioteka | SHA-256 |
| --- | --- |
| SPAXParasolid.dll | be5e18d3db02317501d949f5e006ba3b8d95e3118c649fe795e5b945ec5d5da2 |
| SPAXStep.dll | c121ce68868a80a133c2d15db42d5cc862c47fd0ce2c1736a3f4d0685ed85af4 |

Lista ostrzeżeń o niezaładowanych bibliotekach jest pusta. Convert zwraca
teraz `0x0100000c` / `Licence Not Available`, bez zgłoszonego wyjątku i bez
częściowego powodzenia. Zatem poprawka ładowania przesunęła próbę dalej;
nie potwierdza to jeszcze gotowości samej translacji. Czas całkowity wyniósł
0,449985 s, a nieudanej konwersji 0,037699 s. Źródła pozostały bez zmian.
Nie powstał zaakceptowany STEP. Dziennik lokalny ma 1467 bajtów, a podsumowanie
zawiera trzy wystąpienia `error` i jedno `licence`.

Wynik dotyczy samodzielnego procesu tego testu. Nie dowodzi nieaktywności
licencji ALPHACAM, braku uprawnień do konkretnego formatu ani wystarczalności
samego uruchomienia CLS/ALPHACAM. Nie wskazuje jeszcze, którego składnika lub
kierunku translacji dotyczy brak licencji. Potrzebna jest treść nowego
`translator_native.log` z wyniku v3 przed kolejną zmianą sposobu uruchamiania.
Nie przygotowujemy kolejnej identycznej próby ani nie zmieniamy bieżącego
czytnika MiniCam na tę niepotwierdzoną ścieżkę.

## Dziennik v3 i ustalony kierunek dalszej pracy

Dziennik v3 wymienia `Product "GENERIC_R" is not authorized`, brak licencjonowania
komponentu GENERIC_GRAPH_R oraz `Product "STEP_W" is not authorized` i
`Product "STEP_GRAPH_R" is not authorized`. To są komunikaty procesu naszego
testu, nie dowód nieaktywności licencji w samym ALPHACAM ani diagnoza uprawnień
całej instalacji. Samo poprawne załadowanie DLL i oryginalny SPAIConverter nie
zapewniły autoryzacji potrzebnej do tej próby.

Użytkownik potwierdził: **kontynuujemy konwersję całej bryły, bez uruchamiania
ALPHACAM**, a nie powrót do odczytu krawędzi i odbudowy geometrii. Krótko
rozważana optymalizacja metadanych czytnika została wycofana przed publikacją.
Nie podłączono API uruchomionego ALPHACAM. Aktualny czytnik i zapis pozostają
zachowane; próby dotyczą osobnej ścieżki konwersji.

## Próba v4 — pełna funkcja SLInterop i zwykła sesja CLS

Poprzednie próby kończyły się na bezpośredniej warstwie Spatial. V4 używa
oryginalnego `SLInterop::RunInteropStandard` (RVA 0x23030), który sam wykonuje
przygotowanie produktu, buduje dokumenty i opcje, uruchamia log, wywołuje
konwersję oraz sprząta. Nie wydzielamy jego materiału autoryzacyjnego ani nie
odtwarzamy go w swoim kodzie. Nie modyfikujemy kontroli i wyników licencji.
Przez cały czas rodzic utrzymuje zwykłe, aktualnie otwarte połączenie CLS
przez istniejący `feature_cls_session_14.ps1`, z oryginalnym sprawdzeniem wersji,
publicznymi wywołaniami, walidacją połączenia i normalnym zamknięciem. Wyjście
STEP jest akceptowane dopiero po potwierdzonym zakończeniu CLS.
Otwarcie CLS samo w sobie nie jest dowodem uprawnienia do STEP_W.

`solidlink_bulk_worker.py` wymaga przypiętych SHA-256 SLInterop/SPAIInterop/C,
sprawdza adresy eksportów i oryginalnych importów MFC. 96-bajtową pamięć
SLInterop z obszarami kontrolnymi uzasadniają konstruktor 0x24550, przypisanie
0x2ee0 i destruktor 0x24620: pola dochodzą do 0x5c przy wyrównaniu 8.
Oryginalny konstruktor i destruktor zarządzają obiektem. Nie odczytujemy ani nie
ustawiamy prywatnych pól licencji. Publiczny SetNamedParameters otrzymuje pustą
opcjonalną listę. Trzy argumenty CStringW tworzy oryginalny konstruktor MFC
(import SLInterop IAT 0x2ceb8 / ordinal 290), a zgodność ścieżek sprawdza
GetLength (0x2cf30 / 5674). RunInteropStandard przejmuje argumenty przez
wartość i sam je niszczy (0x232e7–0x232fe); adapter nie zwalnia ich ponownie
po przekazaniu. Przed przekazaniem w razie błędu sprząta je oryginalnym
destruktorem z IAT 0x2cf80 / 1033. Wszystkie te importy wskazują na zweryfikowane
ordinały tej samej załadowanej mfc140u.dll. Nie stosujemy własnych definicji
klas MFC ani podmienionych implementacji producenta.

To eksperymentalny adapter konkretnego binarnego interfejsu, nie potwierdzony
kontrakt SDK do ogólnej dystrybucji. W chmurze sprawdzono symbole/importy
z dostarczonymi DLL i przenośne granice procesu z atrapami. Nie wykonano v4
na Windows, nie potwierdzono autoryzacji ani czasu udanej konwersji.
Sukces funkcji bez nowego kompletnego STEP, zgodności wejść, poprawnego
sprzątania i późniejszego sprawdzenia geometrii nie wystarcza do integracji.
