# Moduł „Zapisz do ALPHACAM (.ard)”

Opcjonalny moduł MiniCam, bez ładowania DLL Hexagona do procesu edytora.
W menu Plik znajdują się „Zapisz do ALPHACAM (.ard)…” i „Ustawienia eksportu
ALPHACAM…”. Konfiguracja jest potrzebna dopiero przy korzystaniu z eksportu.
Zapis 2D bez bryły nie uruchamia konwertera.

## Przygotowanie na Windows

1. Zbuduj MiniCam x64 ze standardowym Qt/OCCT. Opcja
   `MINICAM_WITH_ALPHACAM_EXPORT` jest domyślnie włączona.
2. Rozpakuj przygotowaną paczkę `MiniCam_ALPHACAM_runtime.zip` obok
   `minicam.exe`. Powstanie folder `alphacam-export`.
3. W ustawieniach eksportu wskaż Python 3 x64, folder tego modułu, katalog
   instalacji ALPHACAM 2025 i SolidLink.exe 2025.4. Wymagana jest działająca
   instalacja i zwykła bieżąca licencja używanych modułów.
4. Zamknij ALPHACAM. W MiniCam otwórz/ustaw model, wybierz zapis ARD i plik
   docelowy. Ustawienia są zapamiętywane.

Obsługiwany profil: Feature 2025.4.1.200, SolidImport 2025.4.1.100,
SolidLink 2025.4.2616.108 oraz odpowiadające im PSKERNEL/CLS. Konwerter
sprawdza konkretne SHA256. Inna wersja kończy się błędem, bez przełączenia
na starszy, niesprawdzony host. Podczas eksportu okno czeka na zakończenie;
nie przerywamy posiadaczy sesji CLS dodatkowym limitem procesu GUI.

## Kontrakt i izolacja

`createSolidRecord(model, parent)` przygotowuje nowy STEP z bieżącego modelu
OCCT, uwzględniając zapisane położenie i kolory. Nie sięga po pierwotny STEP
ani po wcześniejszy rekord. Przekazuje JSON z SHA256 do osobnego Pythona
(`-I -u export_record.py --request … --response …`). Backend wykonuje
sprawdzony test 15, z własnymi osobnymi procesami natywnymi i normalnym
zamknięciem swoich połączeń CLS. Zwraca dopiero zweryfikowany rekord jednej
bryły. GUI sprawdza SHA256 STEP-a i rekordu, długość, status i liczbę brył.

Obecny writer `core/ArdExport` składa końcowy dokument z tego rekordu,
aktualnych geometrii 2D, warstw i miniatury. `QSaveFile` podmienia docelowy
ARD dopiero po pełnym sukcesie. Bazowy `1_test.ard` wewnętrznego konwertera
służy jego sprawdzonemu przebiegowi; jego próbne geometrie 2D nie są kopiowane
do końcowego dokumentu MiniCam.

Każde wywołanie ma nowy katalog w lokalnych danych aplikacji,
`alphacam/jobs/<UUID>`. Błąd pokazuje tę ścieżkę. `conversion.zip` zawiera
ograniczony zestaw diagnostyczny; nie należy zastępować go całym katalogiem
roboczym z prywatnymi plikami etapów natywnych. Nie ma automatycznej wysyłki.

## Pakowanie i kompilacja

Paczka zawiera źródła naszego runtime, dostarczony prywatny wzorzec ARD i
katalog tekstów Feature.po. Nie zawiera Pythona, DLL/EXE producenta, wejściowego
STEP-a ani historycznej bryły Parasolid. Nie jest to instalacja Hexagona
przeniesiona do własnego folderu.

Z repozytorium, z własnymi wcześniej zweryfikowanymi plikami:

```powershell
python tools/step_ard/package_export_module.py --base C:/dane/1_test.ard --po C:/dane/Feature.po --out MiniCam_ALPHACAM_runtime.zip
```

Przygotowany folder można kopiować przy budowaniu, ustawiając
`-DMINICAM_ALPHACAM_RUNTIME=C:/dane/alphacam-export`.
`-DMINICAM_WITH_ALPHACAM_EXPORT=OFF` usuwa moduł i obie akcje menu;
pozostałe funkcje MiniCam nie potrzebują Pythona ani instalacji ALPHACAM-a.
Historyczny host C++ jest domyślnie wyłączony i nie jest używany w tej ścieżce.

## Zakres weryfikacji

Kontrole lokalne: 153 testy Pythona (62 adapterów, 65 SolidImport,
26 STEP/ARD), kompilacja Qt 6.4.2 / OCCT 7.6.3 na Linuksie z modułem
włączonym i wyłączonym, test CTest
przesuniętej bryły 12 × 23 × 4 mm przez STEP i ponowny import oraz test
zachowania rzeczywistego rekordu z Windows w dokumencie C++ z 2D i warstwą.
Nowy backend zaakceptował rzeczywisty raport testu 15 i jego bajty rekordu.
Sprawdzono ZIP runtime, manifest SHA256 i odizolowane uruchomienie CLI.

Natywny test 15 potwierdził zamknięty przebieg STEP → Parasolid → ARD dla
Bryła_4.stp na przypiętej instalacji 2025.4. To nie potwierdza jeszcze nowego
wywołania z GUI, dokumentu z aktualnymi warstwami ani wszystkich modeli.
Pierwszy test na stacji: otworzyć model, przesunąć/wyrównać, zapisać przez
nową akcję, a następnie otworzyć ARD w ALPHACAM i porównać bryłę oraz 2D.
Obsługa wielu brył, pełna walidacja topologii/objętości i przenośny minimalny
zestaw DLL pozostają poza potwierdzonym zakresem.
