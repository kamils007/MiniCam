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
