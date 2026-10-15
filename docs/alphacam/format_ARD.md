# Rekord Feature i jego opakowanie w ARD

Zakres: zaobserwowany `Licom-APS File V1.94`, jeden rekord dodatku
`LicomUKRJMParaSolid`, wersja fragmentu 3. Nie jest to pełna specyfikacja ARD.
Wszystkie poniższe liczby wielobajtowe są little endian.

## Zapis przez DLL

`LONG ObjectQueryWrite2(VARIANT* context, char* name, LONG version)`:
przekazujemy `version=0`, 64-bajtowy bufor nazwy otoczony znacznikami.
Wynik 1 i poprawna zakończona NUL nazwa oznaczają zgłoszenie obiektu.

`LONG ObjectDoWrite2(VARIANT* context, FILE* stream, FwriteFn write)`:
wynik **0 oznacza powodzenie**, 1 błąd; to nie HRESULT.
FwriteFn odpowiada natywnemu `size_t fwrite(const void*, size_t, size_t, FILE*)`.
Przekazać bezpośredni wskaźnik fwrite z importów DLL.

| Import CRT | RVA komórki IAT |
| --- | --- |
| _wfopen_s | `0x13CE28` |
| fwrite | `0x13CE38` |
| fflush | `0x13CE20` |
| fclose | `0x13CE68` |

Pod tymi adresami są wskaźniki funkcji, nie kod funkcji. FILE* otworzyć
przez ten sam import `_wfopen_s` w trybie `wbx` i zamknąć tym samym CRT.
Sprawdzić wynik zapisu, fflush i fclose, niepusty plik i zgodny nagłówek.
Nie przekazywać FILE* utworzonego przez statyczny CRT MiniCam.

## Zawartość feature_object.bin

| Pole | Format |
| --- | --- |
| Wersja | uint32 = 3 |
| Liczba opisów źródeł | uint32 |
| Każdy opis | uint32 ID, tekst ścieżki, tekst formatu, uint32, uint8, uint32 |
| Każdy tekst | uint32 liczby bajtów z końcowym NUL, następnie bajty tekstu |
| Geometria | uint32 długości, dokładnie tyle bajtów binarnego Parasolid |
| Końcówka | Metadane brył; zachować w całości, nie są w pełni rozpoznane |

Nie zakładać UTF-8 opisów ANSI; czytnik referencyjny zachowuje ich surowe bajty.
W teście 4.9 końcówka ma 12 bajtów, a pierwsze uint32 wskazuje jedną bryłę.
Nie traktować 12 bajtów jako uniwersalnego rozmiaru dla wszystkich modeli.
Pole uint32 z długością geometrii nie jest długością całego rekordu.

## Opakowanie w dokumencie

Przed końcowym rekordem `0x2F` (wskaźnik zwrotny) i `0x00` (koniec)
istniejący writer umieszcza:

| Pole | Format |
| --- | --- |
| Znacznik | uint8 = 0x22 |
| Nazwa dodatku | uint16 = 20, następnie 20 bajtów `LicomUKRJMParaSolid` wraz z NUL |
| Liczba obiektów | uint32 = 1 |
| Długość fragmentu | uint32, rozmiar całego feature_object.bin |
| Pole zarezerwowane | uint32 = 0 |
| Fragment | Surowe bajty ObjectDoWrite2, bez zmian |

`LicomUKRJMParaSolid` ma 19 znaków; długość 20 uwzględnia NUL.
Całe opakowanie ma 35 bajtów oprócz fragmentu.
Zachować nagłówek dokumentu, miniaturę, warstwy, geometrię 2D i pozostałe rekordy.
Końcowy wskaźnik odnosi się do wcześniejszego identyfikatora; jeśli zmieni się
jego położenie, wskaźnik trzeba przeliczyć. W teście 6 dodanie przed samą
końcówką nie zmieniło położenia tego identyfikatora.

Test 6: 62 907 bajtów pliku bazowego + 67 498 fragmentu + 35 opakowania
= 130 440 bajtów. Fragment zaczyna się pod offsetem 62 936.
Liczby te opisują próbkę, a nie stałe do implementacji.
Kod C++ generuje dokument przez `writeArd(..., solidRecord)`;
nie musi korzystać z wcześniejszego dokumentu jako szablonu.

## Weryfikacja

Czytnik `object_serialization.parse_fragment` sprawdza granice pól, wersję,
ciągi zakończone NUL i zakres Parasolid; resztę zachowuje jako nieznane metadane.
`tools/ard/ArdRecordTest.cpp` sprawdza opakowanie i zachowanie bajtów 2D.
Kontrola struktury nie zastępuje otwarcia nowego dokumentu w ALPHACAM-ie.
