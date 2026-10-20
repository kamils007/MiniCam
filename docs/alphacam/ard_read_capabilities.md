# Raport możliwości odczytu ARD — 20.10.2026

Źródło: dostarczony przez użytkownika `translator_metadata.json`, schemat
`minicam-ard-import-capabilities-v1`. To odczyt metadanych, nie test konwersji.

Konfigurację odczytano z ustawień eksportu MiniCam w CurrentUser. Znaleziono
wszystkie cztery biblioteki; ich skróty pozostały niezmienione podczas testu.

| Plik | Wersja | SHA-256 |
| --- | --- | --- |
| Feature.dll | 2025.4.1.200 | 5bbfbdbd41c32dde36dfc81db3a7a022b9e3688622f7005130cc207bc6638438 |
| SolidImport.dll | 2025.4.1.100 | b143da4e3f8771e04aebb7d20334244900866148dc83d242a3e0ad6938c5f82c |
| SolidLink.exe | 2025.4.2616.108 | 71d48d786a76aa16031be28c9ebbe4c9b95555643fd79f475b5a6e76587ce83c |
| SLInterop.dll | 2025.4.2616.108 | 30c67178c5224d24ce1ab13ff759e5d4795919314d8637d3b4c472224fc56442 |

Feature odpowiada istniejącemu CURRENT_SHA256 w `feature_builds.py`. Nie trzeba
zmieniać przypiętych skrótów ani ponownie prosić użytkownika o foldery.

## Ustalone deklaracje

- Eksport natywny `ObjectRead3` istnieje. Raport nie określa jego sygnatury
  natywnej; nie wywoływać go na podstawie domysłu.
- `ISolidFeatures.ImportParasolid` (DISPID 28) ma trzy wejściowe BSTR i jeden
  parametr typu użytkownika. `ImportAs` (19) także istnieje. Pełny typ parametru
  użytkownika i semantykę argumentów sprawdzać w istniejącym kontrakcie,
  nie utożsamiać dowolnego enum z ustaloną wartością liczbową.
- `ISolidBody` deklaruje `MoveG`, `RotateG`, `ScaleG`, `CopyG`, `Faces`, `Edges`,
  `Vertices` i metody zapisu `Save` oraz `SaveACIS`.
- `ISolidBodies` deklaruje `Save` i `SaveACIS`.
- Nie znaleziono deklaracji `SaveSTEP`, `ExportSTEP` ani `SaveIGES` w odczytanych
  interfejsach Feature. Nie jest to dowód nieistnienia innej drogi translacji.
- `SLInterop.RunInteropIntermediateStep` jest nazwą eksportu C++; sama nazwa
  nie dowodzi zapisu pliku STEP ani nie ustala ABI lub obsługiwanych kierunków.
- Błędy TYPE_E_CANTLOADLIBRARY przy SolidImport/SLInterop dotyczą odczytu
  biblioteki typów. Eksporty PE zostały odczytane; nie są to wyniki konwersji.

## Granica wyniku

Potwierdzono istnienie interfejsów obsługi Parasolid w Feature, a nie ich
uruchomienie przez MiniCam w tej próbie. W szczególności `ObjectRead3` nie
zostało wywołane, a model OCCT nie został utworzony. Zapis SaveACIS nie zapewnia
sam w sobie odczytu przez standardowy OCCT. Kolejny test musi korzystać
z działającej inicjalizacji/licencji istniejącego runtime i weryfikować
rzeczywisty wynik geometrii. Nadal brak potwierdzonej drogi Parasolid → OCCT/STEP.
