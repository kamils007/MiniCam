# Test 14.3 — bezpośrednie sprawdzenie oryginalnego writera ARD

Status: wykonany na Windows; natywny writer zgłosił błąd, ARD nie powstał.

Wynik `step_ard_test_14_3_20261017_214857_195490.zip`, SHA256
92e83002acde2e28b885630ee7abc2909e9a534d8e777123850ba6853f1937c4,
jest integralny. Ponownie potwierdzono jedną bryłę o wymiarach
1200 × 2300 × 10 mm oraz liczniki 63/158/100. Rzeczywista sesja CLS, odczyt
348=false/projekcja=0, walidacje 101 i normalne zamknięcie są poprawne.
Oba procesy zakończyły się 0; brak nieobsługiwanych metod i wyjątków natywnych,
pliki niezmienione. Nie zaobserwowano Acam.exe w 35 próbkach.

ObjectQueryWrite2 zwrócił 1 i LicomUKRJMParaSolid. ObjectDoWrite2 zwrócił
błąd 1, flush/close=0. Rzeczywisty feature_object.bin ma tylko 157 bajtów,
SHA256 1c3a5087557272fa8ab27af67e790d823e17649309204f994f44d46204c5467a.
Rozmiar i hash zgadzają się z logiem. Parser zatrzymuje się przy byte 157;
brakuje właściwej geometrii. Pliku nie użyto do stworzenia ARD.
Przyczyna nieudanego transmitu Parasolidu pozostaje do rozpoznania w 14.4.
Po poprawnym imporcie i pomiarach w 14.2 brakowało pomocniczego pliku
roundtrip.x_b. Nie wykonywano jeszcze oryginalnego ObjectDoWrite2.

Ślad w aktualnej DLL: konstruktor z 0x1130 tworzy backend przez 0xead20
i przechowuje go pod 0x184140. Vtable 0x151d60 ma pod slotem 0x2f8 metodę
0x10a3f0. Metoda ta sprawdza rozszerzenie przez CString CompareNoCase
(ordinal MFC 2909, zweryfikowany względem publicznego PDB Microsoftu),
zbiera identyfikatory pasujących brył, usuwa rozszerzenie i wywołuje
PK_PART_transmit_u przez thunk 0x131297 / IAT 0x13f680. Wynik tego wywołania
nie jest przenoszony do wyniku metody: wraca liczba dopasowanych brył.
Sama liczba 1 nie dowodzi utworzenia pliku. Nie ustalono jeszcze konkretnej
przyczyny braku zewnętrznego pliku w 14.2.

14.3 nadal wywołuje oryginalne Bodies.Save z rzeczywistą maską CLS 0/16,
wymaga akceptacji HRESULT i liczby 1. Odrzucenie API zatrzymuje próbę.
W jawnym trybie direct_record oryginalne ObjectQueryWrite2/ObjectDoWrite2
mogą następnie zapisać rekord z importowanej bryły niezależnie od istnienia
roundtrip.x_b. Nie zmienia się kodu bibliotek ani warunków kontroli CLS.
SpecialKey nadal nie jest dostępne poza fazą Save; nieznany konsument zatrzyma
proces jak poprzednio.

Zakończenie completed_native_record jest odrębne od completed z plikiem
roundtrip. save_succeeded_with_file pozostaje false, gdy pliku nie ma.
Budowa ARD wymaga jednej zaimportowanej bryły, pomiarów, działającego hosta
i tabeli tekstów, rzeczywistych kodów natywnego writera, zgodności hash/rozmiaru
rekordu i rozpoznanego prefiksu v3. Faktyczna geometria rekordu jest wydzielana
do ard_geometry.x_b i sprawdzana niezależnie przez nagłówek oraz SHA256.
Wrapper ARD dodatkowo wymaga dokładnie jednej metadanej bryły.

Wymagane są także niezmienione źródła, brak zaobserwowanego Acam.exe,
brak błędów natywnych, prawdziwa sesja CLS oraz jej normalne zamknięcie.
Wymiary i liczniki nie oznaczają pełnej poprawności topologii, kształtu ani
objętości. Konieczne będzie otwarcie nowego ARD i nałożenie oryginalnego STEP.

Walidacja przygotowania: 12 testów STEP/ARD oraz 50 testów adapterów hosta,
tekstu i serializacji. Obejmują rozdzielenie statusu rekordu od brakującego
roundtrip oraz odrzucanie niezaakceptowanego Save, pustego rekordu i błędów.
Te testy nie zastępują wykonania Windows i nie dowodzą nowego ARD.
