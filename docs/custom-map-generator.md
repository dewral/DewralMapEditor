# Generator własnych materiałów — Dungeon i Cave

Otwórz **Tools → Custom Map Generator…** po wczytaniu mapy i klienta.
Panel nie blokuje edytora. Własny generator DME działa w osadzonej Lua 5.1:
Dungeon tworzy prostokątne pokoje i korytarze, a Cave nieregularne komory
połączone tunelami. Nie wymaga otMapGenPublic ani instalacji Lua.
Oba tryby korzystają wyłącznie z materiałów przypisanych w profilu.

## Przygotowanie

Wczytaj DAT, SPR i OTB odpowiadające swoim grafikom. Generator używa **server ID
z OTB**, a nie numerów sprite’ów. DME sprawdza typ podłogi i blokowanie ruchu
w danych aktualnego klienta przed każdym uruchomieniem.

## Pierwszy profil „Midhem — jaskinia”

1. Wpisz nazwę profilu. W ustawieniach wybierz **Cave**.
2. Wyszukaj item po nazwie lub ID. Zaznacz go i użyj **+ Add** przy kategorii,
   albo przeciągnij go na kategorię. Dwuklik dodaje item do aktywnej kategorii.
3. Dodaj **Background**: ground wypełniający obszar poza jaskinią.
4. Dodaj **Rooms and corridors**: nieblokujący ground do chodzenia.
5. Opcjonalnie dodaj **Secondary floor**. Zastąpi około 15% pól podłogi.
6. W **Decorations** dodaj pojedyncze kamienie, grzyby lub inne itemy.
   Nie dodawaj fragmentów obiektów składanych z wielu pól.
7. Ustaw rozmiar, liczbę pomieszczeń, seed, gęstość dekoracji i docelowe X/Y/Z.
   Na początek użyj 70 × 70, 8 pomieszczeń i 8% dekoracji.
8. Kliknij **Save**, potem **Generate preview**. Obejrzyj wynik na mapie.
9. **Apply** zatwierdza cały wynik. **Cancel preview** lub zamknięcie panelu
   usuwa go bez zmiany dokumentu.

Nie ma przykładowych ID Midhem wpisanych na stałe: profile muszą odpowiadać
Twojej wersji grafik. Waga 10 oznacza dziesięć razy większą szansę wyboru niż
waga 1 w tej samej kategorii. Można usunąć wpis przyciskiem ×.

## Ściany i bordery

Są opcjonalne. Po włączeniu **Generate walls** trzeba wypełnić wszystkie cztery
sloty: Horizontal, Vertical, Small junction, Corner.
Po włączeniu **Generate borders** trzeba wypełnić wszystkie 12 slotów.
Numery slotów zachowują zgodność z dotychczasowymi profilami:

| Slot | Wariant bordera |
| --- | --- |
| 1–4 | South, West, North, East |
| 5–8 | Outer SE, SW, NE, NW |
| 9–12 | Inner SW, SE, NW, NE |

Ściany są umieszczane na tle przy krawędzi podłogi. Bordery są nakładane
na podłogę; kilka krawędzi i narożnik może znaleźć się na jednym polu.

Kierunki przypisujesz samodzielnie. Generator nie rozpoznaje ich z grafiki.
Wybierz itemy bez ground; osobne warianty w danym slocie mogą mieć własne wagi.
Układ, który zostanie odcięty przez blokujące ściany lub bordery, nie zostanie
przekazany do podglądu. Zmień materiały lub seed zgodnie z komunikatem.

## Profile i podgląd

**Save**, **Duplicate**, **Delete**, **Import** i **Export** obsługują nazwane
profile JSON: materiały, wagi i wszystkie ustawienia generowania.
Profile zapisane w DME są powiązane z profilem klienta. Aby użyć profilu na
innym kliencie, zduplikuj go i popraw materiały. ID są zawsze sprawdzane ponownie.
Biblioteka znajduje się w katalogu danych aplikacji `custom-generator/profiles.json`.

Generowanie odbywa się w tle; przycisk Generate zmienia się w **Cancel generation**.
Obowiązują limity Lua: 120 sekund i 256 MiB. Rozmiar wynosi 24–200 pól na oś,
liczba pomieszczeń 2–16, dekoracje 0–30%. Zbyt mały obszar zwraca komunikat.
Ta sama konfiguracja i seed dają ten sam wynik przy tej samej wersji generatora DME.
Puste kategorie opcjonalne są pomijane.

Podgląd pokazuje cały stos wygenerowanych itemów w rendererze mapy. Zmiana
materiałów, ustawień, aktywnej mapy lub klienta usuwa podgląd. Kolejne Generate
zastępuje poprzedni wynik. Podgląd nie zmienia dokumentu ani historii Undo.

Apply zastępuje itemy na całym wygenerowanym prostokącie, zachowując metadane
pól, w tym strefy, domy i spawny/stworzenia. Powstaje jedna operacja Undo/Redo.
Przedstawiony wynik można następnie zapisać jako OTBM.

Przepisy łączą pomieszczenia i sprawdzają przechodniość obszaru. Blokujące
dekoracje są dodawane tylko, jeśli nie rozdzielą przejść; rzeczywista liczba
dekoracji może być niższa od ustawionej gęstości. Przy dużych obszarach liczba
prób dla dekoracji blokujących jest ograniczona do 256.

Pierwsza wersja generuje jedno piętro. Nie generuje schodów, drabin, połączonych
pięter ani obiektów składanych z kilku pól.
