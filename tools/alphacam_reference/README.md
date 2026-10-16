# Wzorzec integracji Feature.dll — test 4.9

Ten katalog przechowuje działający wzorzec Pythona do porównywania z portem
C++ w `tools/ard/FeatureHost.cpp`. Python nie jest zależnością MiniCam.
Skrypty `.py`, `.cmd` i kontrakt JSON skopiowano bez zmian z testu 4.9;
ich skróty zawiera `source_manifest.json`.

Punkt wejścia do całej wiedzy: [przekazanie integracji](../../docs/alphacam/integracja.md).

## Powtórzenie próby na Windows x64

1. Z własnej zgodnej instalacji pobierz `Feature.po` do tego katalogu.
   DLL i jej zależności pozostaw w instalacji. Akceptowane skróty są w dokumentacji.
2. Przy otwartym ALPHACAM 2025 Router uruchom `ODCZYTAJ_USTAWIENIA_2025.cmd`.
   Zapisze lokalny `host_environment_2025.json`. Jeśli masz poprawny odczyt dla
   tej samej instalacji, możesz go tutaj skopiować zamiast ponawiać odczyt.
3. Zamknij ALPHACAM. Uruchom `URUCHOM_TEST_4_2025.cmd`.
4. Wybierz zgodną `Feature.dll` oraz testowy model Parasolid `.x_b`/`.x_t`.
5. Wyniki znajdziesz w nowym katalogu `feature_test_4_9_2025_…`:
   raport JSON, `roundtrip.x_b`, `feature_object.bin` i logi.

Wymagany Python 3 x64, standardowa biblioteka, bez pakietów pip.
Ten wariant repozytorium celowo nie zawiera katalogu tekstów producenta,
odczytu ustawień, DLL, modelu ani surowych raportów z komputera użytkownika.
Skrypty odrzucają inną wersję biblioteki/katalogu zamiast zgadywać ABI.

## Kontrole bez ładowania DLL

Z tego katalogu:

```sh
python -m unittest discover -p "validate_*.py" -v
```

Pełne 46 kontroli wymaga lokalnego `Feature.po` o zgodnym skrócie.
Bez niego można uruchomić 41 kontroli niezależnych od katalogu:

```sh
python -m unittest validate_host validate_layers validate_native_trace validate_host_environment validate_object_serialization -v
```

Testy używają lokalnych atrap COM/CRT/MFC. Uruchomienie ich na Linuksie
nie potwierdza działania prawdziwej Feature.dll ani portu C++ na Windows.
