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
