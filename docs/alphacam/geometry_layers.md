# Przypisanie geometrii 2D do warstw ARD

Porównanie z 18.10.2026 wykazało zamianę dwóch pól w `writePath`.
Pierwszy uint32 geometrii jest identyfikatorem geometrii; uint32 pod offsetem
221 od początku jej danych jest odwołaniem do ID warstwy z rekordu 0x10.
Nie jest to numer porządkowy warstwy po nazwie ani numer wiersza w panelu.

Źródła prywatne (nie dołączamy modeli do repozytorium):

| Plik | SHA256 |
| --- | --- |
| miracleDisplay_PODSTAWA.ard, MiniCam, 281974 B | af25d699784f4277973043a35cd096e435692dfbd3b73c09dd54269243fa8e8c |
| miracleDisplay_PODSTAWA_z_ ALphacam.ard, ponowny zapis, 400209 B | de70b6c03ef5755fed7edd12a20d34ca2e69903c5cc0ff31f5add00c7c77c176 |

W obu plikach jest 21 konturów i 1410 odcinków. Każdy odpowiadający sobie
kontur ma identyczne bajty elementów, obwiednię i nazwane atrybuty poziomów Z.
Zmiana numeru atrybutu TempPickZ z 7 na 9 jest uwzględniona przez jego nazwę.

| Pole geometrii | MiniCam przed poprawką | Po zapisie ALPHACAM |
| --- | --- | --- |
| uint32 +0 | 1, następnie czternaście 3 i sześć 2 | 1…21 |
| uint32 +221 | 1…21 | 1…10, następnie jedenaście 3 |

Tabela warstw źródłowych ma ID 1=userKonturZew, 2=userOtwory, 3=GEOMETRIE APS,
4=KONSTRUKCJE, …, 9=TEXT, 10=warstwa bez nazwy. Zatem geometrie 4…10 trafiały
na inne istniejące warstwy, a 11…21 wskazywały ID nieobecne w źródłowej tabeli.
Po ponownym zapisie te ostatnie odwołania mają wartość 3. Kontury 9 i 10
wskazują TEXT i warstwę bez nazwy, choć zamierzone przypisanie to APS.

Poprawka zapisuje kolejne identyfikatory pod +0, a `path.layerId` pod +221.
Odrzuca także niepuste geometrie z ID warstwy 0 lub spoza zapisywanej tabeli.
Mapowanie ID w `ardLayerId` oraz kolejność i format rekordów warstw pozostają
takie jak w porównanych plikach. Nie zmieniamy bitów stanu geometrii;
nie utożsamiamy ich bez dowodu z przynależnością do warstwy.

Kontrola regresji `ard-layers-test` odczytuje niezależnie oba pola, współrzędne
i tabelę warstw. Obejmuje 12 geometrii z powtarzającymi się warstwami, więcej
geometrii niż warstw, pusty kontur, APS bez warstw użytkownika, warstwy
użytkownika i odrzucenie nieistniejącego ID. Test nie przechodzi na starym
writerze (nieunikalne identyfikatory geometrii), przechodzi po poprawce.

Poprawiony writer C++ odtworzył całą prywatną próbkę bajtowo identycznie z
kopią, w której zamieniono wyłącznie te dwa pola w każdym konturze.
Kopia ma 281974 B, SHA256
`4a4ea58faa43fa8ebdb2864faf146ffab73fa6775413c6ac5d75ed14e746d061`.
Zmieniło się 38 bajtów; pozostałe bajty, w tym rekord Feature i końcówka
dokumentu, są identyczne ze źródłem. Poprawne przypisania: 1 kontur zewnętrzny,
14 geometrii APS, 6 otworów. Przeszły kompilacja Linux oraz dwa testy CTest,
w tym dotychczasowy test położenia bryły przez STEP; sprawdzono także zachowanie
rekordu bryły w dokumencie C++.

Użytkownik zgłosił dwa elementy widoczne po wygaszeniu warstw. Błędne odwołania
są potwierdzone, ale nie traktujemy tego jako niezależnego dowodu przyczyny
widoczności tej dwójki. Otwarcie poprawionej kopii, gaszenie warstw i ponowny
zapis w ALPHACAM wymagają potwierdzenia na stacji użytkownika.
