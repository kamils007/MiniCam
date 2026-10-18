# Test 14.2 — normalizacja argumentów refleksji PowerShell

Status: wykonany na Windows; potwierdzony CLS i pomiar jednej bryły, ARD nie powstał.

Wynik `step_ard_test_14_2_20261017_214152_926443.zip` ma SHA256
8fd2688f53e03a1b2e9cd902d022ae952f51532ec5fcaa2815534f9cd5131319.
ZIP jest integralny, osobne ready/final CLS zgadzają się z raportem, a surowe
checkpoints i wynik procesu odpowiadają kopii w raporcie. Walidatory CLS,
projekcji Save i pomiarów przeszły na rzeczywistych danych tej próby.

Selftest adaptera argumentów wykonał się poprawnie. CLS Open=1,
walidacje przed, po odczycie 348 i po procesie Feature=101, 348=false raz,
projekcja=0, normalne Close potwierdzone, exit=0. Nie było przyznanego 348
do indywidualnego zwolnienia. Proces Feature zakończył się 0, bez błędów
natywnych i nieobsługiwanych metod. Tabela 585 CString została poprawnie
zainstalowana, odczytana i zdjęta; pełny cleanup się zakończył.

ImportAs zwróciło 1; Bodies.Count=1. Rzeczywiste pomiary:
X=-600..600, Y=-1125..1175, Z=65..75, czyli 1200 × 2300 × 10 mm;
63 ściany, 158 krawędzi, 100 wierzchołków. Wszystkie sześć ustawień wyrównania
wyłączono i odczytano. Nie twierdzimy, że pełny kształt/topologia/objętość są
zweryfikowane. Wszystkie osiem wejść/plików instalacji pozostało zgodnych
z profilem i niezmienionych. Nie zaobserwowano Acam.exe w 38 próbkach.

Bodies.Save dało HRESULT=0 i liczbę 1, ale roundtrip.x_b nie istniał.
W raporcie save_succeeded_with_file=false, output=null. Test poprawnie nie
uznał samego kodu API za dowód pliku i zatrzymał się przed ObjectDoWrite2.
Nie powstał fragment ani ARD. Dalszą próbę stanowi test 14.3.
Przyczyną zatrzymania 14.1 był PSObject z wyniku Join-Path przekazany jako
argument System.String do SetClsPath przez MethodInfo.Invoke. Sesja CLS nie
została otwarta i proces Feature nie wystartował. Wszystkie pliki były zgodne
z oczekiwanym profilem i niezmienione.

Ścieżka CLS ma teraz jawny typ string. `feature_cls_invocation.cs` wykonuje
wyłącznie normalizację argumentów: usuwa PSObject.BaseObject (ograniczenie
głębokości 16), wymaga dokładnego typu każdego parametru i przekazuje oryginalne
wartości do wcześniej sprawdzonej publicznej statycznej MethodInfo.
Nie konwertuje enum z dowolnego numeru ani nie zmienia wyników CLS.
Zero parametrów dopuszcza null, tak jak oryginalny MethodInfo.Invoke.

Przed pierwszym CLS wywoływany jest selftest adaptera: opakowany tekst,
DayOfWeek i Int32 muszą przejść poprawnie; Int32 zamiast enum musi zostać
odrzucony. Wynik trafia do cls_ready.json i cls_result.json. Zarówno parent
14.2, jak i natywny worker wymagają potwierdzenia selftestu, zanim uznają sesję
za poprawną. Selftest nie zastępuje prawdziwego CLS ani nie nadaje uprawnień.

Raport zawiera api_call, failed_api_call, error_line i wewnętrzny komunikat
błędu. Cleanup nadal dotyczy wyłącznie własnej sesji i rzeczywiście przyznanego
348, z odróżnieniem 202 NotOptional od indywidualnego zwolnienia.

Profil Feature.dll 2025.4.1.200, 585 CString, wejście testu 13, maska Save 0x10,
pomiary i warunki zapisu ARD pozostają takie jak opisano w teście 14.1.
Nie powtarzamy translacji STEP. Pełna geometria i objętość nadal niepotwierdzone.

Walidacja lokalna: 11 testów kontrolnych, składnia C#/PowerShell przez parsery,
Python py_compile, integralność ZIP i importy/testy po rozpakowaniu w osobnym
katalogu. Selftest C# oraz prawdziwe API CLS wykonają się dopiero na Windows.
