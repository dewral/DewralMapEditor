# Plan migracji i rejestr ustaleń

Stan bazowy planu pochodzi z audytu. W serii 2026-10-27 wdrożono poprawki F01, F02 i bezpieczeństwa F10, opisane w [raporcie napraw](fixes-2026-10-27.md). Pozostałe propozycje nie zostały wdrożone. Zachować QRhi, wygląd UI, obsługiwane formaty, kolejność rysowania i wszystkie narzędzia. Wyniki pomiarów oraz ich ograniczenia: [performance-benchmarks.md](performance-benchmarks.md).

## Rejestr ustaleń

P1 oznacza pierwszeństwo ze względu na możliwe naruszenie bezpieczeństwa dostępu do danych; P2 oznacza istotną poprawność lub responsywność. Priorytet nie jest dowodem częstości występowania.

| ID | Priorytet / pewność | Ustalenie i dowód |
|---|---|---|
| F10 | P1 / analiza źródeł, brak reprodukcji wyścigu | Zapytanie w tle nie ma trwałego kontekstu dokumentu; setOtbm zmienia reader poza blokadą używaną przez query. [editor/map/mapview_query.cpp:624](../../editor/map/mapview_query.cpp#L624); [editor/map/mapview.cpp:423](../../editor/map/mapview.cpp#L423) |
| F01 | P2 / odtworzono | placeItem przyjmuje x=65536, tworzy tile z x=0 i niespójny lookup. [libs/otformats/otbmreader.cpp:1342](../../libs/otformats/otbmreader.cpp#L1342); [wynik](benchmarks/bounds-repro.txt) |
| F02 | P2 / potwierdzone API, brak builda starszym SDK | CMake deklaruje Qt 6.7, ale endFilterChange(Direction::Rows) wymaga 6.10. [CMakeLists.txt:19](../../CMakeLists.txt#L19); [editor/core/palettefilter.cpp:24](../../editor/core/palettefilter.cpp#L24); [Qt](https://doc.qt.io/qt-6/qsortfilterproxymodel.html#endFilterChange) |
| F03 | P2 / mechanizm i pomiar syntetyczny | Normalny zapis jest synchroniczny; mediana nowego pliku miliona prostych tile: 61.196 ms. Źródła i kontrakt zapisu: [analiza danych](map-data-analysis.md). |
| F04 | P2 / mechanizm i pomiar syntetyczny | Recovery capture na GUI: 28.915 ms dla miliona prostych tile. [Własność i źródła](memory-analysis.md). |
| F05 | Do pomiaru contention / analiza źródeł | Pełny skan query trzyma data mutex; może opóźniać GUI oczekujące na tę samą blokadę. Brak lock trace. [Analiza](memory-analysis.md). |
| F06 | Do pomiaru UI / analiza złożoności | Prostokątne zaznaczenie iteruje współrzędne obszaru, również puste. [Źródła](editor-tools-audit.md). |
| F07 | Hipoteza optymalizacji | Scratch vectors i uniform staging mogą korzystać z zachowanej pojemności. Nie wykazano zysku w produkcyjnym frame. [Renderer](qrhi-rendering-audit.md). |
| F08 | Mechanizm potwierdzony, częstotliwość niezmierzona | Preview publikuje pełny atlas po zmianie rewizji; brak pomiaru churn podczas chodzenia. [Renderer](qrhi-rendering-audit.md). |
| F09 | Ryzyko kontraktu API | Ręczne begin/end i bool grupowania historii nie zapewniają ogólnej semantyki zagnieżdżonych transakcji. Nie odtworzono błędu konkretnej sekwencji UI ani OOM. [Historia](editor-tools-audit.md). |

## Kolejne etapy

Każdy etap jest osobnym zestawem małych commitów. Najpierw dopisać test odtwarzający problem, następnie minimalną poprawkę i wykonać walidację. Nie łączyć zmian storage, historii i renderera w jednej migracji.

### 1. Kontekst i lifetime zapytań — F10/F05

| Pole | Plan |
|---|---|
| Obecnie | Worker skanuje żywy MapView/reader; callback publikuje wynik. |
| Słabość | Zmiana dokumentu nie ma wspólnego protokołu synchronizacji z query. |
| Zmiana | Najpierw deterministyczny test przełączenia i zamknięcia. Wprowadzić kontekst zadania: stabilna własność danych wejściowych, generation dokumentu, anulowanie i walidacja publikacji na GUI. Sam QPointer lub sam numer generation nie zabezpiecza dereferencji readera. |
| Oczekiwana korzyść | Usunięcie niesynchronizowanej granicy i wyników z poprzedniej mapy; krótsze locki dopiero po pomiarze. |
| Złożoność | Średnia–wysoka; snapshot całej mapy może kosztować i blokować. |
| Ryzyko regresji | Zmiana semantyki cancel, dodatkowe kopie, shutdown deadlock przy bezwarunkowym wait. |
| Zależności | DocumentManager, MapView query, kontrakt readera. |
| Wymagane testy | Wstrzymany worker + switch/close/reload, późny callback, shutdown, poprawność wyników, ograniczenie pamięci; sanitizer na wspieranej konfiguracji. |
| Rollback | Oddzielny commit; do czasu poprawki ograniczyć operacje konfliktujące zamiast przywracać niebezpieczny dostęp. Nie uznawać guardu publikacji za pełną naprawę lifetime. |

### 2. Walidacja współrzędnych — F01

| Pole | Plan |
|---|---|
| Obecnie | Mutatory przyjmują int i zapisują uint16. |
| Słabość | Górna granica x/y nie jest konsekwentnie odrzucana. |
| Zmiana | Wspólny checked position w istniejących wejściach mutacji przed historią i zmianą tile/indexu; jawnie obsłużyć niepowodzenie insertu. |
| Oczekiwana korzyść | Spójność tile storage i obu indeksów. |
| Złożoność | Mała–średnia, zależnie od liczby wejść. |
| Ryzyko regresji | Odrzucenie dotąd tolerowanych błędnych danych; nie ograniczać świata do deklarowanego rozmiaru mapy bez analizy kompatybilności. |
| Zależności | OTFormats, paste, brushes, generatory. |
| Wymagane testy | -1/0/65535/65536 dla obu osi, piętra -1/0/15/16; brak mutacji i historii po odmowie; undo/redo i roundtrip. |
| Rollback | Osobny commit walidacji z testami; nie zmieniać formatu pliku ani istniejących poprawnych pozycji. |

### 3. Uzgodnienie minimalnego Qt — F02

| Pole | Plan |
|---|---|
| Obecnie | Deklaracja Qt 6.7, kod filtrów używa API 6.10. |
| Słabość | Deklarowane minimum nie odpowiada źródłom. |
| Zmiana | Wybrać świadomie: podnieść minimum do rzeczywiście wspieranego SDK albo dodać warunkowy fallback do dostępnego API starszego Qt. Uzgodnić CMake, CI i instrukcje. |
| Oczekiwana korzyść | Powtarzalny build i uczciwy kontrakt dystrybucji. |
| Złożoność | Mała dla minimum; średnia dla utrzymania fallback. |
| Ryzyko regresji | Utrata wsparcia starszego SDK lub zmiana odświeżania filtrów. |
| Zależności | PaletteFilter, CreatureFilter, CMake, CI i paczki runtime. |
| Wymagane testy | Konfiguracja na zadeklarowanym minimum i aktualnym SDK, filtry w QML, pakowanie DLL. |
| Rollback | Oddzielny commit build/API; zachować poprzedni działający artefakt, nie mieszać bibliotek Qt między wydaniami. |

### 4. Baseline rzeczywistych scen

| Pole | Plan |
|---|---|
| Obecnie | Istnieją testy poprawności i odłączone microbenchmarki audytu. |
| Słabość | Brak end-to-end trace dużej rzeczywistej mapy, CPU/GPU timing, lock contention i VRAM. |
| Zmiana | Użyć jawnie opisanej sceny oraz powtarzalnej ścieżki kamery. Dodać opcjonalne liczniki w istniejących granicach: dirty chunks, draw calls, upload bytes, atlas misses, build time, queue wait. |
| Oczekiwana korzyść | Umożliwienie wyboru optymalizacji na podstawie dominującego kosztu. |
| Złożoność | Średnia. |
| Ryzyko regresji | Koszt instrumentacji może zniekształcić wynik. |
| Zależności | Istniejący renderer, chunk builder i profiler; bez nowego backendu. |
| Wymagane testy | Włączone/wyłączone liczniki; identyczny obraz; warm-up, 3 lub więcej prób, p50/p95/p99, hardware i scena w raporcie. |
| Rollback | Cała diagnostyka opcjonalna; usuwać lub wyłączać niezależnie od funkcji edytora. |

### 5. Responsywność zapisu i recovery — F03/F04

| Pole | Plan |
|---|---|
| Obecnie | Zapis synchroniczny; recovery kopiuje model na GUI, potem zapisuje w tle. |
| Słabość | Zmierzono przekroczenie budżetu klatki przez capture miliona tile; save może blokować. |
| Zmiana | Najpierw rozdzielić snapshot, serializację i commit plików. Prototypować immutable snapshot z wersją dokumentu; ocenić bounded chunk capture bez mieszania wersji. Zachować backup, XML, rollback i późniejsze edycje jako dirty. |
| Oczekiwana korzyść | Mniejsze najdłuższe zatrzymanie GUI, bez zmiany OTBM. |
| Złożoność | Wysoka dla spójnego snapshotu przy edycji; średnia dla osobnej serializacji. |
| Ryzyko regresji | Podwojenie pamięci, zapis niespójnej rewizji, błędny clean state, częściowy commit trzech plików. |
| Zależności | Reader, DocumentManager/Controller, historia, domy/spawny. |
| Wymagane testy | Edycja podczas zapisu, cancel/close/shutdown, ENOSPC, odmowa dostępu, sidecar failure; roundtrip wersji legacy i zones; p95 GUI oraz peak RSS. |
| Rollback | Zachować stary zapis jako drogę powrotu; osobny przełącznik nowej ścieżki do czasu zgodności. Nie zmieniać jednocześnie schematu formatu. |

### 6. Iteracja regionów — F06

| Pole | Plan |
|---|---|
| Obecnie | Rubberband przechodzi wszystkie współrzędne prostokąta. |
| Słabość | Koszt rośnie z powierzchnią, nawet na pustych obszarach. |
| Zmiana | Adapter region query na istniejącym indeksie/chunkach; zachować kolejność wymagającą deterministyczności. Borders potrzebują halo sąsiadów, nie tylko zaznaczonego wnętrza. |
| Oczekiwana korzyść | Potencjalnie duży zysk na sparse; wartość jeszcze niezmierzona. |
| Złożoność | Średnia. |
| Ryzyko regresji | Pominięcie krawędzi, pięter, przeniesionych przez swap tile lub ukrytych elementów. |
| Zależności | TilePositionIndex, MapChunkStore, selection i affected positions. |
| Wymagane testy | Stary/nowy wynik dla sparse/dense, granic chunków i pięter, usunięć/undo; osobno pomiar dużego pustego prostokąta. |
| Rollback | Pozostawić poprzedni iterator jako referencję/fallback do uzyskania równoważności. |

### 7. Render staging i Preview — F07/F08

| Pole | Plan |
|---|---|
| Obecnie | Instancing, dirty chunks i cache już działają; Preview ma własny atlas. |
| Słabość | Możliwe alokacje scratch i pełny upload po rewizji atlasu; wielkość udziału kosztów nieznana. |
| Zmiana | Po etapie 4 reuse pojemności scratch/UBO bez zmiany kolejności; niezależnie prototyp dirty rectangles atlasu Preview, jeśli trace potwierdzi upload bottleneck. |
| Oczekiwana korzyść | Mniej alokacji lub bajtów transferu; bez obietnicy FPS. |
| Złożoność | Mała–średnia reuse; średnia dirty rectangles. |
| Ryzyko regresji | Stare dane GPU, błędny offset/alignment, artefakty patchy, zasoby z obcego QRhi. |
| Zależności | MapRhiBackend, MapRhiRenderer, Preview, cache keys i atlas revision. |
| Wymagane testy | OpenGL/D3D11 przez QRhi, resize/DPR, cache hit/miss, transparentne overlap, wielopiętrowość, overflow i odbudowa atlasu; image diff oraz pomiar tej samej sceny. |
| Rollback | Dwa oddzielne commity; pełny upload jako fallback. Nie scalać całej mapy na podstawie syntetycznego testu draw calls. |

### 8. Kontrakt edycji i historii — F09

| Pole | Plan |
|---|---|
| Obecnie | Historia grupuje snapshoty tile, deduplikuje pozycje i ogranicza bajty. |
| Słabość | Ręczne begin/end nie opisuje bezpiecznie nesting i abort. |
| Zmiana | Najpierw RAII nad istniejącym grupowaniem z jednoznacznym nesting/commit/abort. Dopiero potem opcjonalne małe delty dla zmierzonych częstych operacji; pełny snapshot jako fallback. |
| Oczekiwana korzyść | Przewidywalne kończenie transakcji; ewentualna redukcja historii wymaga pomiaru. |
| Złożoność | Średnia RAII, wysoka delty. |
| Ryzyko regresji | Różnice undo/redo metadata, children, kolejności itemów, border halo i zones. |
| Zależności | Reader history, MapEditController, brushes, clipboard, invalidacja rendera. |
| Wymagane testy | Zagnieżdżenie, odmowa operacji w połowie, compound stroke, losowe replay+undo+redo porównujące serializowany stan, soft cap i jeden duży action. |
| Rollback | Zachować snapshot implementation i włączać delty per typ operacji, bez migracji istniejących plików. |

### 9. Granice assetów i utrzymanie

| Pole | Plan |
|---|---|
| Obecnie | Istnieją cache z limitami, async atlas i duży Backend/MapView; C++17. |
| Słabość | Koszt synchronicznej zmiany profilu i łączny working set wymagają pomiaru. |
| Zmiana | Najpierw statystyki pamięci per cache i lifecycle. Potem wersjonowany pakiet assetów na granicy istniejących usług. Podział targetów CMake lub migracja C++20 tylko osobno, z konkretną potrzebą. |
| Oczekiwana korzyść | Czytelniejsza własność i możliwość kontroli szczytowej pamięci. |
| Złożoność | Średnia–wysoka; nie wymaga przebudowy wszystkich klas. |
| Ryzyko regresji | Invalidacja readerów i modeli QML, koszt podwójnych assetów, niezgodność ABI SDK. |
| Zależności | Client profiles, DAT/SPR/OTB, image providers, atlas jobs, QML. |
| Wymagane testy | Szybkie przełączenia profili, stale thumbnails, close podczas load, asset errors, cold/warm latency i peak RSS; pełny build/packaging przy zmianie standardu. |
| Rollback | Rozdzielone commity; stary profil musi działać aż nowy zestaw jest kompletny. Nigdy nie przywracać bibliotek z innego SDK do tej samej paczki. |

## Kryteria akceptacji i zatrzymania

- Poprawność: identyczny logiczny roundtrip mapy, kolejność itemów i rysowania, houses/spawns, legacy subtypes i opcjonalne zones; brak zmiany UI.
- Responsywność: raport dla tej samej sceny przed/po, bez utożsamiania CPU-submit z czasem GPU. Próg akceptacji ustalić po rzeczywistym baseline; nie deklarować arbitralnego procentu zysku.
- Pamięć: raport retained/peak, kolejki, cache i zasoby żyjące dzięki snapshotom. Mniejsza liczba new nie dowodzi mniejszego RSS.
- Stop: zatrzymać wdrożenie przy różnicy obrazu/formatu, pogorszeniu p95, niespójnym undo albo wzroście pamięci przekraczającym uzgodniony budżet.
- Rollback: cofać wyłącznie własny odrębny commit lub przełączać zachowaną ścieżkę; nie resetować istniejących zmian użytkownika. Audyt nie tworzy ani nie publikuje commitów.

Nie ma obecnie dowodu uzasadniającego wymianę całego magazynu mapy. Hybrydowy indeks już jest chunkowy; rozdzielić ewentualną zmianę polityki sparse indeksu od wymiany deque i stabilności wskaźników. Najpierw rozwiązać poprawność, następnie zmierzone postoje GUI, potem koszt rzeczywistego renderowania.
