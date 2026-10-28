# Dalsza analiza: odzyskiwanie, backupy i zaznaczanie — 2026-10-27

Analiza bieżącego lokalnego drzewa po poprawkach F01/F02/F10. W tym etapie **nie zmieniono kodu aplikacji, UI, CMake ani istniejących testów**. Dodano odłączone harnessy, wyniki i ten raport. Wnioski dotyczą wykonanych scenariuszy; brak założenia, że występują przy każdym zapisie.

Status: wszystkie cztery punkty naprawiono w [raporcie wdrożenia](four-fixes-2026-10-27.md). Poniżej zachowano dowody i pomiary stanu sprzed poprawek.

## Najpierw: odtworzone błędy poprawności

### F11 — P1: dwie instancje nadpisują manifest sesji

**Warunek:** dwie instancje używają tego samego katalogu recovery. Instancja B działa już przed utworzeniem kopii w A; następnie B zapisuje swój stan sesji, a A ulega awarii.

**Mechanizm:** każdy DocumentManager odczytuje listę odzyskiwania przy konstrukcji, a później zapisuje wspólny `session.json` na podstawie własnych dokumentów i własnej listy pending recoveries. QSaveFile zabezpiecza pojedynczy zapis przed częściową zawartością, ale nie scala stanu procesów. Zamknięcie B może oznaczyć wspólną sesję jako clean, choć A ma niezapisany dokument i gotową kopię.

Źródła: [editor/core/documentmanager.cpp:22](../../editor/core/documentmanager.cpp#L22); [editor/core/documentmanager.cpp:166](../../editor/core/documentmanager.cpp#L166); [editor/core/documentmanager.cpp:266](../../editor/core/documentmanager.cpp#L266); [editor/core/documentmanager.cpp:454](../../editor/core/documentmanager.cpp#L454).

**Reprodukcja:** dwa rzeczywiste procesy harnessu korzystają z tych samych plików, bez współdzielenia obiektów w pamięci. B startuje pusta, A tworzy brudną mapę i kończy autosave. B publikuje stan i zamyka się poprawnie. Harness kończy wyłącznie swój proces A, symulując awarię. Kolejny manager oferuje **0 kopii**, mimo że poprawny plik recovery nadal istnieje. JSON: `writerSessionDirty=true`, `otherInstanceMarkedClean=true`, `snapshotStillExists=true`, `offeredRecoveries=0`.

**Skutek:** automatyczne odzyskiwanie może nie zaoferować niezapisanej pracy; sam plik nie jest w tym scenariuszu usuwany. Ręczne znalezienie pliku w folderze recovery jest nadal możliwe. Nie odtworzono tego przez dwa pełne okna QML; użyto tej samej produkcyjnej klasy DocumentManager w dwóch procesach.

**Kierunek naprawy:** osobny manifest/identyfikator sesji dla każdej instancji i rozpoznawanie zakończonych sesji przy starcie; alternatywą jest jawne wymuszenie pojedynczej instancji. Sama blokada tylko na czas zapisu nie wystarczy — nadal może wygrać logicznie niepełny stan innego procesu. Nie usuwać plików należących do żyjącej sesji. Nie zmieniać formatu OTBM.

### F12 — P2: data oryginału ukrywa kopię z niezapisaną zmianą

**Warunek:** mtime oryginalnego pliku jest późniejsze lub równe mtime kopii recovery, mimo że kopia zawiera późniejsze logiczne zmiany. Może to wynikać z innego zegara hosta/pliku sieciowego lub rozdzielczości znaczników czasu; odtworzono konkretnie datę oryginału przesuniętą o godzinę.

Źródło: [editor/core/documentmanager.cpp:244](../../editor/core/documentmanager.cpp#L244).

**Reprodukcja:** zapis oryginału z jednym kafelkiem; ustawienie jego mtime o godzinę do przodu; dodanie drugiego kafelka i poprawny autosave. Oryginał drugiego kafelka nie zawiera, recovery zawiera. Po ponownym uruchomieniu listy: `offeredRecoveries=0`, plik kopii nadal istnieje. Przypadek równych czasów nie był osobno wykonywany — wynika z warunku `<=`.

**Skutek:** odrzucenie prawidłowej kopii na podstawie metadanych systemu plików. Ponowny zapis manifestu może dodatkowo usunąć ją z listy znanych recovery, pozostawiając osierocony plik.

**Kierunek naprawy:** odróżnić logiczną rewizję zapisanej mapy od daty ściennej. Jawny dirty manifest i ważny snapshot powinny zachować możliwość odzyskania; mtime może być informacją ostrzegawczą, a nie jedynym powodem ukrycia. Zachować test, że prawdziwy nowszy zapis użytkownika nie zostaje automatycznie zastąpiony starszym recovery.

### F13 — P2: plik zewnętrzny może nadpisać backup mapy

**Warunek:** dla `world.otbm` użytkownik ustawi w Map Properties nazwę spawnów `world.otbm.bak`, a potem zapisze mapę co najmniej drugi raz.

Źródła: [libs/otformats/otbmreader.cpp:3688](../../libs/otformats/otbmreader.cpp#L3688); [libs/otformats/otbmreader.cpp:3866](../../libs/otformats/otbmreader.cpp#L3866); [libs/otformats/otbmreader.cpp:3931](../../libs/otformats/otbmreader.cpp#L3931); wejście UI: [editor/qml/dialogs/MapPropertiesDialog.qml:26](../../editor/qml/dialogs/MapPropertiesDialog.qml#L26).

**Mechanizm:** walidacja porównuje trzy pliki docelowe OTBM/spawn/house, ale nie uwzględnia nazw ich backupów. Zapis tworzy poprawny backup OTBM w `world.otbm.bak`, po czym commit XML spawnów zastępuje go XML-em. Wszystkie operacje mogą zakończyć się sukcesem.

**Reprodukcja:** `saveAccepted=true`, `primaryValidOtbm=true`, `backupEqualsPreviousMap=false`, `backupValidOtbm=false`, `backupContainsSpawnsXml=true`. Główna mapa jest poprawna; uszkodzony jest kontrakt kopii `.bak`. Nie odtworzono tu częściowego błędu dysku ani awarii między commitami.

**Kierunek naprawy:** przed otwarciem plików zweryfikować kolizje wszystkich aktywnych celów i ich `.bak`, po normalizacji ścieżek właściwej dla systemu. Nie polegać wyłącznie na rozszerzeniu w polu UI: obce mapy i API mogą używać innych nazw. Błąd ma przerwać operację przed zmianą plików i metadanych dokumentu.

## F06 — zmierzony koszt pustego zaznaczenia

Źródła: [editor/map/mapview_input.cpp:154](../../editor/map/mapview_input.cpp#L154); [editor/map/mapview_input.cpp:664](../../editor/map/mapview_input.cpp#L664).

Produkcja przelicza cały prostokąt i każde wybrane piętro, sprawdzając tileAt dla pustych współrzędnych. Coalescing ruchów kursora ogranicza liczbę przeliczeń, ale nie skraca pojedynczego przeliczenia. Koszt jest proporcjonalny do powierzchni × liczby pięter, zamiast do liczby kandydatów w istniejących chunkach.

**Metoda:** rzeczywisty MapView, chronione zdarzenia myszy udostępnione przez testową podklasę; Shift+drag, `mouseMoveEvent` i `advancePointerFrame`. MapView 1920×1080, tileSize=1 (około3.125% skali standardowych32px), floor7, compensatedSelect=false. Syntetyczna mapa ma **18 kafelków**, po2 na każdym piętrze7..15. Wynik sprawdzono:2 albo18 zaznaczonych. Brak widocznego okna, GPU i prywatnych assetów. Pomiar obejmuje obsługę jednego ruchu na wątku GUI, bez renderowania; nie jest pomiarem całej klatki ani FPS.

Każdy przypadek:1 warm-up +3 pomiary, kolejność ustalona. Windows/MinGW Release, Qt6.10.2, C++17, sprzęt jak w baseline audytu (i5-12400F). Nie wyłączano procesów tła. Tabela przedstawia medianę3 próbek; nie p95/p99.

| Prostokąt, pola | Piętra | Kafelki w całej mapie | Zaznaczone | Obsługa ruchu GUI, ms |
|---|---:|---:|---:|---:|
| 64×64 | 1 | 18 | 2 | 0.0240 |
| 256×256 | 1 | 18 | 2 | 0.3475 |
| 1024×512 | 1 | 18 | 2 | 3.0022 |
| 1900×1000 | 1 | 18 | 2 | 10.4617 |
| 64×64 | 9 | 18 | 18 | 0.2367 |
| 256×256 | 9 | 18 | 18 | 3.3511 |
| 1024×512 | 9 | 18 | 18 | 28.0402 |
| 1900×1000 | 9 | 18 | 18 | 104.9900 |

**Wniosek:** około105 ms dla1900×1000 i9 pięter przekracza budżet interakcji jeszcze przed renderem. Duża liczba itemów nie jest konieczna do wystąpienia tego kosztu. Warto porównać iterator zajętych chunków przecinających region, z dokładnym filtrowaniem granic i zachowaniem compensation/multi-floor. Pełny skan wszystkich kafelków również może być zły dla małego regionu ogromnej mapy — potrzebny jest dobór kandydatów przestrzennych. Nie wdrażano zamiennika i nie deklaruje się jeszcze zysku nowego algorytmu.

## Odtworzenie i status weryfikacji

Źródła i dane:

- [Harness IO](benchmarks/io-continuation.cpp) i [JSON z wynikami](benchmarks/io-continuation-results.json).
- [Harness zaznaczania](benchmarks/selection-continuation.cpp) i [24 próbki CSV](benchmarks/selection-continuation-results.csv).
- [Skrypt kompilacji i uruchomienia](benchmarks/run-continuation.ps1).

Skrypt jest przeznaczony dla istniejącego Windows/MinGW Release oraz zgodnego SDK. Najpierw zbudować rzeczywiste targety `document_recovery_test` i `map_creature_refresh_test` w tym samym buildzie; harnessy linkują ich produkcyjne obiekty, wyłączając main testu. Nie konfiguruje ani nie podmienia aplikacji. Wszystkie pliki IO powstają w QTemporaryDir; proces kończony przez harness jest wyłącznie jego własnym dzieckiem z tymi syntetycznymi danymi.

```powershell
& docs/architecture/benchmarks/run-continuation.ps1 -BuildDirectory <istniejacy-build-Release> -QtPrefix <SDK-z-cache>
```

Zweryfikowano pełne wykonanie skryptu w bieżącym środowisku. Exit0 harnessu IO oznacza, że **odtworzono opisane błędy**, nie że aplikacja jest bezbłędna. Istniejący `ctest --test-dir <build> -R '^document_recovery$' --output-on-failure` także przeszedł (0.91s): obejmuje izolację snapshotu, późny Save/Close i retry, ale nie trzy nowe przypadki.

Nie wykonano: pełnego UI z dwoma oknami DME, awarii fizycznego dysku, równych timestampów, testów na Linux/macOS i interaktywnego renderu sceny zaznaczenia. Nie zmieniano produkcyjnych źródeł ani wcześniej dodanych testów i napraw. Integralność istniejących plików sprawdzono po zakończeniu.

## Priorytet dalszej pracy

| Kolejność | Zmiana | Najważniejsze testy / ryzyko |
|---|---|---|
| 1 | F11: niezależność sesji procesów | Dwa procesy w różnej kolejności start/exit/crash; brak przejęcia/usunięcia aktywnej sesji, migracja starego manifestu, PID reuse. |
| 2 | F12: kwalifikacja recovery przez stan logiczny | Przesunięty i równy czas, normalne Save/Close/reload, stary snapshot po poprawnym nowszym zapisie; bez automatycznego nadpisania oryginału. |
| 3 | F13: walidacja kompletnego zestawu ścieżek zapisu | Kolizje OTBM/XML/backup, relative paths, case na Windows, brak zmian plików po odmowie, standardowy zapis i rollback. |
| 4 | F06: iteracja kandydatów regionu | Równoważny zbiór zaznaczenia, podstawowy/compensated, piętra, granice chunków, ctrl-add i usunięcia/undo; ta sama scena do porównania czasu. |

Każda poprawka w osobnym commicie, z reprodukcją zamienioną w test regresji. Dla zmiany recovery zachować bezpieczne odczytanie starego formatu sesji; rollback nie może kasować utworzonych kopii. Dla zaznaczania zachować starą ścieżkę jako porównanie do czasu potwierdzenia równoważności. Nie łączyć tych prac z wymianą magazynu mapy, wyglądu UI ani QRhi.
