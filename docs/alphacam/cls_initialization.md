# CLS: wynik analizy bibliotek z instalacji użytkownika

**Stan po teście 7.2:** CLS i publiczny interfejs SolidLink 2025.4 działają
bez zaobserwowanego Acam.exe. TranslateSolid zwróciło 1 i nie utworzyło pliku.
Nadal nie potwierdzono Core:Solids ani konwersji STEP bez procesu aplikacji. Poniżej zachowano
kolejne wyniki diagnostyki; wcześniejsze ograniczenia opisują ówczesny stan.

Stan: 17.10.2026. Odczyt statyczny, bez wykonania bibliotek na Windows.
Nie potwierdzono konwersji STEP przy zamkniętym ALPHACAM-ie.

## Sprawdzone pliki

| Plik | Architektura i środowisko | Dostępne wejście |
| --- | --- | --- |
| CLS2005Interface.dll | Natywny x86, MFC80 / MSVCR80 | get_icls |
| SWLicense.dll | Natywny x86, MFC100 / MSVCR100; wersja 9.0.0.101 | Cztery funkcje dialogów rejestracji i informacji o licencji |
| CLS.dll | x64, mieszany C++/CLI, CLR v4.0.30319; wersja 2025.1.2507.3 | ClsCommand, ClsClose, LogLine, FlushLog oraz dwa operatory przypisania Hasp64 |

SHA-256 odpowiadają raportowi z testu 5:

- CLS2005Interface.dll: `cbf60ac2ceba3c6ee0d87660e97950626e009e2a4ff0611774d43ad0b26d8bdc`
- SWLicense.dll: `ff529e5f6f302aade46ef1f9ea117cea93e2889ab831115e151ac9d15af34c48`
- CLS.dll: `5d1e001654690774794b2d6093491e6b0b51aa670b6694f6c98883c29b8f02c8`

Nie należy interpretować x86 jako AnyCPU: oba małe pliki nie mają nagłówka CLR.
Nie można ich bezpośrednio załadować do naszego procesu Python x64.
Nie ustalono, czy mają zastosowanie do obecnej ścieżki licencji aplikacji x64.

## Rzeczywisty interfejs CLS.dll

W tabeli eksportów nie ma DllGetClassObject ani funkcji tworzącej ILicense.
Nie znaleziono publicznej zarządzanej klasy dostarczającej takiego adaptera.

Metadane C++/CLI i nazwa dekorowana wskazują na:

```cpp
short __cdecl ClsCommand(void* pData, short nSlot);
```

Metoda zarządzana znajduje się pod RVA 0x7AD8; eksport jest natywnym thunk.
Modyfikator sygnatury rozwiązuje się do
System.Runtime.CompilerServices.CallConvCdecl. Nie wystarcza znajomość samego
prototypu: nie ustalono kontraktu bufora pData, znaczenia wszystkich slotów,
wersjonowania komunikatów i sposobu powiązania sesji z funkcjami produktu.

Oryginalne ClsCommand tworzy CEventCls i wątek komunikatów, blokuje mutex,
przekazuje bufor do warstwy komunikacji i wysyła żądanie. Jego wartość zwrotna
nie może być utożsamiana z pozytywnym sprawdzeniem Core:Solids.
Nie wywoływano go z pustym lub odgadniętym buforem. Nie odtwarzano materiału
autoryzacyjnego, nie zmieniano sprawdzeń i nie modyfikowano kodu producenta.

## Dlaczego samodzielna DLL nie zastępuje jeszcze hosta

Dostarczony Acam.exe 2025 ma SHA-256
`2d11c21b884be255163a8367fa53b624151a6794527c757cd8eae4112f8f5f38`.
Przez RTTI typu ATL::CComObjectNoLock<CLicense> ustalono jego tablicę metod
ILicense pod RVA 0x8FC6D0. IsFeatureLicensed jest w slocie 9, RVA 0x355200;
odpowiada to przesunięciu 0x48 w kontrakcie ILicense z TYPELIB.

Implementacja rozpoznaje prefiks Core:, m.in. nazwę Solids, i odczytuje
stan funkcji produktu w aplikacji (pomocnicza procedura RVA 0x31FAF0).
Nie jest to eksport CLS.dll przyjmujący nazwę Core:Solids. Wniosek ograniczony
do tej implementacji: udostępnienie prawdziwego ILicense z zamkniętego hosta
wymaga również poprawnej inicjalizacji stanu produktu, nie samego LoadLibrary.
Nie dowodzi to, że niezależna implementacja przez SDK jest niemożliwa.

## Co wiadomo, a co pozostaje niepotwierdzone

- Test 5 zakończył odczyt przy zamkniętym ALPHACAM-ie. Brak rejestracji serwera
  klasy License w obu sprawdzonych widokach rejestru.
- Skan plików testu 5 osiągnął limit 12 000 wpisów. Nie jest kompletnym
  spisem instalacji i nie wyklucza istnienia dalszego adaptera lub SDK.
- Native CLS.dll x64 istnieje i ma oryginalny interfejs komend.
- Brak sprawdzonego kontraktu klienta CLS/EPA i niezależnego tworzenia ILicense.
- Nie przygotowano kolejnego testu konwersji, który pozorowałby obsługę tego
  interfejsu. Nie ma jeszcze testu 6 ani potwierdzonej konwersji bez Acam.exe.

### Dalsze dane — publiczny klient zarządzany

Po otrzymaniu zrzutu folderu oraz LicManaged.dll, lic.pdb i ClsInit.exe
znaleziono publiczny interfejs PlanitCls.License w LicManaged.dll 2025.1.0.0.
To interfejs połączenia CLS, nie COM ILicense z Acam.exe.

LicManaged.dll x64, SHA-256
`0b30282755e72feefdc3b43e50c0628749e49a5bd97fa06dabc002a0851a4827`,
udostępnia publiczne statyczne OpenVersionedLicenseConnection,
ValidLicenseConnection, GetCurrentSessionId, GetClsProduct, SetClsPath
i CloseLicenseConnection. Sygnatury i wartości enumów zachowano w
tools/solidimport/cls_managed_api_reference.json. Kod produktu Alphacam = 2,
tryb komunikatów Silent = 2, wynik otwarcia Ok = 1, walidacji Ok = 101.
Są to wartości oryginalnych enumów, nie własne stany uprawnień hosta.

ClsInit.exe x64 ma SHA-256
`d594c321383c33ca4eb12eedf3f95cb2adfc269484cdea986fbe5973737ef252`.
Dostarczony lic.pdb ma GUID c79b340a-d569-453e-9c57-150105fcf2ed, age 48.
Nie pasuje do identyfikatorów CodeView CLS.dll, LicManaged.dll ani ClsInit.exe
i nie jest używany do przypisywania tym plikom symboli.

[Test 6](../../tools/solidimport/README_TEST_6_PL.md) otwiera rzeczywiste
połączenie dla produktu Alphacam w wersji 2025.3 przez publiczne API,
sprawdza walidację i zamyka własne połączenie. Bieżący identyfikator sesji
odczytuje przez GetCurrentSessionId. Wywołania wykonuje nowy PowerShell x64
z CLR v4, przy monitorowaniu braku Acam.exe i kontrolowaniu skrótów plików.
Nie wywołuje kodów dostępu, nie buduje ILicense i nie deklaruje Core:Solids.

Przygotowano 5 testów przenośnych odrzucających niepełny lub niezgodny wynik.
Sprawdzono składnię PowerShell gramatyką tree-sitter. Wykonanie natywne na
Windows pozostaje niepotwierdzone. Nadal nie ma pełnego testu STEP → ARD
bez Acam.exe; ten etap dotyczy jedynie publicznego połączenia CLS.

## Wynik testu 6 i poprawka 6.1 — 17.10.2026

Raport użytkownika cls_test_6_20261017_182620_245286.zip potwierdza brak
zaobserwowanego Acam.exe (5 próbek, także kontrola przed i po), zgodne skróty
trzech modułów i ich niezmienność. Proces zakończył się TypeLoadException:
nie znaleziono typu Product. Nie wywołano otwarcia ani walidacji CLS.
Nie jest to odmowa licencji ani wynik konwersji.

Ponowny odczyt tabeli NestedClass wykazał, że wszystkie cztery enumy
są NestedPublic w PlanitCls.License, np. PlanitCls.License+Product.
Poprzednia interpretacja ich jako typów najwyższego poziomu była błędna.
Wersja 6.1 rozwiązuje je przez GetNestedType(Public) i wykorzystuje pełne
nazwy typów do sprawdzenia sygnatur metod. Wynik wykonania na Windows
pozostaje do potwierdzenia.

## Wynik 6.1 i próba 6.2 — 17.10.2026

Raport cls_test_6_1_20261017_183134_535975.zip potwierdza kontrakt publicznego
API, SetClsPath=true, odpowiedź OpenVersionedLicenseConnection=UpdateExpiry
(10), brak obserwowanego Acam.exe (55 próbek oraz kontrola przed i po),
niezmienność trzech modułów i powrót z CloseLicenseConnection bez wyjątku.
Nie potwierdzono sesji i nie wywołano ValidLicenseConnection ani konwersji.

Sprawdzono oryginalne wywołanie Acam.exe RVA 0x629840: produkt 2, sesja 0,
kontrola miesiąca i roku -1/-1 przez funkcję RVA 0x7a2570. Domyślne wartości
rozwiązywane wewnątrz Acam.exe to 4 (RVA 0x247b90) i 125 (RVA 0x7a9380).
LicManaged.dll ma te same domyślne wartości, RVA 0xf570 i 0xf580; oryginalne
rozwiązanie wartości ujemnych występuje od RVA 0x4aede.

Publiczna OpenLicenseConnection w LicManaged.dll (managed RVA 0x2e30,
native wrapper RVA 0x4a8e0) ma parametry applicationName, displayError,
forProduct, checkMonth, checkYear. Sama odczytuje aktualny identyfikator sesji.
Wersjonowany wrapper RVA 0x4bfb0 zamiast tego przekazuje versionMinor+1
i versionMajor+500: dla wcześniejszych 2025/3 daje to 4/2525. Nie ustalono
jeszcze właściwej reprezentacji wersji dla tej metody; nie wolno na podstawie
jej odmowy orzekać, że licencja użytkownika wygasła.

[Test 6.2](../../tools/solidimport/README_TEST_6_2_PL.md) wywołuje publiczne
OpenLicenseConnection z oryginalnymi domyślnymi -1/-1, bez prób innych dat
i wersji. Biblioteka sama wykonuje normalną kontrolę. Raport jawnie utrzymuje
release_version_verified=false; nawet Open/Valid=Ok nie potwierdza wydania
2025.3, Core:Solids, translacji STEP ani uprawnienia do eksportu.

## Wynik testu 6.2 — połączenie potwierdzone

Raport cls_test_6_2_20261017_184400_581922.zip sprawdzono niezależnie
przez confirmed_session; raport nadrzędny i worker_result są zgodne.
OpenLicenseConnection=Ok (1), ValidLicenseConnection=Ok (101),
GetClsProduct=Alphacam (2), worker exit=0, końcowa faza completed.
SetClsPath i kontrola kontraktu publicznego API zakończyły się poprawnie.
Nie zaobserwowano Acam.exe w 37 próbkach ani przed/po próbie. Trzy
kontrolowane moduły pozostały niezmienione. CloseLicenseConnection wróciło
bez błędu; nie dowodzi to zwolnienia stanowiska po stronie serwera.

To potwierdzenie publicznej sesji CLS, nie konkretnego wydania ani funkcji.
Core:Solids, autoryzacja SolidLink i konwersja STEP nie były testowane.
Nie powstał Parasolid ani ARD. Do dalszej próby potrzebne jest jeszcze
powiązanie rzeczywistej sesji z oryginalną inicjalizacją importera;
samo potwierdzenie CLS nie dostarcza COM ILicense i nie zastępuje odpowiedzi
IsFeatureLicensed. Moduły zainstalowanego ALPHACAM-a pozostają zależnościami.

## Przygotowana próba 7 — CLS otwarte podczas bezpośredniej translacji

[Test 7](../../tools/solidimport/README_TEST_7_PL.md) utrzymuje rzeczywistą
sesję CLS podczas zwykłego wywołania publicznego COM SolidLink. To weryfikacja,
czy taki kontekst wystarczy do konwersji; nie jest jeszcze implementacją
przekazywania uprawnień między procesami ani adapterem ILicense.

Nowy worker wykorzystuje sprawdzoną sekwencję CLS z 6.2, waliduje ją przed
i po próbie, zwalnia COM przed zamknięciem CLS. Odrzuca istniejący proces
SolidLink, aby nie opierać wyniku na obiekcie pozostawionym przez inną pracę.
Monitorowany jest Acam.exe. Nie wywołuje SendLicenseString,
ConfirmParasolidLicensed, GetAccessCode ani zmian konfiguracji licencji.

Potwierdzenie konwersji wymaga natywnego TranslateSolid=0, nowego pliku
z rozpoznanym nagłówkiem Parasolid, niezmiennych wejść/modułów, zwolnienia
COM i potwierdzenia CLS przed/po oraz zamknięcia jego połączenia bez błędu.
Wynik sesji jest niezależny od wyniku translacji. Przy braku uprawnień próba
ma zwrócić oryginalny wynik odmowy. Nie sprawdza geometrii, nie tworzy ARD
i nie potwierdza Core:Solids lub wydania 2025.3.

Wykonanie na Windows pozostaje niepotwierdzone do otrzymania nowego raportu.
Nie ustalono jeszcze minimalnego zestawu plików z pełnej instalacji SolidLink.

## Wynik testu 7 i poprawka 7.1

Raport cls_solidlink_test_7_20261017_185404_094213.zip zatrzymał się
na kontroli ProgID/CLSID. Proces PowerShell nie został uruchomiony,
nie otwarto nowej sesji CLS i nie wywołano konwersji. Nie zaobserwowano
Acam.exe ani SolidLink.exe przed/po; moduły CLS i oryginalny STEP niezmienione.
Raport nie zapisał odczytanego CLSID i błędów rejestru; przyczyna różnicy
wobec wcześniejszych raportów rejestracji pozostaje nieustalona.

[Test 7.1](../../tools/solidimport/README_TEST_7_1_PL.md) zachowuje
wartości kluczy i metadane EXE również po błędzie. Aktywacja nadal dotyczy
wyłącznie znanego CLSID i pliku o wymaganym SHA-256. Brak samego aliasu
ProgID nie blokuje poprawnej rejestracji tej klasy; sprzeczny alias,
nieznany plik i błąd odczytu nadal blokują aktywację. Rejestr nie jest zmieniany.
Przeszło 10 testów przenośnych, w tym brak aliasu, różny zapis GUID,
sprzeczny CLSID, brak serwera, inna wersja pliku i błąd dostępu.
Wykonanie 7.1 na Windows pozostaje do potwierdzenia.

## Wynik 7.1 — brak dotychczasowej rejestracji i ścieżki SolidLink

Raport cls_solidlink_test_7_1_20261017_190108_930026.zip: wskazany ProgID
i LocalServer32 znanego CLSID nie występują w obu odczytanych widokach HKCR;
nie zapisano błędów dostępu. Poprzednio istniejący plik w Common Files /
Hexagon / SolidLink 2025.3 / code / bin / SolidLink.exe ma exists=false.
Nie szukano innych lokalizacji instalacji i nie można stwierdzić, że plik
nie istnieje nigdzie na komputerze. Nie uruchomiono workera, nowej sesji CLS
ani konwersji. Trzy moduły CLS i oryginalny STEP pozostały niezmienione.

Przed kolejną próbą potrzebna jest aktualna lokalizacja SolidLinka
i ustalenie zgodności jego wersji. Wynik nie przeczy sukcesowi sesji CLS 6.2
i nie jest odmową licencji. Nie zmieniano rejestru ani instalacji.

## Aktualna lokalizacja użytkownika: SolidLink 2025.4

Po raporcie 7.1 użytkownik podał ścieżkę Common Files / Hexagon /
SolidLink 2025.4 / code / bin / SolidLink.exe. Dotychczasowe kontrole
rejestracji i skrótu dotyczyły 2025.3; nie dowodziły braku nowszej instalacji.

[Test 7.2](../../tools/solidimport/README_TEST_7_2_PL.md) wybiera
SolidLink.Document.2025.4, wymaga zgodności ścieżki LocalServer32 z podanym
EXE, odczytuje jego wersję oraz cztery publiczne metody z biblioteki typów.
Metadane są odczytywane przez LoadTypeLibEx / REGKIND_NONE bez rejestracji
i przed aktywacją COM. Do używanych pól i metod COM zastosowano publiczne
typy System.Runtime.InteropServices.ComTypes, zgodnie z dokumentacją
Microsoft: https://learn.microsoft.com/en-us/windows/win32/api/oleauto/nf-oleauto-loadtypelibex
oraz https://learn.microsoft.com/en-us/dotnet/api/system.runtime.interopservices.comtypes.elemdesc

Nowy EXE ma skrót bazowy obliczany w instalacji użytkownika i kontrolowany
przed/w trakcie inicjalizacji oraz po próbie. Nie wymaga skrótu poprzedniej
wersji; nie korzysta z jej prywatnego ABI. Moduły CLS zachowują wcześniej
potwierdzone przypięcie skrótów. Sesja CLS i translacja pozostają osobnymi
wynikami. Nowe wykonanie na Windows i kontrakt 2025.4 są niepotwierdzone
do otrzymania raportu.

## Wynik 7.2 — konwersja wywołana, bez pliku wynikowego

Raport cls_solidlink_test_7_2_20261017_192725_586052.zip zweryfikowano
niezależnie. CLS: Open=Ok (1), Valid=Ok (101) przed i po próbie, produkt 2,
powrót z CloseLicenseConnection bez błędu. SolidLink 2025.4.2616.108,
ProgID SolidLink.Document.2025.4, CLSID 0F3A6583-13A8-4304-8544-88602B3A56F5,
EXE 1 254 040 bajtów, SHA-256
71d48d786a76aa16031be28c9ebbe4c9b95555643fd79f475b5a6e76587ce83c.
Kontrakt czterech publicznych metod z metadanych został potwierdzony.

COM utworzony i zwolniony, GetFileType=11, wywołano TranslateSolid,
wynik 1, GetInteropResult=0, output_exists=false. Worker exit=0 oznacza
zakończoną diagnostykę, nie sukces translacji. Nie zaobserwowano Acam.exe
w 16 próbkach ani przed/po; po próbie nie było też SolidLink.exe.
Moduły, EXE oraz oryginalny STEP i kopia robocza pozostały niezmienione.

Sama otwarta sesja CLS nie wystarczyła w tym wariancie wywołania.
Nie ustalono przyczyny kodu 1; nie należy przypisywać go wyłącznie licencji,
formatowi lub brakowi opcji bez dalszych danych. Potrzebna jest analiza
aktualnego EXE 2025.4; lokalny starszy SolidLink.exe 2025.3 nie jest
bajtowo zgodny z tą wersją. Nie powtarzamy identycznego wywołania jako nowego
testu i nie wymuszamy potwierdzeń licencyjnych.

## Analiza dostarczonego SolidLink.exe 2025.4 — źródło kodu 1

EXE ma dokładnie skrót i rozmiar z testu 7.2. Mapa MFC wiąże
TranslateSolid z RVA 0x29850, który wywołuje właściwą translację RVA 0x32c40.
Na początku funkcja sprawdza wewnętrzny stan autoryzacji SolidLink.
Konstruktor inicjalizuje go zerem; przy zerze następuje powrót z 1 przed
ustawieniem plików i wyborem właściwego translatora. Stan jest zapisywany
wynikiem walidacji wywoływanej przez publiczną SendLicenseString, osobno
od sesji CLS. ConfirmParasolidLicensed zapisuje inny stan i nie zastępuje
tego etapu. Nazwy metod, wskaźniki mapy i instrukcje sprawdzono automatycznie.

Sekwencja testu 7.2 nie zawiera SendLicenseString, co wyjaśnia wczesny
kod 1. To wniosek z kodu i raportowanej sekwencji, nie nowy ślad debuggera.
Nie wydobywano ani nie odtwarzano materiału autoryzacyjnego, nie wywoływano
tej metody z odgadniętą wartością i nie wymuszano stanu licencji.
Następny potrzebny element to aktualny SolidImport.dll z instalacji
użytkownika do analizy oryginalnej inicjalizacji. Poprawne CLS nie jest
samo w sobie potwierdzeniem tej inicjalizacji ani źródłem ciągu SolidLink.
Szczegóły: tools/solidimport/solidlink_2025_4_initialization_reference.json.

## Aktualny SolidImport.dll 2025.4.1.100

Dostarczony SolidImport(1).dll ma 156 824 bajty i SHA-256
`b143da4e3f8771e04aebb7d20334244900866148dc83d242a3e0ad6938c5f82c`.
To natywny x64/MFC140. Eksporty InitAlphacamAddIn i StepInputFile
znajdują się odpowiednio pod RVA 0x78c0 i 0xcf90. Nie wolno stosować do
niego przypiętego skrótu ani prywatnych offsetów testu 3.2 dla 2025.3.

Init nadal odczytuje rzeczywisty ProgramLevel hosta, a przy poziomie 14
pobiera jego License i sprawdza Core:Solids. Negatywny wynik prowadzi do
błędu inicjalizacji. IID aplikacji i ILicense oraz offsety tych metod
odpowiadają wcześniejszemu kontraktowi; nie tworzy to samodzielnego dostawcy
licencji przy zamkniętym procesie aplikacji.

Ścieżka StepInputFile prowadzi przez procedurę 0x92b0 do wrappera translacji
0x10ef0. Ten tworzy COM i wywołuje oryginalny helper autoryzacji 0x12b20;
w nim są wywołania SendLicenseString (DISPID 16). Wynik musi być dodatni
przed dalszą translacją. Następnie wrapper przekazuje stan
ConfirmParasolidLicensed (DISPID 21), wywołuje TranslateSolid (DISPID 1)
i GetInteropResult (DISPID 10). Są to odrębne etapy od otwarcia sesji CLS.

Nie wyodrębniano ani nie odtwarzano ciągów autoryzacyjnych. Analiza
potwierdza brakujący etap w teście 7.2, ale nie dostarcza jeszcze poprawnego
samodzielnego hosta SolidImport ani udanej konwersji przy zamkniętym Acam.exe.
Nie utworzono nowego testu wykonującego ten sam niepełny przebieg.
Sanityzowany zapis: tools/solidimport/solidimport_2025_4_initialization_reference.json.

## Acam.exe 2025.4 i przygotowany test 8

Pełny nowy Acam.exe ma wersję 2025.4.1.104, 25 812 120 bajtów i SHA-256
`02997d0f6c15d0e6c807d1be19621e128bdbf3a55ae1846e0196872766bf205d`.
TYPELIB/694/2057 zachowuje IID ILicense i CLSID License. Getter 0x33cbc0
wywołuje wewnętrzną fabrykę 0x36a420. Nie ma eksportu DllGetClassObject.
Sam obiekt nadal nie inicjalizuje stanu funkcji całej aplikacji.

IsFeatureLicensed (0x369e80) mapuje Core:Solids na bit 3 stanu produktu.
Procedura 0x3340c0 wyznacza ten stan; jedna ze ścieżek odpytuje kod 390
przez metodę dostawcy w slocie 0x10. Dla oryginalnego dostawcy CLS to metoda
0x63f6a0 i oryginalne GetAccessCode 0x7b53f0. Inne ścieżki wykorzystują
zapisany stan modułu lub dodatkowe warunki produktu. Nie odtwarzano ich
przez ustawianie flag ani tworzenie sztucznego ILicense.

[Test 8](../../tools/solidimport/README_TEST_8_PL.md) wykonuje jedno zwykłe
publiczne GetAccessCode(390) z LicManaged.dll przy zweryfikowanej sesji CLS.
Sprawdza ważność sesji po żądaniu, zwalnia tylko własne uzyskane uprawnienie
i zamyka własne połączenie. Nie wykonuje SolidImport, SolidLink ani STEP.
Wynik solids_access_request_confirmed wymaga przyznania, poprawnego
zwolnienia, zamknięcia CLS, niezmienności czterech plików i braku Acam.exe
w monitorowaniu. Core:Solids i wydanie nadal pozostają niepotwierdzone.
Odmowa kodu 390 nie oznacza braku licencji na bryły w normalnym ALPHACAM-ie.

Dowody: tools/solidimport/acam_2025_4_license_reference.json.
Nowe wykonanie na Windows pozostaje do potwierdzenia przez użytkownika.

## Wynik testu 8 — przyznanie 390 potwierdzone, zwolnienie NotOptional

Raport cls_solids_test_8_20261017_195301_317870.zip sprawdzono niezależnie;
worker_result.json odpowiada dokładnie kopii w raporcie nadrzędnym.
OpenLicenseConnection=Ok (1), walidacja przed i po GetAccessCode=Ok (101).
GetAccessCode(390) zwróciło true. Nie zaobserwowano Acam.exe w 14 próbkach
ani przed/po; cztery kontrolowane pliki pozostały niezmienione.

ReleaseAccessCode(390) zwróciło NotOptional (202). Test 8 uznawał za poprawne
indywidualne zwolnienie tylko Ok (201) i CapacityReleasedOk (207), stąd
phase=failed, worker_exit_code=1 i oba zbiorcze potwierdzenia=false.
Nie oznacza to odmowy przyznania 390 ani odmowy otwarcia CLS.
CloseLicenseConnection wróciło bez wyjątku. Nie ma potwierdzenia
indywidualnego zwolnienia 390 ani zwolnienia stanowiska po stronie serwera.

Nie zmieniano oryginalnych raportów ani nie zamieniano NotOptional na Ok.
Raportowanie rozdziela teraz obserwację przyznania, wynik indywidualnego
zwolnienia, powrót z zamknięcia połączenia i ścisły wynik całej próby.
Nie ma potrzeby powtarzać identycznego zapytania tylko dla zmiany komunikatu.

Potwierdzono zwykłe przyznanie konkretnego uprawnienia przez publiczne API
bez zaobserwowanego Acam.exe. To nadal nie jest pełny ILicense hosta,
autoryzacja SolidLink, potwierdzenie wydania ani konwersja STEP/ARD.

## Przygotowany test 9 — adapter COM oparty na rzeczywistym CLS

cls_license_adapter.cs definiuje lokalny obiekt CCW z IID ILicense i dwunastoma
metodami w kolejności oraz z sygnaturami bieżącej TYPELIB. IsFeatureLicensed
obsługuje wyłącznie dokładne Core:Solids: waliduje bieżącą sesję, wykonuje jedno
publiczne GetAccessCode(390), ponownie waliduje sesję i zwraca jej rzeczywistą
odpowiedź jako VARIANT_BOOL. Odmowa nie jest zamieniana w przyznanie. Pozostałe
metody są nieobsługiwane; nie ma fikcyjnych uchwytów modułu lub stanów licencji.

[Test 9](../../tools/solidimport/README_TEST_9_PL.md) uzyskuje natywny interfejs,
wykonuje QueryInterface, odczytuje metodę w slocie 0x48 i wywołuje ją z BSTR
oraz kontrolowanym buforem wyniku. Strażniki kontrolują szerokość dwubajtową.
Następnie sprawdza odmowę nieznanej funkcji i pustej nazwy bez kolejnego
zapytania CLS. Zwalnia własne referencje COM przed zamknięciem sesji.

NotOptional=202 jest jawnie odrębne od poprawnego indywidualnego zwolnienia;
przy tym wyniku test 9 może potwierdzić adapter po poprawnym zamknięciu CLS,
ale release_completed pozostaje false. Test 8 zachowuje ścisły wynik całości.
Warunki potwierdzenia sesji i adaptera sprawdzają się oddzielnie. Nie ma
twierdzenia o zwolnieniu stanowiska po stronie serwera.

Wykonanie C#/COM na Windows pozostaje niepotwierdzone. Przeszło 30 testów
przenośnych raportowania oraz parsowanie składni C#, PowerShell i Python.
Sprawdzono IID, kolejność, DISPIDs i sloty wszystkich 12 metod względem
metadanych dostarczonego Acam.exe; nie jest to zastępstwo kompilacji Windows.

Test nie wywołuje jeszcze InitAlphacamAddIn ani StepInputFile. Nie ustalono
pełnego źródła bieżących ProgramLevel, SpecialKey i tekstów hosta. W szczególności
SpecialKey=1 z wcześniejszego odczytu 2025.3 nie jest potwierdzeniem obecnego
stanu 2025.4. Nie należy go wpisywać do nowego hosta jako aktualnego uprawnienia.
Nie przenoszono prywatnych offsetów 2025.3 ani materiału autoryzacji SolidLink.

## Wynik testu 9 — natywny adapter działa na Windows

Raport cls_adapter_test_9_20261017_201017_881329.zip sprawdzono niezależnie
przez confirmed_session i confirmed_license_adapter; oba wyniki true.
Kopia worker_result w raporcie nadrzędnym odpowiada osobnemu plikowi.
QueryInterface=0, tożsamość interfejsu zgodna, wywołanie ze slotu 0x48
zwróciło HRESULT=0 i VARIANT_BOOL=-1. Kontrola otaczającej pamięci potwierdza
dwubajtowy zapis wyniku. CLS rzeczywiście przyznało 390; wykonano jedno
zapytanie, walidacja przed/po=101. Nieznana funkcja zwróciła E_NOTIMPL,
pusta nazwa E_INVALIDARG, obie z false i bez nowych zapytań CLS.

Zwolniono własne referencje COM. Indywidualne ReleaseAccessCode zwróciło
NotOptional=202; release_completed pozostaje false. CloseLicenseConnection
wróciło bez wyjątku, faza completed, worker exit=0. Nie zaobserwowano Acam.exe
w 14 próbkach ani przed/po; trzy moduły CLS i Acam.exe pozostały niezmienione.
Stdout/stderr procesu są puste. Ścisłe kryterium indywidualnego zwolnienia
z testu 8 nadal nie jest spełnione; nie jest wymagane do osobnego dowodu ABI
adaptera po normalnym zamknięciu jego sesji.

Potwierdzono wąski most rzeczywiste CLS → nasz natywny interfejs ILicense.
Nie wywołano SolidImport ani SolidLink; nadal nie ma inicjalizacji pełnego
hosta, autoryzacji SolidLink, kontroli wydania lub konwersji STEP/ARD.

## Przygotowany test 10 — oryginalny importer jako odbiorca adaptera

[Test 10](../../tools/solidimport/README_TEST_10_PL.md) ładuje dokładnie
SolidImport.dll 2025.4.1.100 i wywołuje InitAlphacamAddIn na natywnym wątku
utworzonym przez publiczne AfxBeginThread. Sesja CLS, adapter i wywołania
importera działają na tym samym wątku STA. Nie wykonuje bezpośredniego
samotestu adaptera: zapytanie Core:Solids ma pochodzić z oryginalnego importera.

Jawnie wybrany profil hosta acamLevelCORE=14 pochodzi z bieżącej TYPELIB.
Wybór prowadzi przez oryginalną kontrolę licencji; nie jest twierdzeniem
o bieżącej klasyfikacji produktu. SpecialKey pozostaje nieobsługiwane.
Próba zatrzyma się po zapisaniu żądania, walidacji i zamknięciu własnej sesji
CLS, jeśli importer poprosi o tę lub inną nieobsługiwaną usługę hosta.
Nie przenosi historycznej wartości SpecialKey z wersji 2025.3.

Raport osobno ocenia most licencji oraz pełny powrót z inicjalizatora.
Kontrolowany stop 80 na późniejszej usłudze hosta może potwierdzić przyjęcie
odpowiedzi licencyjnej, ale nie zakończenie InitAlphacamAddIn. Prefiksy COM
są prywatnym hostem diagnostycznym, nie kompletną implementacją Automation.
Normalne wyładowanie DLL pozostaje niepotwierdzone; obiekty są przypięte
do zakończenia jednorazowego procesu. Watchdog kończy wyłącznie własny proces.

Przeszło 35 testów przenośnych raportowania i parsowanie nowych źródeł.
Uruchomienie testu 10 i kompilacja nowych źródeł na Windows nie są jeszcze
potwierdzone. Nie wywołuje StepInputFile, nie tworzy Parasolidu ani ARD.

## Wynik testu 10 — oryginalny importer przyjął odpowiedź adaptera

Archiwum solidimport_bridge_test_10_20261017_203900_735920.zip ma SHA256
802f151072f7be377f1a7dfe033f3ff37023ae417f99a4814a986c1009327532.
Potwierdzono integralność ZIP, zgodność raportu worker z kopią nadrzędną
i niezależne confirmed_importer_bridge=true. Nowe źródła C#/COM uruchomiły się
na Windows. InitAlphacamAddIn wywołano na rzeczywistym wątku MFC/STA.

Oryginalny importer odczytał kolejno ProgramLevel, License, Frame,
wywołał RegisterImportFunction i poprosił o SpecialKey. Wszystkie te wywołania
odbyły się na tym samym wątku. Publiczny CLS rzeczywiście przyznał 390,
dokładnie raz; walidacja przed i po zapytaniu oraz po przerwaniu inicjalizatora
wyniosła 101. Przejście do usług Frame potwierdza wyjście z oryginalnej kontroli
Core:Solids. Profil CORE pozostaje wyborem kontraktu diagnostycznego hosta.

Zatrzymanie na nieobsługiwanym SpecialKey było kontrolowane, exit=80.
Initializer nie wrócił i initializer_succeeded=false. ReleaseAccessCode
zwróciło NotOptional=202, więc indywidualne zwolnienie nie jest potwierdzone.
Zamknięcie własnego połączenia CLS wróciło bez wyjątku. Nie zaobserwowano
Acam.exe w 22 próbkach ani przed/po; pięć plików instalacji pozostało bez zmian.
Stdout/stderr są puste. Nie wykonano konwersji STEP, nie powstał Parasolid ani
ARD. Następną brakującą usługą hosta jest bieżące SpecialKey; pełny host,
kontrola wydania i normalne wyładowanie natywnej biblioteki nadal niepotwierdzone.

## Przygotowany test 11 — projekcja maski SpecialKey używanej przez Init

W bieżącym Acam.exe getter SpecialKey (RVA 0x33d330) składa kilka flag.
Dla dostawcy CLS w tablicy 0x9a5a78 slot 0x98 prowadzi do 0x20e770:
GetAccessCode(348) daje flagę 16. Slot 0x50 prowadzi do 0x20e590:
GetAccessCode(347) OR GetAccessCode(349), ze skróconym obliczaniem OR,
daje flagę 2. Oba wywołują slot 0x10 tego samego dostawcy (0x63f6a0),
dalej natywne CLS GetAccessCode (0x7b53f0). Nie szukano kodów próbami.

SolidImport.dll z ustalonym SHA256 odczytuje SpecialKey w Init jeden raz,
pod 0x7b63. Wartość jest używana wyłącznie przez test maski 0x12 pod 0x7b83
(bajty f6453012). Test 11 implementuje tę konkretną projekcję: zapytanie 348,
potem 347 i tylko przy false — 349. Zachowuje rzeczywiste odpowiedzi false/true
i waliduje CLS przed/po każdej odpowiedzi. Nie zmienia bitów kodem klienta.

To nie ustalenie pełnego SpecialKey: bity poza 0x12 pozostają niepotwierdzone.
Wartość projekcji ma sens tylko dla tego sprawdzonego konsumenta Init.
Nie wolno przedstawiać jej jako pełnej klasyfikacji aktualnego ALPHACAM
ani używać do dowolnych innych klientów API, eksportów lub StepInputFile.
Kod kontroluje skrót całej biblioteki i dodatkowo bajty instrukcji maski.

Na tym samym wątku MFC otwiera własną sesję CLS, ponawia oryginalne Init
i wykonuje zapytania klasyfikacji dopiero w odpowiedzi na odczyt SpecialKey.
Wszystkie przyznane zapytania są oddzielnie sprzątane, następnie zamykana
jest sesja; 202 pozostaje odrębne od indywidualnego zwolnienia 201/207.
Nieobsługiwane usługi kończą próbę. ReadTextFile ma teraz dokładną sygnaturę
z bieżącej TYPELIB (BSTR, I4, I2, BSTR*); strażnik zapisuje tylko identyfikatory
żądanego tekstu, nie odczytuje zawartości i nie zwraca fikcyjnego wyniku.

[Test 11](../../tools/solidimport/README_TEST_11_PL.md) rozdziela dowód przejścia
przez dwie flagi od pełnego powrotu z Init. Przeszło 40 testów przenośnych
raportowania; sprawdzono składnię C#/PowerShell i sygnaturę ReadTextFile,
tablicę dostawcy, stałe kodów oraz instrukcję konsumenta względem bieżących
plików. Kompilacja/wykonanie nowej próby na Windows nadal niepotwierdzone.

## Wynik testu 11 — importer przeszedł przez dwie flagi SpecialKey

Raport solidimport_special_key_test_11_20261017_204645_481267.zip ma SHA256
9a40f346db2067a88b615656d2abe5ff40654f6ddef408e6077be8580352cc50.
Integralność ZIP i zgodność worker z raportem nadrzędnym potwierdzono.
Niezależne confirmed_projection i confirmed_special_key_bridge zwracają true.
Nowe źródła wykonały się na Windows. Zapytania 348, 347 i 349 zwróciły false;
każde miało walidację CLS 101 przed i po. Projekcja maski 0x12 wyniosła 0.
To brak dwóch badanych flag klasyfikacji, nie odmowa wcześniej przyznanego
Core:Solids/390 ani ustalenie pełnego SpecialKey.

Oryginalny Init przeszedł do dalszych rejestracji. Log obejmuje 16 lokalnych
RegisterImportFunction i 2 RegisterExportFunction. Następnie poprosił o
ReadTextFile("SolidImport.po", 10, 1). Dokładnie to jest pierwszą zaobserwowaną
granicą katalogu tekstów w tej próbie; znany statycznie późniejszy odczyt
aedit.ctx nie został osiągnięty. Wszystkie wywołania hosta oraz zapytania CLS
odbyły się na jednym wątku MFC/STA.

Kontrolowany stop 80 na ReadTextFile nie oznacza powrotu z Init:
init_returned=false i initializer_succeeded=false. Zapytanie 390 pozostało
rzeczywiście przyznane, dokładnie raz. ReleaseAccessCode(390)=NotOptional/202,
indywidualne zwolnienie niepotwierdzone; własne połączenie CLS zamknięto
bez wyjątku. Żadne zapytanie klasyfikacji nie wymagało zwolnienia, bo wszystkie
zwróciły false. Nie zaobserwowano Acam.exe w 23 próbkach ani przed/po.
Pięć plików instalacji nie zmieniło się; stdout/stderr puste. Nie wykonano
StepInputFile ani konwersji Parasolid/ARD. Kolejny brak to obsługa katalogu
tekstów hosta, zaczynając od rzeczywistego żądania SolidImport.po, sekcja 10,
indeks 1. Pełny SpecialKey i pełna implementacja hosta nadal niepotwierdzone.

## Przygotowany test 12 — katalog komunikatów z bieżącej instalacji

Wcześniej dostarczony SolidImport.po (2488 B, SHA256
8c33b9e565aa34869c2b7d33357df8ea8e65b749127b00c368891f908cc2e41c)
jest tekstowym PO UTF-8/BOM: Language=pl-PL, kontekst solidimport|,
25 numerycznych tłumaczeń. Klucz 10-1 odpowiada komunikatowi błędu tłumaczenia.
Ten plik posłużył do sprawdzenia parsera, nie jako założenie o bieżącym
pliku instalacji. Nie jest dołączany do kodu ani ZIP testu.

[Test 12](../../tools/solidimport/README_TEST_12_PL.md) wyszukuje rzeczywiste
SolidImport.po i opcjonalne aedit.po w wybranym katalogu instalacji/języka.
Preferuje jawnie wybrane pl-PL, odrzuca niejednoznaczne pliki, duplikaty
klucza, wpisy fuzzy i nieobsługiwane formy mnogie. Nie zamienia brakujących
tłumaczeń w przykładowy tekst, pusty ciąg lub identyfikator. Własna tymczasowa
mapa trafia do C#; natywne ReadTextFile zwraca dokładny BSTR po sprawdzeniu
skrótu źródłowego pliku. Raport rejestruje klucz, hash tekstu, hash źródła
i wątek, bez surowej zawartości katalogów.

Żądanie aedit.ctx może być jawnie zmapowane przez ten prywatny host na
ten sam numeryczny wpis rzeczywistego aedit.po. To ograniczona mapa zasobów,
nie potwierdzona implementacja natywnego czytnika CTX ALPHACAM. Przy braku
odpowiedniego pliku/wpisu lub innej usłudze test zatrzymuje się jak wcześniej.

Raport osobno potwierdza przyjęcie tekstów przez oryginalny importer
i rzeczywisty powrót Init=0. Nie wywołuje StepInputFile. Licencja i dwie flagi
SpecialKey zachowują kontrakt testu 11; inne bity nadal nieustalone.
Przeszło 45 testów przenośnych parsera i kryteriów raportowania; składnia
C#/PowerShell sprawdzona. Wykonanie nowych źródeł na Windows niepotwierdzone.

## Wynik testu 12 — oryginalne InitAlphacamAddIn wróciło z kodem 0

Raport solidimport_text_catalog_test_12_20261017_210006_470330.zip ma SHA256
47638442ae61f611435faeb61dffd5edb87435115e5cd2d4607a5ca581e4d7cc.
Sprawdzono integralność ZIP, zgodność obu osobnych raportów z kopiami
nadrzędnymi i niezależne confirmed_catalog_bridge=true. Kryterium mostu
licencji/flag dla schematu 12 również zwraca true. Nowe źródła wykonały się
na Windows. Oryginalne InitAlphacamAddIn rzeczywiście wróciło: kod 0,
init_returned=true, init_succeeded=true, faza completed, proces exit=0.
Nie było nieobsługiwanych wywołań ani kontrolowanego stopu.

Log obejmuje 16 prywatnych rejestracji importu, 2 eksportu i 25 ReadTextFile
na jednym wątku MFC/STA. Zwrócono 23 teksty SolidImport.po i 2 teksty Aedit.po
(65-1 oraz 65-2), te ostatnie przez jawny alias aedit.ctx. Wszystkie hashe
wartości odpowiadają wybranym wpisom katalogów. Bieżący SolidImport.po ma
2486 B, SHA256
aba33397f309c95eab9b0bc9c45d61eeabae7211a4c9e8668196d1ed690c768c.
Aedit.po ma 39568 B, SHA256
b7d59eca9aa88d63aef5aa30e4331d351148a7586f81ce7b9c83504892a6b3f8.
Oba pliki pl-PL pozostały niezmienione. Nie utożsamiamy bieżącego pliku PO
z wcześniejszą dostarczoną kopią o innym skrócie.

CLS przyznał 390 dokładnie raz, walidacje 101. Zapytania klasyfikacji
348/347/349 zwróciły false; projekcja SpecialKey 0x12 wyniosła 0, pełna wartość
nadal niepotwierdzona. ReleaseAccessCode(390)=NotOptional/202, więc indywidualne
zwolnienie niepotwierdzone. Własne połączenie CLS zamknięto bez wyjątku.
Nie zaobserwowano Acam.exe w 25 próbkach ani przed/po. Pięć plików instalacji
nie zmieniło się; stdout/stderr puste.

Potwierdzono zakończenie oryginalnego inicjalizatora z tym ograniczonym hostem,
rzeczywistym CLS i rzeczywistymi tekstami, przy zamkniętym procesie ALPHACAM.
Nie jest to dowód pełnego hosta Automation, kontroli wydania, natywnego
dekodowania CTX ani normalnego unload DLL. StepInputFile nie wywołano;
konwersja STEP → Parasolid → ARD przy zamkniętym Acam.exe nadal niepotwierdzona.

## Przygotowany test 13 — oryginalne StepInputFile po sprawdzonym Init

[Test 13](../../tools/solidimport/README_TEST_13_PL.md) zachowuje sprawdzony
przebieg CLS i inicjalizacji. Po faktycznym Init=0 na tym samym wątku MFC/STA
wywołuje oryginalny eksport StepInputFile pod RVA 0xcf90. Argument filename
jest const char*, sprawdzony przez bieżące importy CString: konstruktor
ordinal 290/IAT 0x183c0, GetLength ordinal 5674/IAT 0x18378 i destruktor
ordinal 1033/IAT 0x18660. Oryginał STEP pozostaje wejściem tylko do odczytu;
eksport dostaje nową kopię z zachowaniem skrótu.

Bieżący eksport wywołuje wspólną procedurę 0x92b0 pod 0xd078. Jej ścieżka
po translacji przechodzi pod 0x9c2e do helpera 0x8f00, którego pierwszy
ActiveDrawing to wywołanie slotu 0x48 pod 0x8f41. Host zatrzymuje proces
na tej granicy bez oddawania wskaźnika rysunku, również przy błędzie przechwycenia.
Akceptuje tylko jeden rozpoznany Parasolid w własnym TEMP; zapisuje jego kopię
i sprawdza hash źródła/kopii, nagłówek i obecność danych. Przed STEP w TEMP
nie może być Parasolidu. Eksport nie zachowuje wiarygodnego kodu translatora
w epilogu, dlatego sam powrót bez pliku nie jest sukcesem.

Nowa próba sprawdza rejestrację 2025.4, skrót serwera i ścieżkę/hash faktycznie
zaobserwowanego SolidLink.exe. Nie zaczyna z już działającym serwerem. Nie zmienia
rejestracji lub wersji konfiguracji, nie zabija procesów producenta; watchdog
kończy tylko własny proces testowy. CLS pozostaje otwarty przez Init i STEP,
a zwalnianie/Close następuje po przechwyceniu. 202 pozostaje odrębne od 201/207.

Uzupełniono terminalne strażniki dla wszystkich 128 slotów IAlphaCamApp
i 78 slotów IFrame bieżącej TYPELIB, także na ścieżkach błędów. InAPICall
odpowiada wywołaniu przez ten host; trzy metody postępu aktualizują wyłącznie
prywatny stan diagnostyczny. Nie jest to pełny host Automation ani UI ALPHACAM.
SpecjalKey pozostaje ograniczony do maski sprawdzonej w Init; przy nowym
nieobsługiwanym żądaniu test nie dostarcza wymyślonych wartości.

Przeszło 50 testów przenośnych raportowania, parsera i odrzucania fałszywego
sukcesu. Sprawdzono składnię nowych C#/PowerShell i pełne pokrycie slotów
oraz importy i instrukcje względem obecnych plików producenta. Windows/C#
wykonanie nowej próby pozostaje niepotwierdzone. Kontrola nagłówka i kopii
nie jest kontrolą topologii ani geometrii: geometry_verified=false.
ARD nie jest tworzony; normalny unload DLL nadal niepotwierdzony.

## Wynik testu 13 — świeży Parasolid z STEP przy zamkniętym Acam.exe

Archiwum solidimport_step_capture_test_13_20261017_211059_074049.zip ma SHA256
2f912d1468e3a7913b8dbb6a1cd30ffe0839d76d23594e54b4fe7dbedbb9d2f9.
Integralność ZIP, zgodność worker/katalogu z kopiami nadrzędnymi i niezależne
confirmed_capture potwierdzono. Sprawdzono też rzeczywiste bajty captured.x_t:
80273 B, SHA256
bec7767b6fdd940965e79153975ea4db993510dd9abf3b74dabbf19801a0dca2,
rozpoznany tekstowy nagłówek Parasolid i dane po nagłówku. Wynik inspect_output
odpowiada raportowi, nie tylko deklaracji procesu. Geometria nadal niezweryfikowana.

Oryginalne InitAlphacamAddIn wróciło z 0; oryginalne StepInputFile wywołano
na tym samym wątku MFC/STA. Oryginalny CString poprawnie odtworzył pełną ścieżkę
ANSI/ACP1250. SHA256 STEP-a pozostał
88541daf8ce0f68780ab615550c87a0417933083577ca2d63d6f40a1722735d7.
Przed STEP nie było Parasolidu w roboczym TEMP. Po translacji importer odczytał
InAPICall i zażądał ActiveDrawing. Znaleziono dokładnie jeden rozpoznany plik,
przechwycono go bez oddawania wskaźnika rysunku, exit=83, faza parasolid_captured.
StepInputFile nie wróciło, bo terminalna granica została osiągnięta wcześniej;
nie odczytujemy ani nie wymyślamy kodu wyniku eksportu. Brak nieobsługiwanych
żądań hosta; stdout/stderr puste. Nowe źródła C#/COM wykonały się na Windows.

Zaobserwowano nowy SolidLink.exe z dokładnie oczekiwanej ścieżki 2025.4 i SHA256
71d48d786a76aa16031be28c9ebbe4c9b95555643fd79f475b5a6e76587ce83c.
Nie było serwera przed próbą. Nie zaobserwowano Acam.exe w 64 próbkach
ani przed/po. Pięć plików instalacji, oba katalogi PO, SolidLink.exe,
oryginalny STEP i jego kopia pozostały niezmienione.

CLS przyznał 390 raz, walidacja przed i po STEP=101. Zwolnienie dało
NotOptional/202, więc indywidualne zwolnienie nie jest potwierdzone;
własne połączenie CLS zamknięto bez wyjątku. Pozostałe badane flagi dały false.
Nie twierdzimy, że powstał pełny ILicense/host, pełne SpecialKey, potwierdzenie
wydania lub normalny unload DLL. Nie odczytywano i nie odtwarzano materiału
autoryzacji; wykonano zwykłe oryginalne eksporty z rzeczywistymi odpowiedziami CLS.

Potwierdzono świeży STEP → plik Parasolid przy zamkniętym procesie ALPHACAM,
z użyciem zainstalowanych komponentów producenta. Nie jest to niezależność
od tych komponentów. Następny etap to sprawdzenie topologii i geometrii tego
konkretnego pliku oraz zapis ARD; pełny bieżący STEP → ARD nadal niepotwierdzony.

## Przygotowany test 14

[Opis testu 14](feature_test_14.md) dokumentuje osobny rzeczywisty odczyt
CLS 348 dla maski 0x10 sprawdzanej przez oryginalny Feature Bodies.Save.
Nie jest to pełne SpecialKey ani odtworzenie starej wartości 1. Sesja CLS
pozostaje otwarta podczas pracy Feature, a odpowiedź true/false jest zachowana.
Pakiet sprawdzono statycznie i 57 testami przenośnymi; wykonanie Windows,
pomiar nowej bryły i zapis nowego ARD nie są jeszcze potwierdzone.
