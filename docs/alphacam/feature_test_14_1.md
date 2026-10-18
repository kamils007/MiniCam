# Test 14.1 — nowa Feature.dll 2025.4.1.200

Status: wykonany preflight; sesja CLS i natywne Feature nie zostały uruchomione.

Wynik `step_ard_test_14_1_20261017_213703_872574.zip`, SHA256
de0cf89635b6a6078ed6469da1b70519da1aff6df7cb4c658331d6b27b247f12,
potwierdził zgodność wszystkich ośmiu plików oraz ich stabilność przed/po.
Nie zaobserwowano Acam.exe w 11 próbkach. Helper zakończył się kodem 1 przed
zapisaniem cls_ready.json: MethodInfo.Invoke odrzuciło PSObject zamiast String
w argumencie SetClsPath pochodzącym z Join-Path. request_count=0, ready=false;
połączenie CLS nie zostało jeszcze otwarte, więc close_completed=false nie
oznacza nieudanego zamknięcia otwartej sesji. Brak logów procesu Feature
i brak ARD. Poprawkę przekazywania argumentów przygotowano w teście 14.2.
Nowy przesłany plik odpowiada dokładnie hashowi wykrytemu w raporcie testu 14:
5bbfbdbd41c32dde36dfc81db3a7a022b9e3688622f7005130cc207bc6638438,
1 854 104 bajty, wersja zasobu 2025.4.1.200.

Dodano osobny profil w `tools/alphacam_reference/feature_builds.py`.
Stare domyślne wywołania nadal wymagają poprzedniego buildu. Test 14.1 jawnie
wybiera nowy hash, eksporty i adresy danych we wszystkich adapterach.
Worker niezależnie porównuje hash i eksporty przed wywołaniem kodu natywnego.

## Zweryfikowany układ

| Element | RVA nowej DLL |
| --- | --- |
| GetSolidFeatures / GetUIVersion | 0x389f0 / 0x38a40 |
| InitAlphacamAddIn (niewywoływane) | 0x38b80 |
| ObjectNumberOf | 0x40d10 |
| ObjectDoWrite2 / ObjectQueryWrite2 | 0x40080 / 0x413a0 |
| fallback aplikacji / kontekst | 0x18a660 / 0x18a5b8 |
| backend / głębokość kontekstu | 0x184140 / 0x184360 |
| obiekt postępu / rodzic | 0x18a648 / 0x18a650 |
| wskaźnik tabeli tekstów | 0x18a588 |
| CString construct / get / destroy | 0x140068 / 0x140060 / 0x140070 |
| CString assign | 0x1408b8 |
| CRT _wfopen_s / fwrite | 0x13fe30 / 0x13fe40 |
| CRT fflush / fclose | 0x13fe28 / 0x13fe70 |

Importy CString sprawdzono przez rzeczywiste ordinały MFC 296/1641/1033/1503,
a CRT przez nazwy z tablicy importów. Adresy stanu leżą w zapisywalnej sekcji
danych. Ślad ObjectNumberOf prowadzi przez oryginalny helper 0x66450,
który zachowuje i zwalnia referencje do fallback aplikacji 0x18a660.

Biblioteka typów jest bajtowo identyczna z poprzednią:
SHA256 869e1572af2e7a0b5e2419dacc531dc65b2c239e8e609a1b90858c9b4196ab8e.
Pozostają te same publiczne właściwości wyrównania, Bodies.Item,
Min/Max XYZ, kolekcje topologii, ImportAs i Save.

Nowa tabela ma **585** CString zamiast 583. Rzeczywiste wektory z inicjalizatora
dają w grupie 40 liczbę **33** zamiast 31; reszta liczników pozostaje zgodna.
Adapter alokuje właściwy rozmiar i odmawia katalogu z innym układem grup.
Dotychczasowy dostarczony PO nie ma 40-32 i 40-33. Wraz z trzema wcześniejszymi
brakami daje to pięć pustych identyfikatorów Feature i siedem pustych tekstów
innych katalogów. Są to poprawne obiekty CString; braki są jawne w raporcie.
Wymagany tekst postępu 80-6 jest obecny. Nie twierdzimy, że odtworzono kompletne
zasoby językowe ani pełną inicjalizację dodatku.

W oryginalnych zapisach przy RVA 0x89390, 0x89550, 0x8df50, 0x8e120 nadal
odczytywany jest SpecialKey przez slot 0x1c8. Instrukcje maskujące 0x10 leżą
pod 0x89444, 0x89604, 0x8e004, 0x8e1d4; ustawiony bit daje E_NOTIMPL.
Zachowano bieżący GetAccessCode(348) z CLS, jego rzeczywistą odpowiedź,
ograniczenie udostępniania właściwości do fazy Save i normalne zamknięcie sesji.

Instrukcje wrapperów ObjectDoWrite2/ObjectQueryWrite2 oraz obszaru writerów
0xe870–0xee70 odpowiadają poprzedniej strukturze po pominięciu adresów
PC-relative. Nie oznacza to identyczności całych plików ani wszystkich funkcji.
Sprawdzono przebieg przekazywania VARIANT*, FILE* i callbacku fwrite,
inwersję bool wyniku zapisu oraz query. Literał obiektu LicomUKRJMParaSolid
występuje przy 0x1527f0; faktyczną nazwę, kody, bajty rekordu v3 i jego geometrię
nadal musi potwierdzić test Windows, zanim powstanie ARD.

## Zakres i sprawdzenie pakietu

Wejściem pozostaje konkretny Parasolid testu 13, hash bec7767b…a0dca2.
Nie powtarzamy STEP ani SolidLink. Wymagane są jedna bryła, rzeczywiste wymiary
1200 × 2300 × 10 mm, dodatnie liczniki, natywny zapis i czyste diagnostyki.
Wymiary i liczniki nie zastępują pełnego testu topologii, kształtu ani objętości.
Po sukcesie użytkownik ma nałożyć nowy ARD na oryginalny STEP.

Poprawiono rozróżnienie stabilności plików od zgodności ich wersji z profilem:
`inputs_unchanged` porównuje faktyczne odczyty przed i po, a osobne pole
`files_match_profile_after` porównuje je z oczekiwanymi hashami.

Przeszło 59 testów przenośnych, w tym rzeczywiste operacje na pamięci testowej
z nową komórką tabeli i pełnym cyklem pliku przez callbacki nowego układu IAT.
Statycznie porównano profil z przesłaną PE. Te sprawdzenia nie uruchamiają DLL
na Windows i nie dowodzą utworzenia nowego ARD.
