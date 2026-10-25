# Kontekst pracy nad MiniCam

Przed kontynuowaniem prac przeczytaj [stan prac i ustalenia](docs/stan_prac_2026-10-18.md).
To punkt przekazania między czatami; obejmuje także ograniczenia potwierdzonych testów.
W razie późniejszych zmian pierwszeństwo mają aktualny kod, historia Git i nowe
polecenia użytkownika. Nie zakładaj dostępu do wcześniejszej historii czatu ani
do prywatnych plików wymienionych w dokumentacji.

Na zakończenie tego etapu użytkownik zamknął temat eksportu ALPHACAM i wybrał
**otwieranie plików** jako temat kolejnego wątku. Szczegółowy zakres tego nowego
zadania nie został jeszcze określony. Zachowaj istniejący eksport przy pracach
nad otwieraniem; zmieniaj go tylko wtedy, gdy wynika to z nowego zadania.

Aktualne ustalenie z 25.10.2026: pracujemy nad **konwersją całej bryły** zawartej
w ARD (Parasolid → format odczytywany przez OCCT), bez uruchamiania ALPHACAM
i bez API jego działającej aplikacji. Użytkownik wyraźnie odrzucił powrót do
optymalizacji odczytu krawędzi i odbudowy geometrii jako kierunku tego etapu.
Nie zastępuj ustalonej konwersji taką odbudową bez nowego polecenia użytkownika.
Obecny czytnik i eksport zachowujemy do czasu potwierdzenia nowej ścieżki.
Dowody prób, błędy licencji i ograniczenia opisuje
[analiza SolidLink](docs/alphacam/solidlink_converters.md).
