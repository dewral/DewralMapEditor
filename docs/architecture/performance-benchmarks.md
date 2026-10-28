# Benchmarki i weryfikacja audytu

## Co rzeczywiście wykonano

Data: 2026-10-26. Windows 11 Pro 10.0.26200; Intel i5-12400F, 6 rdzeni/12 wątków; NVIDIA RTX3060, sterownik 32.0.16.1714 / 617.14; Qt **6.10.2 MinGW**, GCC **13.1**, C++17, `-O3 -DNDEBUG`, istniejący single-config Release. Driver log potwierdził NVIDIA dla OpenGL 3.3 i D3D11. Nie zmieniono ustawień zasilania ani nie wyłączono innych programów; cache plików OS nie był opróżniany.

Pomiar dotyczy bieżącego drzewa roboczego opisanego w [current-architecture.md](current-architecture.md). Nie jest porównaniem dwóch wersji aplikacji: nie wdrażano optymalizacji. Porównano alternatywy w odłączonych harnessach, zachowując istniejący kod.

Źródła i surowe dane:
- [Indeks: harness](benchmarks/index-benchmark.cpp), [45 próbek CSV](benchmarks/index-results.csv).
- [Mapa: harness z rzeczywistym OTFormats](benchmarks/map-benchmark.cpp), [15 próbek CSV](benchmarks/map-results.csv), [reprodukcja granic](benchmarks/bounds-repro.txt).
- [QRhi: harness oparty na istniejącym teście i MapRhiBackend](benchmarks/rhi-benchmark.cpp), 3 procesy na backend, np. [OpenGL 1](benchmarks/rhi-opengl-1.csv), [D3D11 1](benchmarks/rhi-d3d11-1.csv); pozostałe próby 2/3 w tym katalogu.
- [Sprzęt](benchmarks/hardware.json), [oddzielny pomiar CPU/pamięci procesu](benchmarks/process-metrics.json), [skrypt odtworzenia](benchmarks/run-audit.ps1).

## A. Porównanie indeksów pozycji

Deterministyczne pozycje na piętrze 7, seed shuffle 12345. Dense512/1024 to pełne kwadraty, scatter256 to 65 536 punktów co128 współrzędnych, każdy w osobnym chunku. 5 prób każdej struktury; kolejność hybrid/hash naprzemienna. Sorted vector zawsze ostatni; to ograniczenie eksperymentu. Wszystkie lookup/mutation sprawdzają poprawność, checksum jest wspólny. Lookup oznacza **3×N trafień** w przetasowanej kolejności, mutacja **256 erase+insert**. Brak miss-dominated i range-scan workloadu.

Wartości to mediany ms. Bytes to suma rozmiarów new/new[] w budowie indeksu, **nie retained bytes/RSS**; obejmuje też alokacje przejściowe i nie obejmuje allocator headers, Qt ani GPU.

| Scena / N | Struktura | Build ms | Lookup 3N ms | 256 mutacji ms | Alokacje build | Allocated B build |
|---|---|---:|---:|---:|---:|---:|
| dense-512 / 262144 | hybrid | 2.355 | 7.321 | 0.0062 | 2817 | 2113768 |
| dense-512 / 262144 | hash | 9.223 | 15.604 | 0.0593 | 262145 | 8509544 |
| dense-512 / 262144 | sorted | 3.935 | 173.569 | 31.2508 | 1 | 4194304 |
| dense-1024 / 1048576 | hybrid | 8.684 | 76.215 | 0.0130 | 11265 | 8455192 |
| dense-1024 / 1048576 | hash | 34.670 | 107.626 | 0.1221 | 1048577 | 33616408 |
| dense-1024 / 1048576 | sorted | 18.134 | 1131.020 | 199.4520 | 1 | 16777216 |
| scatter-256 / 65536 | hybrid | 6.270 | 10.152 | 0.0773 | 131082 | 5646144 |
| scatter-256 / 65536 | hash | 2.039 | 2.027 | 0.0343 | 65537 | 2111320 |
| scatter-256 / 65536 | sorted | 0.898 | 21.527 | 6.3501 | 1 | 1048576 |

**Interpretacja:** obecny hybrid jest dobrym baseline'em gęstych regionów, ale skrajnie sparse workload zwiększa liczbę chunków/alokacji i może faworyzować per-tile hash. Sorted vector oszczędza liczbę alokacji, ale zmiana pośrodku dużej tablicy jest droga. Nie wynika z tego automatyczna przewaga nowego magazynu chunkowego ani potrzeba wymiany deque.

## B. Rzeczywisty model mapy, IO i historia

3 próby każdej sceny. Identyfikatory syntetyczne 100..107, bez DAT/SPR/OTB i bez realnych materiałów. Konstrukcja przez addItem z wyłączonym history służy wyłącznie fixture. Zapis do nowego pliku w QTemporaryDir, odczyt przez rzeczywisty OtbmReader. Sprawdzono tile/item count po load oraz po undo/redo. Bulk zmienia do4096 tile na jednym piętrze; jest to koszt readera, **nie pełnego brush stroke z borders, atlasem i repaintem**. Capture recovery mierzy utworzenie deep snapshot, nie jego zapis. Liczby to mediany, nie p95/p99 z trzech próbek.

| Scena | Tile / item | Save nowy plik ms | Load ms | Bulk4096 ms | Undo ms | Redo ms | Recovery capture ms |
|---|---|---:|---:|---:|---:|---:|---:|
| small | 4096 / 4096 | 3.398 | 7.427 | 0.931 | 0.612 | 0.571 | 0.211 |
| medium | 65536 / 65536 | 5.494 | 22.748 | 0.762 | 0.478 | 0.512 | 1.668 |
| large | 1048576 / 1048576 | 61.196 | 313.516 | 0.805 | 0.521 | 0.454 | 28.915 |
| item-heavy | 65536 / 524288 | 17.847 | 65.136 | 1.143 | 0.971 | 0.702 | 4.251 |
| four-floors | 262144 / 262144 | 15.980 | 78.691 | 0.779 | 0.486 | 0.412 | 6.825 |

Osobny run wszystkich pięciu scen ×3: 2.774 s wall; ostatnia próbkowana wartość CPU procesu 2.453 s; maksimum próbek working set 319.1 MiB przy interwale 10 ms. To dolne oszacowania z próbkowania całego procesu, nie peak per scena ani VRAM. Nie doliczać ich do tabeli jako precyzyjnych allocation counts.

**Wniosek:** capture miliona tile zajmuje 28.91 ms w tym workloadzie, podczas gdy kod wywołuje capture na GUI przed background serialization. Normalny save również jest synchroniczny. Wynik uzasadnia pracę nad tymi granicami, ale nie jest liczbą dla ciężkiej rzeczywistej mapy. Ponowny zapis z backupem, XML, nested containers i miliony bogatych itemów może mieć inne koszty.

## C. QRhi: batching, upload i cache

Ta sama scena: 3072 nieprzezroczyste czerwone sprite'y po32px, viewport1024×768, świat2048×1536, syntetyczny atlas32×32 i ten sam istniejący MapRhiBackend. Warianty: 1 draw, 48 grup po64 instancje w kolejności wierszy (to nie odwzorowanie produkcyjnego chunka32×32), 3072 drawy, cache po pierwszej klatce oraz 1 draw z dirty uploadem 73 728 B każdej klatki.

Każda próba: 16 warm-up +120 pomiarów; 3 nowe procesy na każdy backend. Końcowy obraz musi być równy baseline'owi i mieć oczekiwane czerwone piksele. Readback jest **po** pomiarach. Brak overlappujących/transparent sprite'ów, świateł, pięter i prywatnych zasobów.

Dwa tryby są celowo odrębne:
- `cpu-submit`: wall od beginOffscreenFrame przez backend.render do endOffscreenFrame, bez GPU wait. To **nie jest czas wykonania GPU ani FPS użytkownika**.
- `serialized-completed`: dodatkowe `QRhi::finish()` w eksperymencie, żeby ograniczyć wpływ kolejkowania. To sztuczna serializacja, nie harmonogram Qt Quick/present. Nie wolno kopiować tego oczekiwania do kodu produkcyjnego ani traktować wyniku jako GPU timestamp.

W tabeli: mediana średniej oraz mediana p95/p99 z trzech prób; percentyle per próba używają sorted samples [113] i [118] ze120. Nie są to percentyle z połączonych360 próbek.

| Backend | Tryb | Wariant | Drawy | Średnia ms | p95 ms | p99 ms |
|---|---|---|---:|---:|---:|---:|
| opengl | cpu-submit | merged | 1 | 0.0021 | 0.0029 | 0.0050 |
| opengl | cpu-submit | chunk64 | 48 | 0.0124 | 0.0152 | 0.0230 |
| opengl | cpu-submit | per-sprite | 3072 | 1.1346 | 1.5104 | 1.6039 |
| opengl | cpu-submit | cache-static | 1 | 0.0019 | 0.0039 | 0.0042 |
| opengl | cpu-submit | merged-upload-every-frame | 1 | 0.0168 | 0.0417 | 0.0588 |
| opengl | serialized-completed | merged | 1 | 0.0022 | 0.0042 | 0.0059 |
| opengl | serialized-completed | chunk64 | 48 | 0.0150 | 0.0277 | 0.0339 |
| opengl | serialized-completed | per-sprite | 3072 | 1.1982 | 1.6273 | 1.6854 |
| opengl | serialized-completed | cache-static | 1 | 0.0012 | 0.0013 | 0.0027 |
| opengl | serialized-completed | merged-upload-every-frame | 1 | 0.0065 | 0.0096 | 0.0128 |
| d3d11 | cpu-submit | merged | 1 | 0.0463 | 0.1156 | 0.2255 |
| d3d11 | cpu-submit | chunk64 | 48 | 0.0955 | 0.1435 | 0.2634 |
| d3d11 | cpu-submit | per-sprite | 3072 | 5.7748 | 6.8258 | 7.7173 |
| d3d11 | cpu-submit | cache-static | 1 | 0.0425 | 0.1529 | 0.2010 |
| d3d11 | cpu-submit | merged-upload-every-frame | 1 | 0.0635 | 0.1923 | 0.3032 |
| d3d11 | serialized-completed | merged | 1 | 0.0429 | 0.1049 | 0.2790 |
| d3d11 | serialized-completed | chunk64 | 48 | 0.0723 | 0.1225 | 0.2150 |
| d3d11 | serialized-completed | per-sprite | 3072 | 5.4322 | 6.7041 | 7.1840 |
| d3d11 | serialized-completed | cache-static | 1 | 0.0365 | 0.1046 | 0.1986 |
| d3d11 | serialized-completed | merged-upload-every-frame | 1 | 0.0414 | 0.1269 | 0.1783 |

**Interpretacja:** per-sprite draw jest kosztowny w tym workloadzie, a dirty upload ma widoczny narzut. Nie oznacza to, że produkcyjny renderer tak działa — już używa instancji, dirty buffers i cache. Margines cache oraz oddzielne chunki amortyzują edycję; stały cache nie zawsze wygrywa dla bardzo taniej sceny. Nie wyciągać z tej tabeli rekomendacji zmiany domyślnego backendu ani deklaracji przyspieszenia DME.

## D. Wykonane testy

Zbudowano istniejące targety: tile_position_index_test, otbm_compact_storage_test, otbm_editing_storage_test, otbm_background_load_test, otbm_rme_compatibility_test, otbm_legacy_subtypes_test, otbm_numbered_zones_test, map_rhi_backend_test, atlas_background_build_test, map_creature_refresh_test, document_recovery_test. Build zakończył się sukcesem.

CTest: **14/14 PASS**, 34.70 s łącznie: otbm_rme_compatibility, otbm_legacy_subtypes, map_rhi_opengl, map_rhi_d3d11, otbm_compact_storage, otbm_numbered_zones, otbm_editing_storage, tile_position_index, otbm_background_load, atlas_background_build, map_creature_refresh, map_view_position, map_item_locations, document_recovery. To weryfikacja poprawności, nie benchmark czasów testów.

## E. Pokrycie wymaganych workloadów

| Workload | Status |
|---|---|
| Mała, średnia, duża mapa | Zmierzone syntetyczne4096 /65536 /1048576 tile dla modelu/IO |
| Gęsty region item-heavy | Zmierzone65536 tile ×8 items, bez assetów |
| Szybka kamera / zoom | Produkcyjny viewport niezmierzony; sam test cache nie jest jego zamiennikiem |
| Wiele pięter | IO/storage cztery piętra; GPU warstwy w testach poprawności, bez benchmarku realnego viewportu |
| Duży brush | Reader bulk4096 zmierzony; pełne materiały/borders/gesture niezmierzone |
| Bulk undo/redo | Zmierzone na readerze; testy integracji także przeszły |
| Full load/save | Syntetyczny OTBM zmierzony; realny klient/sidecars/backup niezmierzone |
| Init klienta | Ścieżka źródłowa przeanalizowana; brak bieżącego benchmarku prawdziwych DAT/SPR |
| GPU utilization / VRAM / timestamps | Nie wykonano; draw counts są konstrukcją harnessu, nie globalnym trace DME |
| Cache hits / alokacje / lock contention całej aplikacji | Nie zmierzono; allocation counts tylko indeks, koszty cache analizowane w źródłach |

## Odtworzenie i następny baseline

Skrypt jest przeznaczony dla istniejącego **Windows/MinGW single-config Release** i dopasowanego SDK. Nie konfiguruje produkcyjnego CMake, nie publikuje i nie uruchamia aplikacji użytkownika. Wymaga zbudowania wskazanych testów, a następnie:

```powershell
# Parametry wskazują istniejące lokalne drzewo builda i SDK z jego cache.
& docs/architecture/benchmarks/run-audit.ps1 -BuildDirectory <istniejacy-build> -QtPrefix <pasujacy-SDK>
```

Nawiasy to parametry, nie gotowe ścieżki. Po zmianie źródeł najpierw zbuduj na nowo OTFormats/map_rhi_backend_test; harness linkuje te istniejące artefakty. Zweryfikowano pełne wykonanie skryptu w bieżącym środowisku. MSVC/Linux/macOS wymaga adaptacji linkowania, nie zostało to przetestowane.

Przed implementacją optymalizacji viewportu potrzebny jest odrębny zapis warunków sceny użytkownika: hash mapy/assetów bez publikacji ich zawartości, trasa kamery, dpi/zoom/piętra, flags, klatki warm-up, VSync/FPS cap, perf markers sync/chunk/build/upload/present i pamięć per kategoria. Każdy wariant porównać w tej samej scenie. Hipotezy z pozostałych dokumentów pozostają hipotezami do tej walidacji.
