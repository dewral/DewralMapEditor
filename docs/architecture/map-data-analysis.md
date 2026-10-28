# Dane mapy, indeksowanie i serializacja

## Wniosek
Nie zastępować teraz magazynu. DME ma już rzadki deque, hybrydowy indeks chunkowy i odrębny przestrzenny indeks renderera. Benchmark potwierdził zalety gęstych chunków oraz koszt dla skrajnie rozproszonych danych. Dane: [performance-benchmarks.md](performance-benchmarks.md).

### Magazyn mapy

| Pole | Ustalenie |
|---|---|
| Odpowiedzialność | Autorytatywne tile/item, kontenery, domy, spawny i creatures. |
| Źródła / wejście | [libs/otformats/otbmreader.h:240](../../libs/otformats/otbmreader.h#L240); [libs/otformats/otbmreader.h:592](../../libs/otformats/otbmreader.h#L592) |
| Zależności | CompactVector, CompactQString, TileZoneIds, TilePositionIndex; bez GPU. |
| Interfejsy | `tiles`, `tileAt`, `placeItem`, mutacje item/house/spawn/creature. |
| Przepływ runtime | Parser → deque → indeks pozycji; edycje w miejscu → obserwujący indeks MapView. |
| Własność zasobów | OtbmReader posiada tile; item posiada extra/children. Single item inline, pozostałe rzadkie pola opcjonalne. |
| Inicjalizacja / destrukcja | `reset` usuwa mapę/indeksy/historię. Structural undo naprawia także adres relokowanego ostatniego tile. |
| Operacje kosztowne | Pełna iteracja N, deep copy rare extras, snapshot i bulk mutations. |
| Słabości / pewność | F01: brak górnego limitu współrzędnych w placeItem/addItem; reprodukcja poniżej. |
| Możliwa poprawa | Walidacja checked position przed recordTile i narrowing; zachować istniejący format. |

### Indeksy i regiony

| Pole | Ustalenie |
|---|---|
| Odpowiedzialność | Lookup pozycji oraz mapowanie widoku do niepustych kafelków. |
| Źródła / wejście | [libs/otformats/tilepositionindex.h:13](../../libs/otformats/tilepositionindex.h#L13); [editor/map/mapview_chunks.cpp:21](../../editor/map/mapview_chunks.cpp#L21) |
| Zależności | Hash chunków, sparse entries/dense array, MapChunkStore, floor bounds. |
| Interfejsy | `insert/find/erase`, `estimatedBytes`, `denseChunkCount`; floor/chunk index. |
| Przepływ runtime | Key z/y/x → chunk32×32 → local slot → indeks deque; renderer posiada dodatkowy vector const tile pointers sortowany y/x. |
| Własność zasobów | Indeks pozycji należy do readera, indeks renderera do MapChunkStore. Wskaźniki są nieposiadające. |
| Inicjalizacja / destrukcja | Budowa po load, aktualizacja append; structural undo odtwarza dotknięte chunki. |
| Operacje kosztowne | Hash + O(log k) sparse lookup lub O(1) dense; przesuwanie lokalnego vector przy sparse mutation. |
| Słabości / pewność | Dense threshold=256 bez demotion do czasu opróżnienia. estimatedBytes jest estymatą, nie RSS. |
| Możliwa poprawa | Najpierw occupancy/region benchmarks; potem ewentualny tuning progu, demotion lub ograniczenie drobnych alokacji. |

## Reprezentacja

W wykonanym buildzie `sizeof(OtbmTile)=64 B`, `sizeof(OtbmMapItem)=16 B`; static_assert chroni granice. Nie obejmuje to heap extra, children ani allocator overhead. Ground/borders są elementami stosu items, a nie drugą globalną mapą. `is_ground`, OTB/DAT i managed border IDs wpływają na klasyfikację. Creature name, NPC, direction, spawntime i spawn radius są polami tile; dom ma flagę i house_id oraz listę house records. Towns/waypoints mają osobne kolekcje.

x/y są uint16, piętra 0..15. Deque chroni adresy przy dopisywaniu, nie dowolnym erase. Undo usuwa przez swap-last/pop, aktualizuje indeks i dodaje relokowany tile do lastAffected; MapView odtwarza również jego chunk. Nie należy zastępować tego zwykłym erase.

Źródła: [libs/otformats/otbmreader.h:88](../../libs/otformats/otbmreader.h#L88); [libs/otformats/otbmreader.cpp:2994](../../libs/otformats/otbmreader.cpp#L2994); [editor/map/mapview_edit.cpp:207](../../editor/map/mapview_edit.cpp#L207).

| Struktura | Koszt pamięci | Lookup/iteracja | Mutacja i zgodność | Ocena |
|---|---|---|---|---|
| Pełna flat array świata | 65536²×16×64 B = 4 TiB samych tiles; 4 B indeks/slot = 256 GiB | O(1), skan pustych miejsc | Prosta, ale nierealna pamięć | Odrzucić globalnie |
| Flat array ograniczonego regionu | 64 B na każdy slot, także pusty | Lokalnie szybka | Zmiana granic/relokacja i occupancy | Kandydat na patch, nie cały świat |
| Hash mapa per tile | Node/buckets per pozycja | Oczekiwane O(1), słaby skan | Dużo alokacji, łatwa mutacja | Zmierzona, dobra dla skrajnie rozproszonych danych |
| Sorted flat vector | Ciągłe wpisy, mało alokacji | Binary search i dobre skany | Przesuwanie ogona, invalidacja adresów | Zmierzona; słaba przy mutacji dużej tablicy |
| Dense chunk tile[1024] | 64 KiB/chunk także dla pustych slots | Dobra lokalność i region query | Potrzebne stable handles, occupancy i adapter historii | Cały zamiennik nie został zmierzony |
| Obecny hybrid + deque | Dense index 4096 B/chunk; sparse entries do progu | Hash + lokalny lookup; sekwencja deque osobna od regionu | Stabilny append, naprawa przy usuwaniu | Zachować baseline |

Porównanie złożoności nie jest benchmarkiem wszystkich alternatyw. Zmierzono hybrid, unordered_map i sorted vector; nie kompletny zamiennik magazynu chunkowego.

### Parser, zapis i sidecars

| Pole | Ustalenie |
|---|---|
| Odpowiedzialność | Walidacja drzewa OTBM oraz zapis mapy/spawn/house. |
| Źródła / wejście | [libs/otformats/nodefilereader.cpp:116](../../libs/otformats/nodefilereader.cpp#L116); [libs/otformats/otbmreader.cpp:3639](../../libs/otformats/otbmreader.cpp#L3639) |
| Zależności | QFile/mmap, BinaryNode, text codec, item type flags, QSaveFile/XML. |
| Interfejsy | `loadFileDetached/adoptLoadedState`, `saveFile`, recovery snapshot. |
| Przepływ runtime | Mmap/readAll fallback → unescaped data + tree → tiles → indeks → XML. NodeWriter zapisuje przez QSaveFile. |
| Własność zasobów | NodeFileReader posiada node tree/data do końca parsowania; reader posiada tiles; staging/backup RAII. |
| Inicjalizacja / destrukcja | Detached load chroni żywy dokument. Błąd save przywraca metadata i próbuje rollback XML. |
| Operacje kosztowne | Przejściowe node tree + gotowe tiles, walidacja legacy, XML i pełny backup istniejącej mapy. |
| Słabości / pewność | F03: normalny save jest synchroniczny z QML; wieloplikowy commit ma rollback, ale nie atomowość przy awarii zasilania. |
| Możliwa poprawa | Background save tylko ze snapshotem wersji/dirty contract. Journal sidecars jako oddzielna, testowana zmiana. |

## Kompatybilność i bezpieczeństwo IO

NodeFileReader dopuszcza pliki do 4 GiB i głębokość 512; mmap ma fallback do readAll. Buduje wspólny bufor unescaped oraz drzewo, nie osobny QByteArray na każdy node. Nie jest to pełny streaming parser. Zmiana wymaga testów escape bytes, anulowania, kontenerów, legacy, duplikatów i peak RAM.

Oryginalne bajty tekstów oraz obsługiwane named attribute payloads są zachowywane. Nieznany format/atrybut może być jawnie odrzucony; nie obiecujemy dowolnego round-trip wszystkich nieznanych pól bez informacji o ich długości. Legacy subtype zależy od wersji OTBM i typu itemu. Numerowane zones są opcjonalne w zwykłym zapisie, lecz zachowywane w recovery. Sidecars są opcjonalne przy load; unread/malformed XML nie wolno bezgłośnie zastępować.

Zapis: odrębne target paths, QSaveFile, staging XML, backup `.bak`, XML commit i na końcu mapa; rollback w razie błędu commit zgłasza także niepowodzenie przywracania. To zabezpieczenie w procesie, nie crash-atomic transakcja trzech plików.

Źródła: [libs/otformats/nodefilereader.h:58](../../libs/otformats/nodefilereader.h#L58); [libs/otformats/nodefilereader.cpp:142](../../libs/otformats/nodefilereader.cpp#L142); [libs/otformats/otbmreader.cpp:374](../../libs/otformats/otbmreader.cpp#L374); [libs/otformats/otbmreader.cpp:3750](../../libs/otformats/otbmreader.cpp#L3750); [libs/otformats/otbmreader.cpp:3916](../../libs/otformats/otbmreader.cpp#L3916).

## F01 — potwierdzona reprodukcja granic API

`newMap(1024,1024,1098,3,57)` → `placeItem(65536,1,7,100,0,false,true)`:

```text
accepted=1 tiles=1 original_lookup=0 wrapped_lookup=0 stored_x=0
```

Sprawdzana jest tylko dolna granica x/y, potem narrowing do uint16 i zignorowany wynik index.insert, który odrzuca oryginalny key. Zapis może wyemitować obciętą pozycję. P2: naruszony kontrakt C++ API; konkretny wpływ UI wymaga osobnej reprodukcji. Nie stwierdzamy exploita.

Źródła: [libs/otformats/otbmreader.cpp:1349](../../libs/otformats/otbmreader.cpp#L1349); [libs/otformats/tilepositionindex.h:229](../../libs/otformats/tilepositionindex.h#L229).
