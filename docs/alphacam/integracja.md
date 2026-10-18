# Przekazanie integracji bryły do ARD

Stan integracji GUI: 18.10.2026. Aktualna ścieżka to opcjonalny
[moduł ALPHACAM](../../modules/alphacam/README.md), który uruchamia test 15
w osobnym procesie, zwraca zweryfikowany rekord i zachowuje writer 2D MiniCam.
[Test 15](feature_test_15.md) potwierdził jeden przebieg STEP → Parasolid → ARD
bez wykrytego Acam.exe na instalacji 2025.4. Nowe wywołanie z GUI wymaga
jeszcze próby na Windows. Starszy host C++ nie jest używany przez edytor.

Poniżej historyczny stan przekazania z 16.10.2026: Python test 4.9,
Feature.dll 2025.3.0.200. Nie jest to instrukcja konfiguracji obecnego modułu.
Ten dokument pozwala kontynuować pracę bez historii rozmowy.

## Co zostało potwierdzone

Własny proces Windows x64, przy zamkniętym Acam.exe, załadował Feature.dll,
dostarczył jej minimalny host COM, zaimportował jedną bryłę Parasolid,
zapisał `roundtrip.x_b`, a następnie rekord obiektu `feature_object.bin`.
Test 4.9 zakończył się kodem 0; rekord ma 67 498 bajtów.
Zestawienie liczb i skrótów jest w [verification.json](verification.json).

Test 5 wstawił rekord do oryginalnego ARD. Test 6 wstawił go do `1_test.ard`
z programu użytkownika, bez użycia pamięci podręcznej oryginalnego ARD.
Użytkownik otworzył oba pliki w ALPHACAM-ie i zwrócił ich ponowny zapis.
Analiza tych zapisów w poprzedniej sesji potwierdziła zachowanie jednej bryły;
w teście 6 program utworzył własną pamięć podręczną.
To potwierdzenie dla jednej próbnej bryły, nie dla wszystkich modeli i wersji ARD.

## Pliki i ich role

| Lokalizacja | Rola |
| --- | --- |
| `tools/alphacam_reference/probe_feature_2025.py` | Sprawdzona kolejność ładowania, hosta, importu, eksportu i zapisu rekordu; proces potomny z limitem czasu |
| `com_host.py`, `layer_host.py` w tym katalogu | Host aplikacji, rysunku, panelu i warstw; referencje i własność obiektów COM |
| `host_contract_common.json` | GUID-y, DISPID-y, pełna kolejność i typy metod pięciu interfejsów hosta |
| `com_typeinfo.py` | Odczyt typu rzeczywistego obiektu Feature i rozpoznawanie kontraktów importu/zapisu |
| `feature_texts.py` | Katalog gettext i tabela natywnych CString |
| `object_serialization.py` | ObjectQueryWrite2, ObjectDoWrite2, zgodny CRT i ograniczony czytnik rekordu |
| `host_environment.py`, `native_trace.py` | Odczyt ustawień hosta oraz diagnostyka wyjątków |
| `tools/ard/FeatureHost.cpp` | Istniejący port C++ ścieżki testu 4.9, osobny program Windows |
| `app/ArdSolidExport.cpp` | Istniejący most GUI → eksport bieżącego modelu → proces pomocniczy |
| `core/ArdExport.cpp` | Istniejący zapis dokumentu oraz opakowania rekordu bryły |
| `tools/ard/ArdRecordTest.cpp` | Kontrola zachowania rekordu, geometrii 2D i wskaźnika końcowego |

Sekwencja probe’a Feature pozostaje wzorcem 4.9. W teście 4.1 rozszerzono
com_host.py o DisableUndo i jego testy. `source_manifest.json` zachowuje
pierwotne oraz bieżące skróty zmienionych plików i opisuje rozszerzenie.

## Granica obecnego portu C++

Przy przekazaniu materiałów znaleziono już port w gałęzi `codex/zapis-ard`,
commit `2168cc967e0db55fdb5ef7289e821c95dfbf6989`, PR #4.
Jego [ARD-NATIVE.md](../ARD-NATIVE.md) opisuje wcześniejsze sprawdzenie
kompilacji MSVC i natywnego importu Parasolid. W ramach samego przekazania
materiałów nie powtarzano tych prób na Windows.

MiniCam pracuje na `TopoDS_Shape` z Open CASCADE. Most GUI eksportuje go do STEP,
a sprawdzona ścieżka Feature.dll przyjmuje Parasolid. Dokumentacja portu
odnotowuje błąd bezpośredniego importu STEP `0x80020009`.
**Zapis dowolnej aktualnej bryły edytora do ARD nie jest jeszcze ukończony.**
Nie wolno zastąpić bieżącej bryły starym rekordem testowym ani uznać zapisu 2D
za udany zapis bryły. Należy zachować położenie i wszystkie zmiany modelu.

## Następne prace

1. Na Windows porównać host C++ i wzorzec Python na tym samym wejściu Parasolid.
   Oba powinny zaimportować jedną bryłę, wykonać ObjectDoWrite2 i poprawnie
   zamknąć plik. Porównywać geometrię i metadane, uwzględniając ścieżkę i datę,
   zamiast wymagać identyczności całego rekordu bajt w bajt.
2. Przenieść do C++ i sprawdzić przekazanie **bieżącej** bryły OCCT przez STEP
   do SolidImport/SolidLink. Python test 3.2 potwierdził STEP → Parasolid
   przy otwartym ALPHACAM 2025. Zamknięty program i pełny most GUI wymagają
   dalszej weryfikacji. Najpierw wykonać test bryły z tego STEP-a do ARD.
3. Wygenerowany przez C++ dokument otworzyć w ALPHACAM-ie, zaznaczyć bryłę,
   obrócić ją, zapisać ponownie i porównać geometrię.
4. Dopiero potem rozszerzać próbę o zmiany położenia, wiele brył, inne modele,
   zależności w osobnym katalogu i kolejne wersje bibliotek.

## Lokalne składniki

Własne zgodne Feature.dll, PSKERNEL.dll, Feature.po, biblioteki zależne
oraz aktualny odczyt `host_environment_2025.json`. DLL-e pozostają dynamicznymi
zależnościami nawet przy statycznym Qt/OCCT. `FILE*` użyty do ObjectDoWrite2
musi pochodzić z CRT biblioteki, niezależnie od `/MT` lub `/MD` MiniCam.

Kopiowanie DLL do osobnego katalogu i pełna niezależność od folderu instalacji
nie zostały potwierdzone testem 4.9. Sam brak działającego Acam.exe tego nie dowodzi.

Szczegóły: [COM i ABI](interfejsy_COM.md), [format rekordu i ARD](format_ARD.md),
[uruchomienie wzorca](../../tools/alphacam_reference/README.md).

Nowy etap: [SolidImport/SolidLink — wyniki testów STEP](solidimport.md).
Test 3.2 utworzył 99 728-bajtowy tekstowy Parasolid; log translatora potwierdza
Success i 1 bryłę stałą. Konwersja korzystała z otwartego ALPHACAM-u.
[Test 4 — ten Parasolid do ARD](../../tools/step_ard/README_PL.md) jest przygotowany;
nowe wykonanie Feature i otwarcie wynikowego ARD pozostają do sprawdzenia.
