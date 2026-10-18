# Test 10 — oryginalny importer odbiera odpowiedź CLS

Zamknij ALPHACAM, rozpakuj cały ZIP i uruchom `URUCHOM_TEST_10_IMPORTER_CLS.cmd`.
Prześlij ZIP `solidimport_bridge_test_10_...zip`, którego ścieżkę wypisze test.
Wymagania: Windows, Python x64, bieżąca instalacja ALPHACAM 2025.4.
Inny katalog instalacji: `--root "D:\inna\lokalizacja"`. Limit: 90 sekund.

Ta próba ładuje oryginalny SolidImport.dll 2025.4.1.100 i wywołuje jego
InitAlphacamAddIn na rzeczywistym wątku MFC. Na tym samym wątku otwiera
sesję CLS i udostępnia sprawdzony w teście 9 adapter ILicense.
Tym razem zapytanie Core:Solids musi wyjść z oryginalnego importera;
nie wykonujemy osobnego bezpośredniego sprawdzenia adaptera.

Host wybiera jawny profil `acamLevelCORE=14` z metadanych Acam.exe 2025.4.
Ten profil prowadzi przez oryginalną kontrolę Core:Solids. To wybór kontraktu
hosta, nie deklaracja bieżącego poziomu ani klasyfikacji licencji aplikacji.
Nie oferujemy innych poziomów omijających tę kontrolę.

Host ma prywatne rejestracje importu oraz własny katalog instalacji. Metody
nieobsługiwane kończą próbę po zapisaniu ich nazwy i zamknięciu własnej sesji
CLS. Szczególnie **SpecialKey pozostaje nieobsługiwane** — wcześniejszego
odczytu z wersji 2025.3 nie traktujemy jako aktualnej klasyfikacji 2025.4.
Jeśli inicjalizator o nie poprosi, test zatrzyma się na tym miejscu. Nie
podaje wymyślonej wartości i nie kontynuuje inicjalizacji z niepełnym hostem.

Raport rozdziela przyjęcie licencji przez oryginalny importer od zakończenia
całego InitAlphacamAddIn. Kontrolowany stop 80 po pozytywnym sprawdzeniu
licencji może potwierdzić most, lecz **nie oznacza pełnej inicjalizacji**.
Nowy host używa natywnych prefiksów COM; nie jest pełną implementacją
Automation. Nieobsługiwane sloty są terminalnymi strażnikami.

Nie wywołuje StepInputFile i nie tworzy Parasolidu ani ARD. Nie wyciąga
materiału autoryzacji SolidLink i nie wymusza potwierdzeń licencji. Acam.exe
jest odczytywany tylko do kontroli skrótu. Kontrolowane są skróty pięciu
plików instalacji oraz brak zaobserwowanego Acam.exe. ZIP raportu zawiera
wyłącznie własne raporty i logi, bez bibliotek producenta, plików licencji
i zawartości TEMP. Zakończenie procesu nie potwierdza normalnego unload DLL.

CLS jest walidowane przed i po inicjalizacji. Własne zapytanie 390 jest
zwalniane, jeśli API na to pozwoli. NotOptional (202) pozostaje odrębną
odpowiedzią; nie oznacza indywidualnego zwolnienia. Następnie zamykane jest
własne połączenie CLS. Nie ma potwierdzenia zwolnienia stanowiska po stronie
serwera. Po przekroczeniu czasu sprzątanie sesji pozostaje niepotwierdzone.
