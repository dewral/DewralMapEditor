# Audyt pochodzenia kodu DME — 12 września 2026

W DME są rzeczywiste zapożyczenia z TIME i RME. Najmocniejsze dowody dotyczą modelu danych przedmiotów, parsera DAT, silnika automatycznych borderów, tablic ścian, obliczania oświetlenia oraz wbudowanych danych. Nie ma podstaw z tego badania, aby nazwać cały edytor kopią tych projektów.

## Zakres i metoda

- Badany był bieżący katalog roboczy DME, łącznie z niezacommitowanymi zmianami. HEAD: `0d46bb6`. Nie zmieniano kodu aplikacji ani istniejących opisów pochodzenia.
- Sprawdzono historię lokalnego Git, zwłaszcza pierwszy commit `057f0bb` z 17 lipca 2026 i dodanie NOTICE w `9c1c3ee`.
- Pobrano TIME, commit `1a62d66a3d895e30cdcd6d2f35f6956e9f36522e` z 4 czerwca 2026, oraz OTAcademy/RME, commit `5b179c21ff5ae57ccefd1df97ff4264cac8b0340` z 28 stycznia 2026. Oba poprzedzają pierwszy lokalny commit DME. Commit RME odpowiada deklaracji w `data/RME_SOURCE.md`.
- Przeskanowano pliki C++/nagłówki/QML/Python/GLSL w `libs`, `editor`, `scripts` pod kątem zgodnych ciągów linii z kodem TIME i RME. Pomijano komentarze, białe znaki i krótkie linie; kandydatów weryfikowano ręcznie. To wykrywa część podobieństw, ale nie wszystkie przeróbki ze zmienionymi nazwami.
- Obliczono wartości tablic RME i porównano je z DME. Uruchomiono istniejący konwerter danych do osobnego katalogu audytu i porównano wynik z `data`.

Kopie źródeł, skrypty i wyniki pomocnicze znajdują się w `.cache/provenance-audit/`: `scan.py`, `matches.json`, `check_evidence.py`, `evidence.json`, `asset-matches.json`. To materiały lokalnego audytu, nie nowe zależności aplikacji.

## Potwierdzone zapożyczenia i adaptacje

### 1. Automatyczne bordery i ściany — RME

- `editor/core/brushstore.cpp:175`: **256/256** wartości `kBorderTypes` jest zgodnych z `GroundBrush::border_types` w RME.
- `editor/core/brushstore.cpp:1300`: **16/16** wartości `kFull` i **16/16** `kHalf` jest zgodnych z tablicami `WallBrush`.
- `editor/core/brushstore.cpp:1332`: `getBrushTo` zachowuje rozgałęzienia RME dotyczące priorytetu z, borderów wewnętrznych/zewnętrznych, konkretnego sąsiada, wildcardu i pustego pola.
- `editor/core/brushstore.cpp:1374`: `computeBorderItems` jest adaptacją `GroundBrush::doBorders`: osiem sąsiadów, grupowanie w klastry, priorytety opcjonalnych borderów, sortowanie i rozpakowanie do czterech kierunków z tablicy.

Historia potwierdza kierunek adaptacji. W `057f0bb:editor/core/brushstore.h:15–17` zapisano wprost odtwarzanie `GroundBrush::doBorders + getBrushTo` oraz przeniesienie tablicy z `brush_tables.cpp`. Pierwsze README również deklarowało port. To mocniejszy dowód niż samo podobne zachowanie pędzla.

Źródła: [tablice RME](https://github.com/OTAcademy/RME/blob/5b179c21ff5ae57ccefd1df97ff4264cac8b0340/source/brush_tables.cpp#L28), [algorytm RME](https://github.com/OTAcademy/RME/blob/5b179c21ff5ae57ccefd1df97ff4264cac8b0340/source/ground_brush.cpp#L574).

Nie oznacza to, że cały obecny `brushstore.cpp` jest skopiowany. Zawiera też późniejsze rozszerzenia DME, edycję definicji i obsługę Qt/JSON. Historyczne komentarze wskazują również wzorowanie ścian i doodadów na RME; pełnego kopiowania implementacji doodadów nie ustalono.

### 2. Struktury przedmiotów i parser DAT — TIME

- `libs/otformats/itemtype.h`: długie bloki zgodnych deklaracji i metod odpowiadają `Domain/ItemType.h` z TIME. Skan wykazał 173 zgodne istotne linie poza początkowymi include, rozmieszczone w zakresach 7–188, 190–217, 219–259 i 261–334. To liczba linii po filtracji, nie procent skopiowanego projektu.
- Ten nagłówek jest wymieniony w CMake, ale nie znaleziono jego include ani użyć jego typów poza samym plikiem w przeszukanym kodzie `libs` i `editor`. **Wygląda na pozostawiony, nieużywany kod**; usunięcie należałoby potwierdzić kompilacją.
- `libs/otformats/datreader.h:46–81`: blok pól `ClientItem` zgodny z odpowiednikiem TIME.
- `libs/otformats/datreader.cpp:209–307`: obsługa flag odpowiada `DatReaderBase::readItemFlags`; 67 istotnych linii zgodnych po normalizacji, z zachowanym porządkiem i przypisaniami do pól.
- `libs/otformats/canonicalflags.h:6–55`: zgodna tabela flag. Sama identyczność stałych formatu nie dowodzi kopiowania, ale stanowi kontekst dla znacznie szerszych zgodności.
- Pierwotny `datreader.cpp` wskazywał również RME jako źródło transformacji flag i obsługi ich payloadów. Nie należy przypisywać całej wiedzy o formacie wyłącznie TIME.

Źródła: [ItemType TIME](https://github.com/Open-Tibia-Tools/tibia-imgui-map-editor/blob/1a62d66a3d895e30cdcd6d2f35f6956e9f36522e/ImguiMapEditor/Domain/ItemType.h), [parser DAT TIME](https://github.com/Open-Tibia-Tools/tibia-imgui-map-editor/blob/1a62d66a3d895e30cdcd6d2f35f6956e9f36522e/ImguiMapEditor/IO/Readers/DatReaderBase.cpp#L89).

### 3. Obliczanie światła — adaptacja TIME

`editor/map/mapview_gl.cpp:247–309`, `computeLightChunk`, odpowiada fragmentowi TIME `LightManager::computeChunkLight`:

- obliczenia ograniczone do obszaru wpływu światła w chunku;
- odległość euklidesowa i tłumienie `(poziom - odległość) * 0.2`;
- próg `0.01`, ograniczenie intensywności do `1.0`;
- mieszanie maksimum osobno dla R/G/B i pakowanie RGBA.

Sam wzór nie wystarczałby do ustalenia pochodzenia. Jednak komentarz `057f0bb:editor/map/mapview_gl.cpp:142–143` wprost określa tę funkcję jako port TIME `LightGatherer::gatherForChunk + computeChunkLight`. Aktualna implementacja zachowuje opisany rdzeń, choć integracja i zarządzanie danymi zostały dostosowane do DME.

Źródło: [LightManager TIME](https://github.com/Open-Tibia-Tools/tibia-imgui-map-editor/blob/1a62d66a3d895e30cdcd6d2f35f6956e9f36522e/ImguiMapEditor/Rendering/Light/LightManager.cpp#L238).

Dlatego obecne określenie oświetlenia wyłącznie jako inspiracji pomysłami jest mniej precyzyjne niż dowody w historii. Nie ustalono natomiast, że cały renderer lub cały moduł in-game preview jest portem TIME.

### 4. Wbudowane dane — RME

`scripts/import-rme-data.py:598–663` przekształca definicje RME w `brushes.json` i `tilesets.json`, a `items.xml` i `creatures.xml` kopiuje po usunięciu bajtów NUL i sprawdzeniu XML.

Odtworzenie importu dla 23 profili dało 92 pliki:

- **67 identycznych bajt w bajt** z obecnymi plikami DME;
- **25 różnych**: wszystkie 23 `brushes.json` oraz `772/creatures.xml` i `772/tilesets.json`.

Różnice mogą wynikać z późniejszych zmian danych lub konwertera; nie oznaczają niezależnego pochodzenia. Konwerter i `data/RME_SOURCE.md` dokumentują źródło tej rodziny plików. Nie porównywano każdej zmienionej definicji osobno.

## Fragmenty wymagające ostrożniejszej oceny

| Obszar | Ustalenie |
|---|---|
| `binaryreader.h/.cpp` | Podobne API, układ podstawowych operacji i komunikaty jak TIME; adaptacja jest prawdopodobna w kontekście NOTICE. To jednak proste, standardowe operacje — bez rozstrzygającego dowodu dla każdej funkcji. |
| `otbreader.cpp/.h` | Wspólne atrybuty i sekwencje odczytu formatu, lecz istotnie inna implementacja odczytu payloadów i model Qt. Nie potwierdzono skopiowania całego parsera. |
| `otbmreader.h/.cpp` | Zgodności m.in. enumów nagłówka; to w dużej części wymagania formatu. Brak podstaw, by uznać całe ponad 3 tys. linii implementacji za kopię. |
| `nodefilereader.*`, `sprreader.*` | Inne mechanizmy przechowywania/odczytu niż TIME; wspólna semantyka formatu. Nie ustalono dokładnego zakresu ewentualnej wcześniejszej adaptacji. |
| `otfireader.*`, `itemsxmlreader.*` | Nie znaleziono mocnych dowodów kopiowania z dwóch badanych źródeł. |
| QML, aktualizator, generatory, compact storage | Skan nie ujawnił znaczących zgodnych bloków z badanymi źródłami. To nie certyfikat niezależności od wszystkich innych projektów. |
| OpenGL i pomocnicze obliczenia sprite’ów | Krótkie podobieństwa dotyczą standardowych wywołań lub odczytu wymiarów. Odrzucono je jako samodzielny dowód zapożyczenia. |
| Grafiki | Brak identycznych hashy badanych PNG/SVG/JPG/BMP/OTFI z `editor` i `data` względem pobranych TIME/RME. Nie wyklucza to innych źródeł, konwersji ani zmian obrazów. |

## Co wynika z audytu

Zakres ewentualnej wymiany obejmuje więcej niż samą obsługę plików: **DAT/model danych, auto-bordery i tablice ścian, rdzeń oświetlenia oraz dane RME**. Najłatwiejszym kandydatem do porządków jest pozornie nieużywany `itemtype.h`; aktywne algorytmy wymagają zastąpienia i sprawdzenia zachowania.

Nie ma uzasadnienia, aby na podstawie tych wyników przepisywać cały interfejs i wszystkie narzędzia. Nie podaję „procentu kradzieży”: zgodność tekstowa, adaptacja algorytmu, zgodność z formatem i import danych to różne rzeczy. Badanie ustala techniczne pochodzenie, nie rozstrzyga naruszenia licencji ani autorstwa każdego fragmentu.

Wcześniejszy szacunek wymiany samych parserów był niepełny względem celu usunięcia wszystkich stwierdzonych zapożyczeń. Dokładny harmonogram powinien uwzględnić także pędzle, światło i decyzję o zachowaniu lub zastąpieniu danych RME.
