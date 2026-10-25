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
