# Potwierdzony odczyt bryły z testFAz.ard przez Feature

Źródło: dostarczony przez użytkownika `raport_odczytu.zip` z rzeczywistego testu
Windows, schemat `minicam-ard-feature-read-v1`. Raport ponownie sprawdzono
walidatorami CLS, odczytu i pomiarów. Ekstrakcję porównano z pierwotnym ARD
przechowywanym poza repozytorium.

- ARD SHA-256: `13b01c7743bf2e97ca1b503362a476b08706ce562a49c17f3ce1a22c8974e93f`.
- Parasolid: 36 320 bajtów, SHA-256
  `841144d6d6fb91c3d6edb2b67e30a99934f288692bef8d312322f93d024af571`.
- Feature ImportAs: HRESULT 0, wynik 1, Bodies.Count = 1.
- Min XYZ = (0, 0, 0), Max XYZ = (400, 600, 18); profil API w milimetrach,
  bez dopasowania skali lub automatycznego wyrównania.
- Faces.Count = 48, Edges.Count = 126, Vertices.Count = 80.
- Proces Feature i właściciel CLS zakończyły się kodem 0. Licencja pozostała
  ważna do końca testu; CloseLicenseConnection potwierdzone.
- Nie zaobserwowano Acam.exe w 30 próbkach monitora. Nie było wywołania Save
  ani serializacji ARD. Źródło i sprawdzane pliki runtime pozostały niezmienione.

**Potwierdzono ARD → wydzielenie oryginalnego Parasolid → import do Feature →
pomiar bryły i odczyt liczników topologii.** To nie było wywołanie ObjectRead3;
użyto rozpoznanej ścieżki ImportAs na osadzonym X_B.

Nie potwierdzono konwersji do OCCT/STEP, wyświetlenia w MiniCam, pełnej
poprawności topologii, objętości ani zgodności każdego elementu z oryginałem
ALPHACAM. Raport jawnie zawiera converted_to_occt=false, geometry_verified=false
i volume_verified=false. Następny nierozwiązany etap to przekazanie rzeczywistej
geometrii do OCCT. Nie powtarzać diagnostyki ścieżek lub istnienia metod jako
zamiennika tego etapu.

## Raport geometrii v4 — blokada Options

Kolejny raport użytkownika: ARD SHA-256
`20ffc7d04de2e31c833624bc9d7f6855864e230d950322453ba3a7ac12089a5b`.
ImportAs ponownie zwrócił 1, Bodies.Count = 1. Podczas pobierania szczegółów
Feature wywołał IDrawing.Options (PROPERTYGET, DISPID 95, slot x64 0x350).
Host nie obsługuje tej metody i zgodnie z zasadą przerwania nieznanego wywołania
zakończył proces kodem 80. Brak feature_geometry.json; brak potwierdzenia pełnego
odczytu geometrii. CLS zamknięty prawidłowo, wejście i runtime niezmienione.
Sam raport nie określa, który getter Feature wywołał Options. Nie wolno zgadywać
wartości ustawień ani traktować importu jako udanej konwersji do OCCT.

Paczka v5 dodaje geometry_calls.jsonl z zapisem i flush przed każdym getterem,
Item i IsSame oraz checkpoint pomiarów przed geometrią. To diagnostyka przyczyny,
nie implementacja Options i nie potwierdzona naprawa. Dziennik zostaje włączony
do raportu nawet przy natychmiastowym zakończeniu procesu przez host.
Sprawdzono 13 testów Python, pyflakes i integralność ZIP. Natywny test wymaga Windows.
