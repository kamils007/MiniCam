# Dane geometrii bryły do przekazania z Feature do OCCT

1. Zamknij ALPHACAM, jak podczas poprzedniego testu.
2. Uruchom `TEST_GEOMETRII_BRYLY.cmd` i wybierz plik ARD z jedną bryłą.
3. Prześlij `raport_odczytu.zip` ze wskazanego folderu `odczyt_...`.

Ten etap korzysta z potwierdzonego wczytania bryły. Odczytuje współrzędne
wierzchołków, typy i właściwości krawędzi oraz powierzchni, pętle brzegowe ścian
i powiązania pomiędzy elementami. Wynikiem jest `feature_geometry.json`
wewnątrz raportu ZIP, ze skrótem i kontrolą zgodności liczników topologii.
Brakujące właściwości są jawnie odnotowane w `property_errors`.

Identyfikatory w JSON są lokalnymi numerami tego odczytu, nie identyfikatorami
Parasolid lub właściwością Feature.ID. Powtarzające się elementy są rozpoznawane
przez zachowany uchwyt interfejsu lub read-only metodę IsSame; uchwyty pozostają
ważne do zakończenia odczytu i są następnie zwalniane. Błąd porównania przerywa
odczyt, zamiast tworzyć pozornie zgodne powiązania.

Nie jest wymagana identyczność pliku z pierwotnie przesłanym testFAz.ard.
Czytnik przechodzi po rozpoznanych rekordach ARD V1.94, sprawdza ich granice,
rekord Feature v3 i końcówkę dokumentu. Nie szuka znacznika bryły wewnątrz
miniatury lub danych innego obiektu. Nieznane układy są odrzucane czytelnym
błędem. Skróty są obliczane dla rzeczywiście wybranego ARD i wydzielonej bryły;
Feature nadal musi potwierdzić odczyt dokładnie tych bajtów.

To **dane do odbudowy modelu**, jeszcze nie gotowa bryła OCCT. Czytnik nie
zastępuje krzywych odcinkami, nie wytwarza pozornej bryły z samej obwiedni
i nie oznacza brakujących powierzchni jako poprawnie odtworzonych.

Ścieżki i Python są pobierane z ustawień MiniCam. Runtime jest kopiowany do
nowego folderu testu; istniejący runtime eksportu nie jest nadpisywany.
Używana jest ta sama inicjalizacja i normalna sesja CLS. Żaden etap tego testu
nie wywołuje zapisu bryły, ObjectDoWrite2 ani translatora STEP.

Raport **zawiera odczytane dane geometrii modelu** i logi, ale nie zawiera DLL
producenta ani prywatnych katalogów runtime. Paczka pobierana z GitHub nie
zawiera modelu użytkownika. Odczyt tych właściwości przez COM wymaga wykonania
testu na Windows; w chmurze sprawdzono obsługę i kontrolę struktury danych.
