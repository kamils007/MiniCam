# Poprawka importu warstw ARD

Przyczyną powielania było traktowanie wszystkich rekordów warstw poza APS jako
warstw użytkownika. Dotyczyło to również sześciu grup systemowych i pustego
rekordu stylu tekstu. Przy zapisie writer dodawał własne rekordy systemowe,
a błędnie zaimportowane warstwy dopisywał jako użytkownika. Kolejne otwarcie
pokazywało dodatkowe nazwy z przyrostkiem `[ARD ...]`.

Czytnik zachowuje teraz oba numery rekordu 0x10. Rozpoznaje standardowe rekordy
1–8 po zgodności numerów, nazwie i znaczniku odróżniającym rekord użytkownika.
Nie klasyfikuje warstwy tylko na podstawie nazwy. Rekord nr 8 bez nazwy pozostaje
w danych źródłowych i nie tworzy pustej warstwy użytkownika. Nierozpoznana pusta
warstwa użytkownika nadal dostaje nazwę zastępczą; nie jest automatycznie kasowana.

Warstwy systemowe są przechowywane oddzielnie od listy eksportowanych warstw
użytkownika. Panel otrzymuje jawne rozróżnienie grupy systemowej i warstwy
użytkownika, również gdy nazwy są takie same. Widoczność i zaznaczenie nie
łączą tych dwóch grup. Zapis mapuje geometrię systemową na istniejące domyślne
rekordy ARD, bez przenoszenia jej do APS. Konwerter/translator bryły, licencja
CLS i pozostały przebieg eksportu nie są zmienione.

## Sprawdzenie

- Kompilacja aplikacji i pięć testów CTest na Linux: wszystkie przeszły.
- Test panelu Qt: osobne grupy, jednakowe nazwy użytkownika/systemu, przełączanie
  widoczności i czyszczenie przy kolejnym wczytaniu.
- Test importu: brak systemowych/pustych warstw pod użytkownikiem, zachowanie
  prawdziwej warstwy o nazwie Konstrukcje i odrzucenie geometrii wskazującej
  rekord techniczny.
- Testy trzech kolejnych cykli zapisu/odczytu: liczba warstw nie rośnie;
  geometria użytkownika i Konstrukcje zachowują przypisanie.
- Starsza prywatna próbka ARD przeszła kontrolę czytnika; plik nie jest publikowany.
- Ponownego sprawdzenia w nowej kompilacji Windows jeszcze nie wykonano.

## Pozostałe kwestie ze zrzutów

Ta zmiana nie odtwarza jeszcze palety kolorów indeksowanych ani specjalnej
klasyfikacji uchwytów/oprzyrządowania. RGB jest odczytywane jak dotąd, brak
interpretacji indeksów nadal zgłaszamy w ostrzeżeniu. Do porównania PCV i uchwytów
potrzebne są oryginalny ARD ze zrzutu i jego wersja zapisana przez MiniCam;
wcześniejsza próbka ma inny zestaw warstw.

Poprawkę należy sprawdzić od oryginalnego ARD. Już zapisane błędne warstwy
użytkownika nie są automatycznie usuwane ani scalane według podobnych nazw,
ponieważ nie można w ten sposób odróżnić ich od celowych warstw użytkownika.
