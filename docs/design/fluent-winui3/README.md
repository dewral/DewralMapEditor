# DME — Fluent Dark

Fluent Dark to nazwa motywu aplikacji, opartego na Qt Fluent WinUI 3.
Kod motywu znajduje się w `editor/qml/themes/fluent`, a jego identyfikator
to `fluent-dark`. Poniższe makiety dokumentują rozwój projektu interfejsu.

Projekt koncepcyjny, 16.09.2026. Makieta: `dme-fluent-concept-v1.png`.
Grafika została wygenerowana wbudowanym image_gen; nie jest zrzutem działającej implementacji Qt. Miniatury, mapa i numery ID są ilustracyjne. Specyfikacja poniżej rozstrzyga detale, których obraz nie pokazuje dokładnie.

## Układ

- Pasek tytułu: nazwa aplikacji, wyszukiwarka poleceń Ctrl+K, profil klienta, przyciski okna.
- Pełne menu: File, Edit, Search, Map, Select, Tools, View, Help. Na makiecie brakuje View; w implementacji musi pozostać.
- Pasek poleceń 40 px: pliki, Undo/Redo, Draw/Select/Erase/Lasso, kontekstowe parametry aktywnego narzędzia. Rzadziej używane akcje w menu rozwijanym.
- Paleta po lewej 280–320 px, zwijana: RAW, Items, Terrain, Doodads/Prefabs, Creatures, Houses. Recent/Favorites, wyszukiwanie nazwy i ID, filtr kategorii, miniatury z nazwami, regulacja wielkości miniatur.
- Mapa zajmuje pozostałą przestrzeń. Karty dokumentów, oznaczenie niezapisanych zmian. Panele nie mogą przykrywać podglądu kursora.
- Prawy panel 280–320 px: Inspect, View, Generators. Generator może rozszerzyć panel do 380 px. Nie pokazujemy jednocześnie wszystkich paneli.
- Dolny panel jest domyślnie zwinięty. Zakładki Results, History, Analyzer, Work timer. Wysokość po otwarciu 180–260 px.
- Pasek stanu: współrzędne, piętro, zoom, aktywny profil, stan zapisu, czas pracy. FPS jako opcja diagnostyczna.

## Rozmieszczenie funkcji

| Funkcje DME | Miejsce i zachowanie |
|---|---|
| New/Open/Save/Save As, wiele map, import OTBM, eksport minimapy | File i pasek szybkich poleceń; ostatnie mapy na ekranie startowym |
| Profile 7.60–10.98+, własne profile i ścieżki DAT/SPR/OTB | Selektor profilu otwierający zarządzanie profilami; pełne ustawienia na ekranie startowym |
| Autosave, recovery, updater | Status zapisu w oknie; odzyskiwanie i aktualizacja na ekranie startowym, postęp i wynik czytelne |
| Draw, Erase, Select, Lasso, rozmiar i kształt pędzla | Pasek poleceń i dolna część palety; parametry wyłącznie aktywnego narzędzia |
| Ground, wall, carpet, doodad, warianty i RAW | Kategorie palety z miniaturami; menu pędzla prowadzi do edytora |
| Auto border, optional border, borderize, randomize | Jedno główne sterowanie Auto border przy aktywnym pędzlu; szczegóły w rozwijanym menu. Makieta dubluje przełącznik — implementacja ma tylko jeden |
| PZ, NP, NL, PvP, special zones | Zones: ikona + nazwa + stan aktywny; ustawienia nakładek w View |
| Cut/Copy/Paste/Delete, Rotate Selection, przesuwanie między piętrami | Menu kontekstowe i Select; kompaktowy pasek zaznaczenia. Undo/Redo stale dostępne |
| Cały stos / górny element, piętro bieżące / niższe / widoczne | Pasek kontekstowy Select z jednoznacznymi nazwami zakresu |
| Tile Stack, Browse Field, właściwości itemów | Inspect: uporządkowany stos z miniaturami; ID, subtype, Action ID, Unique ID; szczegóły w sekcjach |
| Kontenery, teksty, teleporty, drzwi, atrybuty | Właściwości kontekstowe; większy edytor jako okno tylko gdy wymaga miejsca |
| Find Item/Everything, Unique/Action IDs, kontenery, teksty, replace/remove by ID | Search; wyniki na dole z wyborem zakresu mapa/zaznaczenie i przejściem do pozycji |
| Domy, towns, waypoints, spawns, potwory i NPC | Paleta Houses/Creatures, zarządzanie w Map; formularze w Inspect, import XML w menedżerze stworzeń |
| Minimap, piętra, zoom, siatka, światło, ambient, animacje, shade, client box | View. Minimap zwijana; piętro/zoom przy mapie; pozostałe przełączniki w panelu widoku |
| Nakładki creatures/spawns/houses/zones/waypoints/pathing/wall outlines, tooltips, placement effects | Grupowane sekcje panelu View: mapa, światło, nakładki, interakcje |
| In-game Preview | View → Play preview; osobne dokowane lub pływające okno z ustawieniami chodzenia i światła |
| Ground Prefab Generator | Generators → Ground prefab. Zawsze bez zaznaczenia; modeless panel, podgląd przy kursorze, klik stawia i losuje następny, PPM kończy |
| Procenty groundów, rozmiar, nieregularność, seed, bordery, doodads | Wiersze z miniaturą, nazwą i procentem; suma udziałów 100%; min/max promienia. Zaawansowane sekcje zwijane |
| Cave i Dungeon | Generators: osobne tryby pracy; w Cave przygotowany mountain ground, opcjonalne ściany, widoczne materiały; podgląd i Apply z Undo |
| Terrain Generator i profile z przykładów | Generators → Terrain: próbka, profil, materiały, parametry, podgląd. Jasny opis dopasowania statystycznego |
| AI Map Assistant | Tools → AI Assistant; profil lokalny/zewnętrzny tylko zgodnie z dostępnym backendem. Nie obiecywać uczenia zamków, którego aplikacja nie implementuje |
| Add Prefab, zapis paczek, Generate Path from Prefabs | Doodads → Prefabs; tworzenie z zaznaczenia i z generatora. Path ma wizualne sloty straight/corner/end |
| Tileset & Brush Manager, advanced brush editor | Tools; duże osobne okno, lista po lewej, sloty i podgląd pośrodku, parametry po prawej |
| Cleanup, Map Analyzer, właściwości mapy | Map; raport analizy na dole, szczegóły i akcje prowadzą do pozycji |
| Work timer, zadania, historia czasu | Pasek stanu i dolny panel Work timer |
| Preferences, themes, renderer/FPS/VSync | File → Preferences; kategorie General, Appearance, Editing, Rendering, Files, Updates; oznaczać opcje wymagające restartu |
| About, skróty i pomoc | Help i wyszukiwarka poleceń |

Structure Manager pozostaje usunięty. Prefaby są częścią Doodads.

## Fluent i wykonalność w Qt

Punktem odniesienia jest https://doc.qt.io/qt-6/qtquickcontrols-fluentwinui3.html.
FluentWinUI3 dostępny od Qt 6.8; projekt używa Qt 6.10.2, więc należy testować na tej konkretnej wersji. Nie jest to natywny WinUI ani automatyczne Mica. Część kontrolek nie ma implementacji Fluent i korzysta z Fusion; splitter, drzewa i układ dokowania wymagają świadomego dopracowania. Unikać nadpisywania wnętrza standardowych kontrolek tam, gdzie wystarczy palette.

- Segoe UI, tekst 13–14 px, nagłówki paneli 14 px semibold; siatka odstępów 4/8/12/16 px.
- Powierzchnie #202020/#272727, kontrolki #303030, tekst #F3F3F3, drugorzędny #BEBEBE, akcent #60CDFF. Obraz jest kierunkiem wizualnym; docelowo respektować paletę systemową.
- Rogi kontrolek 4 px, paneli maks. 8 px, subtelne obramowania, bez dużych cieni i ozdobnych kart.
- Focus klawiatury, tooltips ze skrótami, tekstowe etykiety stanów; nie opierać informacji wyłącznie na kolorze.
- Przy 1366×768 zwijać prawy panel i etykiety nawigacji, zachować mapę min. ok. 700 px szerokości. Na dużym ekranie można rozwinąć oba boki.
- Ekran startowy: Recent maps + New/Open, status profilu klienta, recovery i updates. Ustawienia profilu nie powinny stale zajmować miejsca na mapie.
- Aktualny skrypt DeployQtRuntime usuwa FluentWinUI3. Przy wdrożeniu trzeba zmienić deployment i routing Dme*; sama zmiana stylu Qt nie przeprojektuje obecnego UI.

## Kryteria wdrożenia

Pełne menu i skróty zachowane; generator działa przy otwartym panelu; brak dodatkowego wymogu zaznaczenia dla ground prefabów; procenty i miniatury czytelne; panele pamiętają szerokości; testy 100/125/150% DPI, mały ekran, duża mapa, light/dark i pełna obsługa klawiaturą. Makieta nie potwierdza działania funkcji ani poprawności wygenerowanych sprite'ów.
