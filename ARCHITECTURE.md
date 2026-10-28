# Architektura DewralMapEditor

## Status i źródła prawdy

Zaktualizowano 2026-10-28 na bieżącym drzewie roboczym, obejmującym także lokalne, niezatwierdzone zmiany. Dokument opisuje źródła, nie gwarantuje identycznego stanu na GitHubie. Fakty mają wskazane pliki; zalecenia i ograniczenia oznaczono osobno. Pierwotny dokument powstał podczas konfiguracji Codex. Późniejsze wdrożenie startu opisuje [raport implementacji](docs/architecture/startup-implementation-2026-10-28.md).

## Technologia i zależności — fakty

| Obszar | Stan w repozytorium | Źródło |
|---|---|---|
| Język | `CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_STANDARD_REQUIRED ON`; projekt używa także C dla bibliotek | [CMakeLists.txt](CMakeLists.txt) |
| Build | CMake >= 3.24; AUTOMOC, AUTORCC, AUTOUIC | [CMakeLists.txt](CMakeLists.txt) |
| Qt | Minimum 6.10; Core, Gui, Qml, Quick, ShaderTools, Network, Widgets, Concurrent, Sql; wymagany cel GuiPrivate; QuickTest opcjonalny dla testów | [CMakeLists.txt](CMakeLists.txt) |
| QML | Moduł `Tibia 1.0`, rejestracja typów C++; jawny manifest `QML_FILES` i AOT; grafiki w QRC | [CMakeLists.txt](CMakeLists.txt), [editor/resources.qrc](editor/resources.qrc) |
| Rendering | `QQuickRhiItem` i QRhi, nie bezpośredni renderer OpenGL | [maprhiview.h](editor/map/maprhiview.h), [maprhibackend.h](editor/map/maprhibackend.h) |
| Windows | Domyślnie `QSG_RHI_BACKEND=opengl`, jeśli nie ustawiono jawnie tej zmiennej | [editor/main.cpp](editor/main.cpp) |
| Shadery | Qt Shader Tools: GLSL `300es,330`, HLSL 50, MSL 12 | [CMakeLists.txt](CMakeLists.txt), [map.vert](editor/map/shaders/map.vert), [map.frag](editor/map/shaders/map.frag) |
| Biblioteki lokalne | Statyczne `OTFormats`, `DmeLua`, `DmeLuaScripts`, `DmeLzma`; wbudowana Lua 5.1 | [CMakeLists.txt](CMakeLists.txt), katalog `libs/` |
| vcpkg | Manifest z przypiętym baseline; qtbase z OpenGL/SQL/SQLite, qtdeclarative, qtshadertools, qtsvg | [vcpkg.json](vcpkg.json) |
| CI wydania | Windows 2022, MSVC 2022 x64, Qt 6.10.2; push na `main` buduje, testuje i publikuje wydanie | [.github/workflows/release.yml](.github/workflows/release.yml) |

**C++20 — zalecenie, nie wdrożony standard:** migracja wymaga osobnego zadania obejmującego CMake, wspierane kompilatory, zależności i testy. Samo dopisanie flagi do przykładu polecenia nie migruje projektu; źródłowy `set(CMAKE_CXX_STANDARD 17)` pozostaje wiążący.

**Rozbieżność dokumentacji:** [docs/qrhi-rendering.md](docs/qrhi-rendering.md) opisuje domyślne D3D11 na Windows. Aktualne `editor/main.cpp` wybiera OpenGL, a jawny `QSG_RHI_BACKEND` ma pierwszeństwo. Wybór backendu następuje przed utworzeniem aplikacji i wymaga nowego procesu. Zmiana VSync również dotyczy startu procesu.

## Mapa modułów — fakty

| Ścieżka | Odpowiedzialność |
|---|---|
| `editor/main.cpp` | Konfiguracja procesu, grafiki i Qt Quick Controls Basic; utworzenie `Backend`, silnika QML i image providerów; ładowanie `qrc:/qml/Main.qml`; smoke i profilowanie. |
| `editor/core/` | Usługi aplikacji: `Backend`, `DocumentManager`, palety i filtry, brush/creature stores, hotkeys, generatory, timery, aktualizacje, activity. |
| `libs/otformats/` | Odczyt/zapis OTBM i danych DAT/SPR/OTB/OTFI/XML, struktury i indeksy danych. Statyczny target `OTFormats` jest używany przez aplikację i testy. |
| `editor/map/mapview*.cpp`, `mapview.h` | `MapView`: fasada stanu mapy, nawigacja, edycja, interakcja i przygotowanie danych renderowania. Implementacja jest rozbita tematycznie. |
| `editor/map/mapservices.h`, `mapservices.cpp`, `map*controller.h` | Istniejące usługi atlasu, chunków, edycji, pędzli, selekcji, nawigacji, przedmiotów, minimapy i spawnów. |
| `editor/map/maprhiview.*` | Most `MapView`–render thread, widoczne chunki, cache sceny, drawy i osobne nakładki. |
| `editor/map/maprhibackend.*` | Własność GPU, pipeline'y, uploady, uniformy, samplery, render target cache i rejestrowanie drawów QRhi. |
| `editor/preview/` | Oddzielna scena i sterowanie In-game Preview oraz pogoda; `IngamePreviewView` korzysta z `MapRhiBackend`. Jego aktualizacje nie są tożsame z cache głównego viewportu. |
| `editor/qml/` | `Main.qml`, `StartupWindow.qml`, `PalettePanel.qml`; `components/`, `controllers/`, `dialogs/`, `style/`, `themes/`. Komponenty `Dme*` i warstwy motywów współdzielą wygląd. |
| `editor/updater/`, `cmake/` | Oddzielny Windows updater i deployment/pakowanie. |
| `tests/` | Natywne programy testowe, Qt Quick tests, fixture i testy skryptów. |
| `activity/` | Osobny panel web/API; nie jest viewportem Qt. Szczegóły: [activity/README.md](activity/README.md). |
| `data/`, `tools/`, `scripts/` | Profile i materiały edytora, import i narzędzia pomocnicze, m.in. profilowanie ładowania. Prywatne zasoby klienta nie są częścią źródeł. |

## C++–QML i czas życia

1. `main.cpp` tworzy `QGuiApplication`, stosowy `Backend` i `QQmlApplicationEngine`; rejestruje image providery `tibiaui` i `paletteitem`.
2. `qt_add_qml_module(... URI Tibia VERSION 1.0 SOURCES ...)` rejestruje typy z `QML_NAMED_ELEMENT`; `Backend` ma także `QML_SINGLETON`. [cmake/QmlFiles.cmake](cmake/QmlFiles.cmake) zawiera jawny manifest QML/JS modułu. Aliasy `qml/...`, `RESOURCE_PREFIX /` i `NO_RESOURCE_TARGET_PATH` zachowują adresy `qrc:/qml/...`; `editor/resources.qrc` pakuje grafiki. Nie dubluj QML w obu zasobach.
3. [backend.h](editor/core/backend.h) udostępnia czytniki i usługi przez `Q_PROPERTY`. [backend.cpp](editor/core/backend.cpp) zwraca istniejący singleton z `create()`, ustawia `QQmlEngine::CppOwnership` i sprawdza zgodność wątku silnika. Wiele usług to pola składowe, nie obiekty do niezależnego usuwania przez QML.
4. [MapWorkspace.qml](editor/qml/components/MapWorkspace.qml) zestawia `MapView`, `MapRhiView`, interakcję i nakładki. [AppSettings.qml](editor/qml/controllers/AppSettings.qml) i [MapViewSettings.qml](editor/qml/controllers/MapViewSettings.qml) integrują preferencje. Zmiana bindingu/sygnału może dotyczyć menu, toolbaru, skrótów i Preview równocześnie.
5. Ładowanie mapy i atlasu wykorzystuje zadania w tle, generacje/anulowanie i odroczoną publikację. Sprawdzaj czas życia dokumentu oraz kontekst Qt w `mapview.cpp` i `mapview_atlas.cpp` przed modyfikacją tej granicy.

Zaobserwowane konwencje: zwykle 4 spacje, klasy UpperCamelCase, metody lowerCamelCase i pola `m_`. Nagłówki mają include guards lub `#pragma once`; część funkcji jest kompaktowa. QML używa również `required property var`, aliasów, bindingów i `objectName` w testach. To obserwacja, nie nakaz masowego przepisywania. W przeglądanych plikach konfiguracji nie stwierdzono nadrzędnego `.clang-format` ani `.editorconfig`.

## Start, przygotowanie klienta i narzędzia — wdrożone

- `StartupWindow` tworzy karty recovery/postępu tylko wtedy, gdy są potrzebne. Dialogi plików, katalogu i nowej mapy są odraczane przez URL i zachowywane po pierwszym użyciu. Dalszy pomiar: [raport odchudzenia](docs/architecture/startup-followup-2026-10-28.md).
- [Main.qml](editor/qml/Main.qml) tworzy ustawienia, `ApplicationSession` i ekran powitalny. `EditorScene.qml` jest adresem URL nieaktywnego Loader; nie jest zależnością typu ekranu startowego. Edytor powstaje dopiero na żądanie open/new/recovery i pozostaje do końca procesu.
- [ApplicationSession.qml](editor/qml/controllers/ApplicationSession.qml) przechowuje żądanie, recent files i jeden katalog profili. `ClientProfileController` nie wymaga viewportu. Pierwsze przygotowanie klienta i tworzenie edytora mogą działać równolegle; przekazanie operacji dokumentowi czeka na oba. Gotowość Loader, klienta i pierwszej kompletnej klatki są oddzielne.
- [ClientLoadService](editor/core/clientloadservice.h) jest dzieckiem `Backend`. Worker ma własne lokalne czytniki i wynik wartościowy; przygotowuje definicje, indeksy, miniatury i palety. Żądania mają generacje, anulowanie i fingerprint. GUI publikuje spójny wynik we **wcześniej istniejących** modelach. `SprReader` otwiera własny plik na GUI; nie przejmuje mapowania workera. Przy zajętym mutexie viewportu publikacja jest ponawiana przez timer bez blokującego oczekiwania.
- [DocumentController.qml](editor/qml/controllers/DocumentController.qml) i `ClientProfileController` używają kontynuacji asynchronicznych dla open/new/profile/tab/recovery/save. Dawne `ensureClient*` są tylko zapytaniami zgodności; nie wykonują ciężkiego odczytu. Parser mapy i worker atlasu pozostały istniejącymi podsystemami.
- [LazyToolHost.qml](editor/qml/components/LazyToolHost.qml) tworzy narzędzia przez URL i przekazuje właściwości przed konstrukcją. Konfiguracje 30 narzędzi są w `editor/qml/tools/*Tool.qml`. Powtórne wywołanie nie tworzy drugiej instancji. Polityka zamykania pozostaje we wrapperach; aktywna sesja Generation Library zachowuje stan.
- Trzy `WorkTimerStatus` otwierają jeden [WorkTimerPopupHost](editor/qml/components/WorkTimerPopupHost.qml). Popup powstaje przy pierwszym użyciu i zmienia wizualnego rodzica na okno wskaźnika, również osobne Preview. Historia/Activity są tworzone po wejściu na stronę, wybór ikon i eksporty przez URL przy pierwszym użyciu.
- [WorkTimerService](editor/core/worktimerservice.h) odczytuje JSON po `ensureLoaded()`, w tle; `WorkTimerHistoryModel` udostępnia wiersze na żądanie. Agregaty historyczne są cache’owane do zmiany danych. Przed gotowością mutacje historii są blokowane, a destruktor nie zapisuje pustego stanu. Nie zmieniono formatu JSON ani eksportów.

Pomiary i granice weryfikacji są w [raporcie wdrożenia](docs/architecture/startup-implementation-2026-10-28.md); nie traktuj samego `Loader.Ready` ani testów na atrapach jako dowodu pełnej klatki QRhi.

## Zapis mapy i potwierdzenie klatki

`DocumentController.queueSave` pokazuje modalny komunikat i czeka na token `DmeWindow.requestPresentedFrame()`. Token jest przechwytywany podczas synchronizacji sceny i potwierdzany po przekazaniu klatki do prezentacji; callback wraca na GUI. Dopiero wtedy wywoływany jest istniejący synchroniczny `OtbmReader.saveFile`. Nie jest to asynchroniczna serializacja. Kontrakt obejmuje Save, Save As i Save All; opis i ograniczenia: [komunikat zapisu](docs/architecture/save-feedback-2026-10-28.md).

## Przepływ renderowania — fakty

```text
OtbmReader + DatReader/OtbReader/SprReader
                 |
              MapView
    indeksy / MapChunkStore / MapAtlasService
                 |
  MapRhiRenderer::synchronize() [render thread, GUI zatrzymane przez Qt]
                 |
    instancje + wersje + staging CPU + listy drawów
                 |
  MapRhiRenderer::render() -> MapRhiBackend
                 |
  QRhiResourceUpdateBatch -> pass cache -> pass wyjściowy -> Qt Quick
```

- [mapview_chunks.cpp](editor/map/mapview_chunks.cpp) buduje indeks pięter/chunków; kafelki sortuje według `y`, następnie `x`. Quady uwzględniają stos przedmiotów, elevation, wzorce i warstwy sprite'ów. [maprhiview.cpp](editor/map/maprhiview.cpp) używa chunków 32 × 32 kafelki i zachowuje kolejność drawów.
- [mapchunkstore.h](editor/map/mapchunkstore.h) zawiera wersje, dirty set, cache współdzielonych niemutowalnych list quadów i worker `std::thread` z kolejką, mutexami oraz generacją żądań. Obsługa workera jest w `mapservices.cpp`.
- [mapview_atlas.cpp](editor/map/mapview_atlas.cpp) przygotowuje atlas w tle; zmiana klienta/generacji unieważnia poprzedni wynik. [MapAtlasService](editor/map/mapatlasservice.h) udostępnia `uploadSince(epoch, spriteCount)` i autorytatywny obraz CPU dla niezależnych odbiorców.
- `synchronize()` odczytuje stan GUI w granicy synchronizacji Qt i przygotowuje dane renderera. Wersje ograniczają przebudowy chunków; metadata i pointer overlays mają własne unieważnianie. Nie przenoś dowolnych odczytów `MapView` do `render()` ani workera.
- [MapRenderBuffer](editor/map/maprhibackend.h) zachowuje staging `QByteArray`, `dirty` i `unique_ptr<QRhiBuffer>`. `uploadBuffer()` tworzy/powiększa bufor po przekroczeniu pojemności i wysyła dane dla `dirty`.
- [MapRhiBackend::render()](editor/map/maprhibackend.cpp) przygotowuje uploady przed passami. Uniformy mają wyrównany stride i dynamiczne offsety. Tablica CPU `uniformData` powstaje dla danego wywołania: to kandydat do pomiaru, nie potwierdzony bottleneck.
- `record()` wiąże pipeline/zasoby/bufory i wykonuje instanced `draw(6, count)` dla sprite'ów; linie mają osobny tryb. To ani draw na każdy kafelek, ani jeden draw na całą mapę. Koszt zależy od chunków, pięter, cache i nakładek.
- Cache głównej sceny ma margines 256 jednostek widoku, skalowany do pikseli urządzenia. Poprawny cache pozwala wykonać blit i osobno wskaźniki bez ponownego rysowania całej sceny. Klucze cache i zmiany atlasu są częścią poprawności.
- Backend posiada przez `unique_ptr` bufory, tekstury, samplery, bindingi, pipeline'y i render targety. Zmiana QRhi wymaga odtworzenia GPU i uploadu. Widoki mają własne zasoby GPU; osobne okno nie otrzymuje natywnego uchwytu atlasu z głównego okna.
- [IngamePreviewView](editor/preview/ingamepreviewview.cpp) ma własny renderer i staging, oparte na tym samym backendzie. Pogoda i gracz mają dodatkowy stan/timery. Pomiar głównego viewportu nie dowodzi wydajności Preview.

GuiPrivate/QRhi wymagają zgodnego SDK i wdrażanego Qt. Wariant shaderów nie jest dowodem testowania Vulkan, Metal ani urządzeń mobilnych.

## Build i testy — zweryfikowane punkty wejścia

### Presety repozytorium

[CMakePresets.json](CMakePresets.json) definiuje `windows-vcpkg`: Visual Studio 17 2022, x64, `$env{VCPKG_ROOT}`, triplet `x64-windows-release`, katalog `build/vcpkg-windows`. Build preset `windows-release` wybiera `Release` i target `package_all`. Nie ma test presetów.

```powershell
cmake --list-presets
cmake --preset windows-vcpkg
cmake --build build/vcpkg-windows --config Release --target DewralMapEditor --parallel
```

To istniejąca ścieżka konfiguracji, nie polecenia wykonane podczas pisania dokumentu. Wymaga przygotowanego MSVC/vcpkg i Qt. `cmake --build --preset windows-release` uruchamia **pakowanie**, nie sam build. Nie wykonuj go dla każdej poprawki.

Dla istniejącego builda odczytaj `CMakeCache.txt`. Nie mieszaj MSVC i MinGW w jednym cache. `DME_RUNTIME_OUTPUT_DIRECTORY` domyślnie wskazuje `dist`, ale lokalny cache może to zmienić. Target to `DewralMapEditor`, program to `DME`/`DME.exe`. Build kopiuje także dane i wdraża Qt. Nie podmieniaj EXE innej instalacji ani nie kończ procesu z niezapisanymi mapami automatycznie.

### Mapa testów w CMake

| Obszar | Targety do zbudowania | Nazwy CTest |
|---|---|---|
| Startup | `DewralMapEditor` | `dme_startup` (`--smoke-test`, software Qt Quick) |
| GPU | `map_rhi_backend_test` | `map_rhi_opengl`, na Windows `map_rhi_d3d11` |
| Atlas | `atlas_background_build_test` | `atlas_background_build` |
| Interakcja/Preview | `map_creature_refresh_test` | `map_creature_refresh`, `map_view_position`, `map_item_locations` |
| Pogoda | `weather_particles_test`, `weather_preview_render_test` | `weather_particles`, `weather_preview_render` |
| Motyw QML | `tibia_ui_qml_test` (gdy dostępny QuickTest) | `tibia_ui_qml` |
| Formaty | `otbm_rme_compatibility_test`, `otbm_legacy_subtypes_test`, `otbm_numbered_zones_test` | `otbm_rme_compatibility`, `otbm_legacy_subtypes`, `otbm_numbered_zones` |
| Ładowanie | `otbm_background_load_test` | `otbm_background_load`; `profile_map_loading_script`, gdy znaleziono PowerShell |
| Generacja | `map_generation_test` | `map_generation` |

Sprawdź `BUILD_TESTING`, warunki platformy i `ctest --test-dir <build-dir> -N`. `<build-dir>` oznacza wybrany, istniejący katalog konfiguracji. Większość testów ma `EXCLUDE_FROM_ALL`: sam build aplikacji ich nie aktualizuje. Lista CTest nie dowodzi istnienia zbudowanych programów. Dla generatora wielokonfiguracyjnego dodaj `-C Release` w CTest i `--config Release` w buildzie.

`tibia_ui_qml` uruchamia tylko `tests/qml/tst_tibia_theme.qml`. Runner `tibia_ui_qml_test` przyjmuje `-input` i `-import`, pozwalając wybrać inne `tests/qml/tst_*.qml`. [Mocki Tibia](tests/qml/imports/Tibia/qmldir) nie dowodzą poprawności rzeczywistych singletonów, shaderów ani GPU. Część testów generatorów rejestrowana jest dopiero ze wskazanym zewnętrznym fixture; brak testu nie oznacza zaliczenia.

## Profilowanie — ograniczenia i zalecenia

Istnieją [LoadProfile](libs/otformats/loadprofile.h), tryby `editor/main.cpp`, [scripts/profile-map-loading.ps1](scripts/profile-map-loading.ps1) i [docs/map-loading-performance.md](docs/map-loading-performance.md). Parametry skryptu: `DmePath`, `MapPath`, `ClientFolder`, `ClientVersion`, `Runs`, opcjonalnie `Warm`, `OutputDirectory`. To pomiar ładowania/pierwszej kompletnej klatki, nie gotowy benchmark ciągłego przesuwania mapy. Historyczne wyniki nie są aktualnym baseline'em.

**Zalecenie dla optymalizacji:** porównuj tę samą mapę i zasoby, backend, okno/DPI, zoom, piętro, światło/animacje, trasę kamery, build i GPU. Oddziel start, rozgrzewkę, statyczną scenę, pan/zoom oraz edycję. Mierz powtarzane czasy klatek i ich rozkład, draw calle, uploadowane bajty i alokacje stosownie do hipotezy. Brak sceny lub pomiarów GPU ujawnij; nie zastępuj ich FPS ani testem software.

## Konfiguracja Codex i inspiracje

[AGENTS.md](AGENTS.md) zawiera stałe reguły; pięć `.agents/skills/*/SKILL.md` osobne workflow. Nie dodano zależności ani globalnego `config.toml`. Repozytoryjne `.agents/skills/` i frontmatter `name`/`description` opisuje [oficjalna dokumentacja skills](https://learn.chatgpt.com/docs/build-skills), a instrukcje [dokumentacja AGENTS.md](https://learn.chatgpt.com/docs/agent-configuration/agents-md).

Inspiracje przystosowano, nie skopiowano:
- [Ava](https://github.com/yappologistic/Ava/blob/main/AGENTS.md): krótka polityka i dowody wydajności; bez założeń o Liquid Glass/D3D11.
- [Cavewhere](https://github.com/Cavewhere/cavewhere/blob/master/AGENTS.md): mapa modułów, Qt/QML i testy; bez przejmowania Conan, ich ścieżek i poleceń.
- [Mindrally](https://github.com/Mindrally/skills/blob/main/cpp/SKILL.md): RAII i jawna własność; bez narzucania C++20, wyjątków, `std::async` lub Google Test.
- [cpp-perf-skills](https://github.com/rollysys/cpp-perf-skills): hipoteza, pomiar i kontrola regresji; bez instalowania profilerów, przedstawiania szacunków cykli jako faktów, SSH i arbitralnego wymogu procentowego przyspieszenia.
