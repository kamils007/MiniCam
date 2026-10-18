# Test 15 — jeden przebieg STEP → Parasolid → natywny rekord → ARD

Status: wspólny przebieg Windows potwierdzony na źródłowym Bryła_4.stp.

Wynik `step_ard_test_15_20261018_084240_638505.zip` ma SHA256
`94c0657cfdbf166e115620b06abbe48a52f3de3f160fa0c489e585deafcdc4f7`.
Sprawdzono integralność ZIP-a, ponownie oceniono confirmed_capture oraz zgodność
oddzielnych raportów workera i katalogów. Oryginalny STEP ma SHA256
`88541daf8ce0f68780ab615550c87a0417933083577ca2d63d6f40a1722735d7`.
Próba rzeczywiście powtórzyła import STEP-a i uruchomiła bieżący SolidLink.
Świeże przechwycenie ma 80 273 bajty i SHA256
`bec7767b6fdd940965e79153975ea4db993510dd9abf3b74dabbf19801a0dca2`.
Identyczny wynik jak w teście 13 jest wynikiem deterministycznej konwersji;
świeżość potwierdza aktualny przebieg, nie sama suma pliku.

Surowy stdout Feature odpowiada raportowi nadrzędnemu. SHA wejścia dziecka
przed i po odpowiada aktualnemu przechwyceniu, a ścieżka w rekordzie natywnym
wskazuje prywatną kopię tej próby. Feature i CLS zakończyły się kodem 0,
CLS zamknął połączenie, wszystkie cztery odczyty błędu kernela były czyste.
Konfiguracja Schemas została przywrócona; 101 plików katalogu oraz kontrolowane
pliki instalacji pozostały niezmienione. Nie wykryto Acam.exe w 62 próbkach
etapu przechwycenia ani 38 próbkach etapu nadrzędnego/Feature.

Jeden body: 1200 × 2300 × 10 mm, 63 ściany, 158 krawędzi, 100 wierzchołków.
Natywny rekord ma 55 019 bajtów, SHA256
`b6d28204618a3a34aefdac1d88afe504ddcab7af9f30be2386e751eb0aee9020`.
Rzeczywisty samodzielny Parasolid ma 54 677 bajtów, a geometria rekordu
54 849 bajtów. Oba strumienie binarne mają schemat SCH_3700000_36001.
Geometria wycięta z rekordu jest identyczna z plikiem w archiwum.
`STEP_TEST_15.ard` ma 117 961 bajtów i SHA256
`020aef8015a32ae20862cb6a44c3dd38a993072dfcbbfb127a75de902cea811e`.
Ponowne złożenie z rzeczywistego rekordu i znanej bazy daje bajtowo ten sam ARD.

Użytkownik zgłosił „JEst OK”. Nie zapisujemy tego jako osobnego potwierdzenia
otwarcia, nałożenia ani pomiaru geometrii wyniku testu 15. Jawne potwierdzenie
wizualnego pokrycia dotyczy testu 14.5. Zakres obecnego wyniku obejmuje jeden
model i przypięte wersje instalacji 2025.4, bez pełnej walidacji topologii,
objętości ani integracji z interfejsem MiniCam. Kontrolowane zakończenie
importera kodem 83 nadal nie jest normalnym powrotem StepInputFile.

Punkty odniesienia: test 13 wykonał świeżą zamkniętą konwersję STEP do Parasolidu;
test 14.5 zapisał dokładnie ten wynik jako rekord Feature oraz ARD. Użytkownik
potwierdził otwarcie i wizualne pokrycie ze STEP-em. Test 15 łączy oba etapy
w jednym nadrzędnym procesie Python, zachowując oddzielne procesy natywne.

`probe_solidimport_step_capture.main(argv)` udostępnia poprzedni przebieg bez
zmiany jego mechanizmu natywnego. Dane wejściowe są prywatną kopią wybranego
STEP-a; katalog wyniku musi być nowy. Przechwycenie nadal następuje w oryginalnym
callbacku ActiveDrawing, przed zwróceniem wskaźnika rysunku. Natywny proces
kończy się kontrolowanym kodem 83 po obsłudze własnej sesji CLS. Sam powrót
StepInputFile nie jest uznawany za sukces. Normalnego unload importera nie
potwierdzamy ani nie zmieniamy tego zakresu.

Nadrzędny proces ponownie ocenia confirmed_capture, porównuje oddzielne raporty
workera/katalogów i rzeczywiste bajty przechwyconego pliku. Dopiero po pozytywnym
wyniku kopiuje ten plik do etapu Feature. Nie ma wejścia na wcześniejszy
captured.x_t ani zapasową historyczną geometrię. Nowa opcja --model-sha256
wiąże wejście dziecka Feature z aktualnym przechwyceniem: SHA przed i po,
ścieżka i niezmienność pliku muszą zgadzać się z rodzicem.

Etap Feature zachowuje sprawdzony schemat z 14.5: bieżące publiczne CLS348,
ograniczona projekcja maski 16, CString ścieżki dodatku, oryginalne callbacki
I/O, PK_ERROR_ask_last i rzeczywiste ObjectDoWrite2. Natywny rekord musi
zawierać jeden bieżący source record oraz niepustą geometrię. Wymagany jest
także rzeczywisty samodzielny zapis Parasolidu, prawidłowe wyjście dziecka,
zamknięcie CLS, przywrócenie konfiguracji i niezmienność katalogów.

Monitor Acam działa podczas natywnych etapów i na granicach przebiegu.
Przed i po obu etapach sprawdzane są pliki instalacji, oryginalny STEP,
jego kopia, przechwycenie i kopia Feature. Późny błąd kontroli usuwa wyłącznie
ARD utworzony przez tę próbę i wyklucza go z ZIP-a. Archiwum ma jawny wykaz
dozwolonych raportów i wyników, bez plików TEMP, cache ani modułów producenta.

Dla źródłowego SHA znanego Bryła_4.stp wymagamy dotychczasowego odniesienia
1200 × 2300 × 10 mm. Dla innego STEP-a zapisujemy pomiary bez deklarowania
zgodności z niezależną referencją; --expected-size może dostarczyć taką referencję.
Dotychczasowe wywołanie validate_measurement bez nowego argumentu nadal wymaga
wymiarów starego modelu. Jeden body i poprawne bounds/counts nie zastępują
pełnej walidacji topologii ani objętości. Baza ARD zachowuje istniejące rekordy 2D.

Walidacja: 62 testy adapterów, 65 testów SolidImport i 21 testów STEP/ARD
(148 łącznie), py_compile, git diff --check. Nowe testy używają jawnych atrap
obu etapów natywnych, sprawdzając rzeczywiste kopiowanie, wiązanie SHA,
walidację, składanie rekordu/ARD i whitelistę ZIP-a. Późne wykrycie Acam.exe
usuwa własny ARD i wyklucza go z archiwum. Nie są to testy natywne Windows.

Wszystkie trzy źródła C# i PowerShell przechwycenia porównano bajtowo z paczką
udanej próby 13: bez zmian. Paczka testu 15 ma 42 pliki, nie zawiera DLL/EXE
ani wcześniejszego Parasolidu. Sprawdzono integralność ZIP, każdy wpis manifestu,
składnię wszystkich Pythonów, 21 testów i trzy CLI --help z osobno rozpakowanej
paczki. SHA256 paczki:
`33cd8d3179e3a549c10f00cc17915fcc2898d78f63c3633cf70e88ef924b159d`.
