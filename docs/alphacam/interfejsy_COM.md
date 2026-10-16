# Host COM i ABI dokładnie sprawdzonej wersji

Źródło kontraktów hosta: Acam.exe 2025.3.0.103, TYPELIB/694/2057,
AlphaCAMRouter x64. Pełne 627 opisów metod pięciu interfejsów znajduje się
w `tools/alphacam_reference/host_contract_common.json`.
Nie budować skróconej klasy z samymi używanymi metodami: pozostałe pozycje
tablicy muszą zachować swoje przesunięcia i sygnatury.

| Interfejs | IID |
| --- | --- |
| IAlphaCamApp | `88a2ff74-4309-11d2-9861-00104b4af281` |
| IDrawing | `1a172592-4565-11d2-9866-00104b4af281` |
| IFrame | `0691d8c8-4ae8-11d2-9876-00104b4af281` |
| ILayer | `e1881172-47ba-11d2-986f-00104b4af281` |

IID kolekcji ILayers i wszystkie typy wyjściowe odczytywać z kontraktu JSON.
Host implementuje także IUnknown i IDispatch; obca IID zwraca E_NOINTERFACE.
Nieobsługiwane wywołanie jest logowane i zatrzymuje proces próbny kodem 80.

## Sekwencja

1. Zweryfikować SHA-256 i x64, zainicjalizować COM STA, ustawić katalogi
   zależności i załadować DLL. Wszystkie RVA poniżej są względne do bazy modułu.
2. `GetSolidFeatures(void** out)` dostarcza obiekt; nie traktować wartości
   zwrotnej jako wiarygodnego HRESULT. Uzyskać jego IDispatch przez QueryInterface.
3. `ObjectNumberOf(VARIANT* appContext, LONG flags)` z `flags=0` przekazuje
   `VT_DISPATCH` wskazujący hosta IAlphaCamApp. DLL zachowuje referencję fallback.
   To ścieżka użyta w eksperymencie; nie pełne InitAlphacamAddIn.
4. Przygotować tabelę napisów przez funkcje MFC z tablicy importów DLL.
5. Rozpoznać kontrakt ImportAs z ITypeInfo i wywołać import. Przy Invoke
   argumenty DISPPARAMS są w kolejności odwrotnej do deklaracji.
6. Sprawdzić wynik i Bodies.Count > 0, wykonać Bodies.Save, potem zapis rekordu.
7. Zwolnić obiekty Feature; tabela napisów pozostaje ważna podczas Release.
   Przywrócić pierwotny wskaźnik tabeli, zniszczyć CString przed FreeLibrary.
   Host musi przeżyć także FreeLibrary ze względu na referencję fallback DLL.

## Typy i stan

Na Windows x64: wskaźnik 8 bajtów, VARIANT 24, HRESULT/LONG 4,
SHORT/VARIANT_BOOL 2, VARIANT_TRUE = -1. `bool`, `wchar_t*` i dowolny
bufor UTF-16 nie zastępują VARIANT_BOOL/BSTR. BSTR tworzyć i zwalniać przez
OleAut32. Getter obiektu przekazuje zatrzymaną referencję COM.

IAlphaCamApp: ActiveDrawing, Frame, ProgramLetter (`SHORT`, Router 'R'=82),
InAPICall, SpecialKey (`LONG`, pełne 32 bity), SetUndoCommandName.
SpecialKey pochodzi z odczytu własnej instalacji; nie używać wartości domyślnej.

IDrawing: ScreenUpdating, ThreeDViews, lista warstw oraz Redraw,
RedrawShadedViews i ZoomAll. IFrame: ProjectBarUpdating.
Operacje wyświetlania dotyczą pustego zbioru widoków, a etykieta Cofnij
nie implementuje stosu cofania. Lista warstw musi zachowywać wspólny stan,
nazwy Unicode, aktywną warstwę i właściwości między wywołaniami.
Pełny szerszy zakres wzorca znajduje się w `layer_host.py`; port C++ obsługuje
dotychczas wykorzystaną ścieżkę natywną i nie jest pełnym zamiennikiem API.

## Wersja i napisy

Feature.dll 2025.3.0.200 SHA-256:
`4bd2cbb8606a8a5c9dc5c44b9fb0de1e34ecfb01438b6a1b13898873f6878fec`.

Feature.po SHA-256:
`5ebfd4fedec46583fa72fd2e95862a0b3fbe7eb41b6a9e49ae12895176130776`.

`feature_texts.py` definiuje 26 grup i 583 CString; wskaźnik tabeli ma
RVA `0x186548`. CString jest natywnym obiektem MFC, nie wskaźnikiem do
samodzielnie zaalokowanego napisu. Nagłówki tablic zawierają liczniki uint64.
Brakujące 90-83, 120-28, 120-29 oraz siedem wpisów zewnętrznych mają poprawnie
skonstruowane puste CString. Pełnego inicjalizatora nie wywoływać równocześnie
z tym adapterem. Plik DLL ani instrukcje maszynowe nie są zmieniane.

| Eksport | RVA |
| --- | --- |
| GetUIVersion | `0x38A10` |
| GetSolidFeatures | `0x389C0` |
| ObjectNumberOf | `0x40CE0` |
| ObjectQueryWrite2 | `0x41370` |
| ObjectDoWrite2 | `0x40050` |

Eksporty wyszukiwać po nazwie przez GetProcAddress, RVA służą weryfikacji.
Zmiana SHA-256 oznacza konieczność ponownej analizy także dla identycznej
nazwy pliku i podobnego numeru wersji.
