# TEST 14.4 — rzeczywisty błąd zapisu Parasolidu

1. Rozpakuj cały ZIP do nowego folderu na Pulpicie.
2. Zamknij ALPHACAM i pozostaw zamknięty do zakończenia próby.
3. W folderze `step_ard` uruchom `URUCHOM_TEST_14_4_PARASOLID_ARD.cmd`.
4. Prześlij utworzony `step_ard_test_14_4_....zip`, także jeśli test zgłosi błąd.

Wynik 14.3: bryła odczytana i zmierzona, CLS działa. Oryginalny writer
rozpoznał obiekt, lecz zapis zwrócił błąd 1 i zostawił tylko 157 bajtów nagłówka
z informacją o źródle. Rekord nie zawiera geometrii i nie nadaje się do ARD.

Ta próba odczytuje `PK_ERROR_ask_last` z tego samego załadowanego PSKERNEL.dll:
przed i po Bodies.Save oraz przed i po ObjectDoWrite2. W raporcie pojawią się
numer błędu i dostępne nazwy PK_* opisujące błąd. Odczyt sprzed operacji pozwala
odróżnić błąd już obecny od błędu zaobserwowanego po zapisie.

Czytnik używa wyłącznie publicznego API. Weryfikuje zgodność adresu importu
z eksportem załadowanej biblioteki, identyfikuje jej ścieżkę i SHA256,
a na końcu sprawdza stabilność pliku. Odczyt błędu nie rejestruje callbacków,
nie kasuje ostatniego błędu i nie zmienia opcji Parasolidu. Przydziela własne
bufory z kontrolą granic. Kod błędu ma offset 0x20 sprawdzony w oryginalnym
callbacku Feature.dll; pozostałe pola pozostają nieinterpretowane. Odczyt
tekstów jest ograniczony i raportuje tylko krótkie nazwy PK_*.

Potrzebne są te same moduły i Python x64 co poprzednio. Domyślny katalog:
`C:\Program Files\Hexagon\ALPHACAM 2025`, opcjonalnie `--root "D:\..."`.
Nadal obowiązuje Feature.dll 2025.4.1.200 o wcześniej sprawdzonym SHA256.
Nie należy uruchamiać ALPHACAM przed końcem próby.

Wejściem pozostaje Parasolid z testu 13; nie powtarzamy STEP. Rzeczywisty CLS,
obsługa argumentów, pomiary i oryginalne wywołania zapisu pozostają jak w 14.3.
Pełne SpecialKey nie jest odtwarzane; prawdziwy wynik 348 jest dostępny jako
maska 0/16 wyłącznie podczas Bodies.Save. Odrzucenie API zatrzymuje zapis.

Jeśli wszystkie wcześniejsze warunki i rzeczywisty natywny rekord przejdą
weryfikację, może powstać `STEP_TEST_14_4.ard`. Wtedy dopiero otwórz go
w ALPHACAM, nałóż oryginalny STEP i zapisz `STEP_TEST_14_4_po_otwarciu.ard`.
Prześlij także ten dokument. Nie uznajemy samego kodu wyjścia procesu ani
samego nagłówka za dowód geometrii. Wymiary nie dowodzą pełnego kształtu.

Pakiet sprawdzono testami przenośnymi i kompilacją składni Python.
Nowy odczyt błędu przez API oraz zapis ARD wymagają wyniku wykonania Windows.
