# Test 14.4 — odczyt ostatniego błędu Parasolidu

Status: wykonany w Windows; odczyt diagnostyczny działa, zapis geometrii nieudany.

Wynik użytkownika: `step_ard_test_14_4_20261017_215635_757887.zip`,
SHA256 `089edc3b2bf536fed3a646e8273faee43f6c5a23e1e4e582f342354a46315283`.
Integralność ZIP sprawdzona. Import daje jedną bryłę 1200 × 2300 × 10 mm,
63 ściany, 158 krawędzi i 100 wierzchołków. Acam.exe nie był obserwowany
w 38 próbkach; sesja CLS została poprawnie zamknięta.

Przed Bodies.Save nie ma ostatniego błędu, po wywołaniu jest kod **970**.
Sam Save zwraca HRESULT 0 i liczbę 1, lecz plik geometrii nie powstaje.
Znaczenie kodu 970 pozostaje nieustalone. Odczyty przed i po ObjectDoWrite2
nadal pokazują 970; ponieważ nie kasowano błędu, nie dowodzi to nowego
błędu tego samego rodzaju podczas writera. Writer zwraca 1, a rzeczywisty
rekord ma 157 bajtów i kończy się przed geometrią. ARD nie powstał.

Rzeczywiście załadowany moduł:
`C:\Program Files\Hexagon\ALPHACAM 2025\Add-Ins\Feature\PSKERNEL.dll`,
SHA256 `c900fa3f430fe67c2adb15a50f9604d9812ed81a02c5006b66689dac28263073`.
IAT zgadza się z eksportem; wszystkie odczyty zwracają 0, osłony buforów
są nienaruszone, plik jądra pozostaje bez zmian. Nazw PK_* nie odczytano.
Do dalszej analizy potrzebny jest ten dokładny moduł lub dokumentacja
kodów błędów odpowiadająca jego wersji; nie przypisujemy 970 do licencji,
ścieżki ani wersji formatu bez dowodu.

Po 14.3 wiadomo, że import i pomiary działają, natomiast oba zapisy nie dają
geometrii. Sam writer zwraca 1 i zostawia 157 bajtów nagłówka źródła.

Dodano odczyt publicznego PK_ERROR_ask_last z załadowanego PSKERNEL.dll,
przed/po Bodies.Save i przed/po ObjectDoWrite2. Nie instalujemy error handlera,
nie zmieniamy IAT/kodu/opcji transmisji, nie wywołujemy PK_ERROR_clear_last.
Ścieżka i SHA256 rzeczywiście załadowanego jądra trafią do raportu; po próbie
sprawdzana jest stabilność tego pliku.

ABI odtworzono z oryginalnego wywołania Feature.dll przy RVA 0x105400:
RCX wskazuje logiczny wynik, RDX strukturę błędu. Thunk 0x1311ad skacze
do importu PK_ERROR_ask_last przy IAT 0x13f7b8. Oryginalny callback
zarejestrowany przez Feature.dll przy 0x100ea0 zwraca DWORD [error+0x20].
Z tego wynika sprawdzony offset kodu błędu w tym buildzie.

Czytnik porównuje IAT z GetProcAddress tego samego jądra. Dwa bufory są
nadmiarowo przydzielone i otoczone canary, a logiczny wynik sprawdzany.
Odczyt danych błędu następuje tylko dla query=0 i was_error=true. Reszta
struktury pozostaje nieinterpretowana. Potencjalne wskaźniki tekstów są
czytane przez ograniczony ReadProcessMemory, z limitem 192 bajtów i filtrem
krótkich nazw PK_*. Nie zapisuje się surowych zrzutów pamięci. Windows
LastError jest zachowywany i przywracany, również w razie błędu czytnika.

PK_ERROR_ask_last i osobne PK_ERROR_clear_last opisuje oryginalny podręcznik
PK Interface Programming Reference Manual. Publiczny przykład dostawcy
[Tech Soft 3D](https://docs.techsoft3d.com/hoops/mesh/guide/examples/examples.html)
pokazuje także pola function, code_token i code w standardowym błędzie.
Czytnik nie zgaduje ich wszystkich offsetów: raportuje tylko kod z offsetu
sprawdzonego w aktualnej DLL oraz nazwy PK_* z opisanym offsetem.

Porównanie stanu przed i po jest istotne: ostatni błąd może pochodzić ze
wcześniejszej operacji. Sam niezerowy zapisany błąd nie jest automatycznie
przypisywany nowemu wywołaniu i nie zastępuje kodów oryginalnego writera.
Warunki ARD z 14.3 pozostają zachowane, dodatkowo wymagamy stabilności
załadowanego jądra. Pełna geometria, topologia i objętość nie są potwierdzone.

Walidacja: 56 testów adapterów oraz 12 STEP/ARD, Python py_compile.
Obejmuje odczyt tylko zdefiniowanych danych, granice wskaźników, canary,
zachowanie LastError oraz punkty odczytu przed/po bez zmiany wyniku writera.
Wywołanie do zainstalowanego jądra zostało potwierdzone wynikiem Windows
opisanym powyżej. Nie potwierdza to poprawnego zapisu ani pełnej geometrii.
