# Generator własnych materiałów — Dungeon i Cave

Otwórz **Tools → Custom Map Generator…** po wczytaniu mapy i klienta.
Panel nie blokuje edytora. Korzysta z bibliotek
[otMapGenPublic](https://github.com/abecek/otMapGenPublic), ale uruchamia przepisy
DME, które używają wyłącznie przypisanych materiałów. Standardowe schematy Tibii
nie są używane.

## Przygotowanie

Pobierz lub sklonuj otMapGenPublic. W panelu wybierz **Choose otMapGen folder…**
i wskaż jego katalog główny, zawierający `config.lua` i `lib/`.
Folder jest wspólny z dotychczasowym panelem Lua Scripts & otMapGen.
Biblioteki pozostają zewnętrzne; DME-dist ich nie zawiera.

Wczytaj DAT, SPR i OTB odpowiadające swoim grafikom. Generator używa **server ID
z OTB**, a nie numerów sprite’ów. DME sprawdza typ podłogi i blokowanie ruchu
w danych aktualnego klienta przed każdym uruchomieniem.

## Pierwszy profil „Midhem — jaskinia”

1. Wpisz nazwę profilu. W ustawieniach wybierz **Cave**.
2. Wyszukaj item po nazwie lub ID. Zaznacz go i użyj **+ Add** przy kategorii,
   albo przeciągnij go na kategorię. Dwuklik dodaje item do aktywnej kategorii.
3. Dodaj **Background**: ground wypełniający obszar poza jaskinią.
4. Dodaj **Rooms and corridors**: nieblokujący ground do chodzenia.
   Możesz też wybrać kategorię **Floors**, docelowy slot oraz nazwę ground brusha.
   Wybór brusha od razu dodaje go do docelowego slotu, bez osobnego kliknięcia.
   Slot pokazuje nazwy dodanych brushy, liczbę itemów i sprite każdego wariantu.
   Zmiana docelowego slotu resetuje dropdown do **Choose ground brush…**;
   wybierz brush osobno dla **Background** i **Rooms and corridors**.
   Wczytywane są wszystkie aktywne warianty podłogi
   i ich wagi. Każdy border brusha dostaje osobną pozycję **Borders · nazwa**
   w dropdownie kategorii, razem z przypisaniem do ground ID, kierunkami,
   rodzajem inner/outer i docelowym gruntem. Kolejne brushe można dodać do
   tego samego slotu; ich bordery pozostają oddzielne.
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
W kategorii **Walls** wybór brusha w dropdownie pobiera warianty i wagi
odpowiednich elementów wall brusha.
W **Borders · default floor** ręcznie przypisywany komplet wymaga wszystkich
12 slotów. W zestawach przypisanych do konkretnego gruntu można zostawić
puste sloty, tak jak w ground brushu. Brakujące wklęsłe narożniki są
zastępowane dostępnymi bokami. Zestaw konkretnego celu ma pierwszeństwo przed
zestawem dla dowolnego gruntu. Bordery są generowane po wyborze podłogi,
więc warianty i secondary floor zachowują właściwe zestawy borderów.
Dodanie pojedynczego ground ID tworzy pusty zestaw **Borders · Ground #ID**,
który można uzupełnić ręcznie. Usunięcie ground ID usuwa jego przypisanie.
Cel **no neighboring ground** oznacza brak gruntu na sąsiednim polu, a nie
puste sloty bordera. Cel **any other ground** dotyczy innych gruntów.
W Brush Managerze sprawdzaj tę samą parę Alignment + Border target:
**Inside ground / Empty (no neighbor)** i **Inside ground / Any other brush**
to dwa oddzielne zestawy. Generator importuje tylko zestawy z itemami;
nie dodaje inner borderów do brusha zawierającego wyłącznie outer bordery.
Numery odpowiadają tablicom bibliotek otMapGen:

| Slot | Wariant bordera |
| --- | --- |
| 1–4 | South, West, North, East |
| 5–8 | Outer SE, SW, NE, NW |
| 9–12 | Inner SW, SE, NW, NE |

Kierunki przypisujesz samodzielnie. Generator nie rozpoznaje ich z grafiki.
Wybierz itemy bez ground; osobne warianty w danym slocie mogą mieć własne wagi.
Układ, który zostanie odcięty przez blokujące ściany lub bordery, nie zostanie
przekazany do podglądu. Zmień materiały lub seed zgodnie z komunikatem.

## Profile i podgląd

**Save**, **Duplicate**, **Delete**, **Import** i **Export** obsługują nazwane
profile JSON: materiały, wagi, zestawy borderów przypisane do gruntów
i wszystkie ustawienia generowania. Tooltipy przy **Import** i **Export**
wyjaśniają ich działanie. Import dodaje profil do biblioteki; eksport zapisuje
bieżące ustawienia, również niezapisane zmiany. Mapę zapisuje się oddzielnie
jako OTBM. Dotychczasowe profile z jednym kompletem borderów nadal działają.
Profile zapisane w DME są powiązane z profilem klienta. Aby użyć profilu na
innym kliencie, zduplikuj go i popraw materiały. ID są zawsze sprawdzane ponownie.
Biblioteka znajduje się w katalogu danych aplikacji `custom-generator/profiles.json`.

Generowanie odbywa się w tle; przycisk Generate zmienia się w **Cancel generation**.
Obowiązują limity Lua: 120 sekund i 256 MiB. Rozmiar wynosi 24–200 pól na oś,
liczba pomieszczeń 2–16, dekoracje 0–30%. Zbyt mały obszar zwraca komunikat.
Ta sama konfiguracja i seed dają ten sam wynik przy tej samej wersji bibliotek.
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
