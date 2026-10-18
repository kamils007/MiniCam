# Test 9 — adapter COM ILicense z rzeczywistym CLS

Zamknij ALPHACAM, rozpakuj cały ZIP i uruchom `URUCHOM_TEST_9_ADAPTER_CLS.cmd`.
Prześlij ZIP `cls_adapter_test_9_...zip`, którego ścieżkę wypisze test.
Wymagania i instalacja są takie same jak w teście 8: Windows, Python x64,
zainstalowany ALPHACAM 2025.4.1.104 i sprawdzone moduły CLS. Niczego nie
trzeba doinstalowywać. Alternatywny katalog: `--root "D:\inna\lokalizacja"`.

Adapter jest nowym lokalnym obiektem COM. Udostępnia IID ILicense i natywną
metodę IsFeatureLicensed w slocie 0x48, zgodnie z metadanymi Twojego Acam.exe.
Wywołanie Core:Solids przekazuje do publicznego GetAccessCode(390) w rzeczywistej
sesji CLS. Sesja jest sprawdzana przed zapytaniem i po nim. Odmowa daje false;
błąd walidacji daje błąd HRESULT. Nie ma wymuszonego pozytywnego wyniku.

Test przechodzi przez rzeczywisty wskaźnik COM, QueryInterface i tablicę
metod, a nie samo wywołanie metody C#. Kontroluje dwubajtowy VARIANT_BOOL
i otaczającą go pamięć. Nieznana funkcja i pusta nazwa muszą zostać odrzucone
bez kolejnych zapytań do CLS. Pozostałe metody ILicense zwracają E_NOTIMPL;
nie powstają sztuczne uchwyty modułów ani deklaracje uprawnień do wydania.

Ta wersja adaptera obsługuje jedno zapytanie w pojedynczej próbie. Jej zakres
nie obejmuje jeszcze pełnej polityki ILicense, ProgramLevel, SpecialKey,
lokalizacji interfejsu ani hosta SolidImport. **Test 9 nie uruchamia importera
i nie konwertuje STEP.** To sprawdzenie brakującego połączenia CLS → COM;
wykonanie na Windows jest wymagane przed dalszą próbą importu.

Na końcu test zwalnia własne referencje COM, próbuje zwolnić uzyskane
uprawnienie i zamyka własne połączenie CLS. NotOptional (202), znane z testu 8,
jest zapisywane osobno i nie jest uznawane za potwierdzenie indywidualnego
zwolnienia. Dopuszczenie tego wyniku nie zastępuje wymaganego powrotu
z CloseLicenseConnection bez wyjątku; zwolnienie stanowiska na serwerze
pozostaje niepotwierdzone. Test 8 zachowuje wcześniejsze ścisłe kryterium.

Nie zmienia rejestru, bibliotek, konfiguracji licencji ani zegara. Acam.exe
jest tylko odczytywany do kontroli skrótu. Nie uruchamia SolidLinka i nie
wywołuje SendLicenseString lub ConfirmParasolidLicensed. ZIP raportu zawiera
wyłącznie własne raporty i logi; bez modeli, bibliotek producenta, plików
licencji i zawartości TEMP. Limit: 90 sekund; przerwanie procesu nie potwierdza
sprzątnięcia sesji.
