# Qt 6 / QML — kontrakty, wydajność i wątki

## Zasada audytu
Nie zmieniono wyglądu ani UX. Ocena dotyczy granic stanu, sygnałów, modeli i kosztów wykonywanych na GUI. `QML_NAMED_ELEMENT` i `QML_SINGLETON` są rejestrowane w module `Tibia 1.0`; aplikacja nie opiera głównego API na swobodnie dodanych context properties.

### Backend, modele i bindingi

| Pole | Ustalenie |
|---|---|
| Odpowiedzialność | Stabilny most prezentacji do dokumentów, zasobów i usług. |
| Źródła / wejście | [editor/core/backend.h:36](../../editor/core/backend.h#L36); [editor/core/backend.cpp:33](../../editor/core/backend.cpp#L33); [editor/qml/controllers/MapViewSettings.qml:1](../../editor/qml/controllers/MapViewSettings.qml#L1) |
| Zależności | Qt meta-object/QML, QAbstractItemModel, AppSettings i Dme controls. |
| Interfejsy | Q_PROPERTY, NOTIFY, Q_INVOKABLE, modele OTB/DAT/SPR/creature/filter. |
| Przepływ runtime | C++ publish state → sygnał/model notification → binding/delegat; user action → controller → C++ API. |
| Własność zasobów | Backend na stosie, CppOwnership; modele jako owned members, silnik posiada QML. Testowy Backend mock ma inne lifetime. |
| Inicjalizacja / destrukcja | Engine powstaje po Backend i jest niszczony przed nim; modele resetują się przy przeładowaniu klienta. |
| Operacje kosztowne | Reset modeli, filtr wszystkich rows, tworzenie delegatów, wariantowe listy metadata overlays. |
| Słabości / pewność | Nie dowiedziono błędu nadmiernych resetów dla każdego modelu. Reset całego klienta jest uzasadniony; nie przenosić go mechanicznie na incremental rows. |
| Możliwa poprawa | Zmierz notifications/delegate churn; incremental update tam, gdzie naprawdę zmienia się część modelu, przy zachowaniu role/selection contracts. |

### Profile klienta i dokumentowe operacje GUI

| Pole | Ustalenie |
|---|---|
| Odpowiedzialność | Orkiestracja otwierania, wyboru zasobów i zapisywania bez zmiany kontrolek. |
| Źródła / wejście | [editor/qml/controllers/ClientProfileController.qml:212](../../editor/qml/controllers/ClientProfileController.qml#L212); [editor/qml/controllers/DocumentController.qml:266](../../editor/qml/controllers/DocumentController.qml#L266) |
| Zależności | Backend readers, document manager, settings, MapView, filesystem. |
| Interfejsy | ensureClientVersion/ensureClientLoaded, load/save/close flows, profile fingerprint. |
| Przepływ runtime | Akcja QML → synchroniczny C++ reader/save lub start async load → queued ready → binding. |
| Własność zasobów | QML przechowuje klucz profilu; dane i zasoby są w C++. |
| Inicjalizacja / destrukcja | Fingerprint chroni przed ponownym ładowaniem niezmienionych zasobów; fail resetuje loaded profile state. |
| Operacje kosztowne | DAT/SPR/OTB load i preloaded palette compositions; normalny save i recovery capture. |
| Słabości / pewność | F03/F04: widoczne zamrożenie jest możliwe mimo background OTBM load; nie wszystkie kroki startup zostały przeniesione na worker. |
| Możliwa poprawa | Detached load bundle/save snapshot z publication epoch i niezmienionym stanem UI; zachować poprzedni profil w razie błędu. |

### Viewport, overlay, Preview i style

| Pole | Ustalenie |
|---|---|
| Odpowiedzialność | Prezentacja mapy, narzędzi, etykiet i okien; zachowanie motywów. |
| Źródła / wejście | [editor/qml/components/MapWorkspace.qml:1](../../editor/qml/components/MapWorkspace.qml#L1); [editor/qml/components/MapOverlay.qml:1](../../editor/qml/components/MapOverlay.qml#L1); [editor/qml/components/IngamePreviewPanel.qml:1](../../editor/qml/components/IngamePreviewPanel.qml#L1) |
| Zależności | MapView/MapRhiView, PreviewView/controller, settings, Dme style/themes. |
| Interfejsy | Required properties, aliases, signal handlers, `mapOverlayData`/label data, dock/float APIs. |
| Przepływ runtime | Zmiana kamery/treści → odświeżenie odpowiedniego overlay/renderer; UI nie serializuje mapy w bindingu. |
| Własność zasobów | QML items należą do engine/visual parents; osobne okna mają osobny scene graph i zasoby GPU. |
| Inicjalizacja / destrukcja | Loader/deferred components redukują nieaktywny UI; dock/reparent odtwarza scene graph. |
| Operacje kosztowne | Budowanie QVariant overlay/labels i delegatów, zależności property bindings, aktywne timery Preview. |
| Słabości / pewność | Brak pełnego QML profiler trace: nie stwierdzono jednego dominującego bindingu. Nie wolno traktować samego rozmiaru pliku Main.qml jako przyczyny lagów. |
| Możliwa poprawa | Instrumentować invalidation i deklaratywne zależności; ograniczać pracę nieaktywnych widoków i aktualizować tylko zmienione label data, bez redesignu. |

## F02 — deklaracja minimalnego Qt nie odpowiada API

CMake dopuszcza Qt **6.7**, ale PaletteFilter i CreatureFilter używają `beginFilterChange()` oraz `endFilterChange(Direction::Rows)` bez guardu wersji. Qt dokumentuje begin od 6.9, Direction/end od **6.10**. Konfiguracja z 6.7/6.8 nie zapewnia więc kompilacji tej ścieżki; sam udany build na CI 6.10.2 tego nie ujawnia.

Dowody: [CMakeLists.txt:19](../../CMakeLists.txt#L19); [editor/core/palettefilter.cpp:24](../../editor/core/palettefilter.cpp#L24); [editor/core/creaturefilter.cpp:15](../../editor/core/creaturefilter.cpp#L15); [oficjalna dokumentacja QSortFilterProxyModel](https://doc.qt.io/qt-6/qsortfilterproxymodel.html#endFilterChange).

P2: wybrać jawnie Qt>=6.10 albo kompatybilną ścieżkę dla deklarowanego minimum. Nie wprowadzono żadnej z tych zmian podczas audytu. Nie wykonano builda starszym SDK; wniosek pochodzi z użytych sygnatur i ich wersji API.

## Koszty i sygnały — rozróżnienie

- Model setters sprawdzają zmianę wartości przed filtrowaniem; nie zakładamy, że każdy binding wywołuje pełny reset. PaletteFilter ma konkretne range/prefix/ID filtry i batch begin/endFilterChange.
- `OtbmReader::mapChanged` jest szerokim sygnałem danych/historii; zwykłe grouped edits tłumią część emisji. Jeden nowy coarse signal nie powinien zastępować precyzyjnych chunk/metadata versions.
- Query result/progress są publikowane queued. Fakt publikacji w GUI nie rozwiązuje problemu read lifetime na workerze; F10 opisano w [memory-analysis.md](memory-analysis.md).
- Asynchroniczny PaletteImageProvider korzysta z opublikowanych miniaturek i blokady czytelniczej. Nie wolno przenieść na niego mutacji QObject/SprReader modelu.
- Save, bulk selection, map analysis, asset initialization i generation apply mają różne koszty oraz modele zadań. Nie należy odpowiadać na każdy problem przez nowy QThread.

Źródła: [editor/core/palettefilter.cpp:30](../../editor/core/palettefilter.cpp#L30); [libs/otformats/otbmreader.cpp:2883](../../libs/otformats/otbmreader.cpp#L2883); [editor/core/paletteimageprovider.cpp:7](../../editor/core/paletteimageprovider.cpp#L7); [editor/map/mapview_query.cpp:624](../../editor/map/mapview_query.cpp#L624).

## Walidacja bez zmiany UX

Istnieją testy Qt Quick w `tests/qml`, importy/mocki `tests/qml/imports/Tibia` i runner `tibia_ui_qml_test`. Rejestrowany CTest `tibia_ui_qml` sprawdza konkretnie motyw, nie cały katalog. Mocki nie pokrywają native worker lifetime, parsowania ani QRhi. GPU test z QQuickRenderControl sprawdza także kompozycję, ale nie wszystkie realne okna/docking.

Dla zmian należy zachować: focus i klawiaturę/Escape, gest release/ungrab, current document, checked/enabled, wszystkie motywy, resize/DPI, dock/float oraz rozdzielenie ustawień Preview/editor. W tym audycie nie wykonywano pełnego regresyjnego przejścia wizualnego ani profilu wszystkich bindings. Nie ma podstaw do deklarowania poprawności każdego UI flow.

Źródła: [CMakeLists.txt:331](../../CMakeLists.txt#L331); [tests/qml/imports/Tibia/qmldir:1](../../tests/qml/imports/Tibia/qmldir#L1); [tests/map_rhi_backend_test.cpp:56](../../tests/map_rhi_backend_test.cpp#L56).
