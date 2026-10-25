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
