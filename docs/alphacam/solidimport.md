# Rozpoznanie importu STEP — SolidImport 2025

Analiza statyczna 15.10.2026. Konwersji na Windows jeszcze nie wykonano.

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
Dokładne typy argumentów, inicjalizacja i zwracany wynik nadal wymagają
weryfikacji przed natywnym wywołaniem. Nie wywoływać go na podstawie samej nazwy.

Zależności statyczne SolidImport obejmują MFC, runtime MSVC i biblioteki
systemowe. Feature.dll i PSKERNEL.dll nie występują w jej zwykłej tablicy
importów. Nie oznacza to braku zależności pośrednich: translacja korzysta z COM,
a dalsza ścieżka importu może korzystać z hosta i innych komponentów.

## Następny krok

Na komputerze użytkownika odczytać rejestrację `SolidLink.Document.*` przez
[diagnostykę 1](../../tools/solidimport/README_PL.md). Raport poda CLSID,
rzeczywisty serwer oraz lokalizację ewentualnej biblioteki typów.
Następnie sprawdzić kontrakt translatora i przygotować izolowany test
STEP → Parasolid na dostarczonym modelu. Obecny etap nie potwierdza jeszcze
konwersji, działania przy zamkniętym ALPHACAM-ie ani przenośności translatora.

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
Test 2 nie został jeszcze wykonany na Windows.
