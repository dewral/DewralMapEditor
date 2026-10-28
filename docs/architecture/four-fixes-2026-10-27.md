# Naprawy recovery, backupów i zaznaczania — 2026-10-27

Wdrożono wszystkie cztery problemy opisane w [dalszej analizie](continuation-2026-10-27.md): F11, F12, F13 i F06. Zachowano UI, QRhi, format mapy, wersję wydania oraz istniejące zmiany użytkownika. Nie dodano zależności i nie publikowano GitHub/main.

## Implementacja

### F11: sesja należy do instancji

Każdy DocumentManager zapisuje `session-<UUID>.json` i utrzymuje QLockFile przy tym manifeście. Nie przejmuje manifestów żywych instancji. Blokada nie staje się przeterminowana na podstawie samego wieku; Qt rozpoznaje zakończony proces. Kopie odzyskiwania nadal mają unikalne nazwy.

Przy starcie odczytywane są zarówno nowe manifesty, jak i stare `session.json`. Manifest zakończonej sesji jest blokowany podczas przejęcia, jego wpisy trafiają do nowego manifestu, a stary plik zostaje usunięty dopiero po udanym zapisie i commit. Nieudany zapis pozostawia stary manifest i snapshoty. Zduplikowane identyfikatory/ścieżki kopii są pomijane; nierozpoznana wersja lub uszkodzony JSON pozostają na dysku.

Źródła: [editor/core/documentmanager.cpp:220](../../editor/core/documentmanager.cpp#L220); [editor/core/documentmanager.cpp:232](../../editor/core/documentmanager.cpp#L232); [editor/core/documentmanager.cpp:292](../../editor/core/documentmanager.cpp#L292); [editor/core/documentmanager.h:87](../../editor/core/documentmanager.h#L87).

### F12: mtime nie ukrywa niezapisanej pracy

Wpis oznaczony jako dirty i istniejący plik recovery pozostają dostępne, nawet gdy data oryginału jest późniejsza lub równa. Zachowano jawne anulowanie recovery przy poprawnym Save/Close/reload oraz ochronę generacji zadań. Odzyskanie nadal jest wyborem użytkownika; sam start programu nie nadpisuje oryginału.

### F13: walidacja obejmuje pliki `.bak`

Zanim zostanie otwarty plik zapisu, sprawdzane są wszystkie aktywne cele OTBM/spawn/house oraz ich nazwy `.bak`. Normalizacja uwzględnia istniejącą ścieżkę kanoniczną i katalog rodzica; zachowano konserwatywne porównywanie bez rozróżniania wielkości liter, także dla takich woluminów poza Windows. Odmowa przywraca metadane zmienione podczas przygotowania zapisu, zachowuje dirty i nie zmienia istniejących plików. Recovery nie tworzy normalnych backupów i nie rezerwuje nieużywanych nazw.

Źródło: [libs/otformats/otbmreader.cpp:3688](../../libs/otformats/otbmreader.cpp#L3688).

### F06: przestrzenna iteracja zajętych pozycji

Zaznaczanie korzysta z istniejącego hybrydowego TilePositionIndex należącego do readera. Region odwiedza tylko odpowiednie chunki albo istniejące chunki przy dużym pustym obszarze; sparse chunk przechodzi zajęte wpisy, dense chunk sprawdza ograniczony fragment tablicy. Nie powstaje nowy magazyn mapy, kopia kafelków ani tablica kandydatów.

Wybrano indeks readera, ponieważ pozostaje aktualny także po bezpośrednich edycjach API. Indeks renderowania może wtedy jeszcze nie zawierać nowych kafelków. Zachowano dokładne granice prostokąta, wszystkie tryby pięter, compensated selection, Ctrl-add, odwrócone narożniki, undo/redo i pozycje przy0/65535. Operacja respektuje istniejący mutex danych mapy.

Źródła: [editor/map/mapview_input.cpp:154](../../editor/map/mapview_input.cpp#L154); [libs/otformats/otbmreader.h:357](../../libs/otformats/otbmreader.h#L357); [libs/otformats/tilepositionindex.h:94](../../libs/otformats/tilepositionindex.h#L94).

## Regresje i kompilacja

Windows/MinGW GCC13.1, Qt6.10.2, C++17, Release, istniejący build. Zbudowano `DewralMapEditor` oraz14 targetów testowych. Końcowy EXE jest w istniejącym `build/DME-dist/DME.exe`.

**Końcowy CTest:18/18 PASS,51.57s** — zgodność RME/legacy, compact storage, zones, edycja i historia, indeks, background load/atlas, MapView/Preview, pozycja widoku, wyszukiwanie, nowe zaznaczanie, Activity, Work timer, recovery i nowe sesje. QRhi sprawdzono przez istniejące testy OpenGL i D3D11. Dodatkowo nowe testy `document_session` i `map_rubber_band` przeszły przy usuniętym Qt z globalnego PATH, korzystając z konfiguracji CTest.

Nowe/rozszerzone testy:

- [document_session_test.cpp](../../tests/document_session_test.cpp): dwa rzeczywiste procesy, różna kolejność startu, poprawne zamknięcie drugiego i awaria pierwszego, pomijanie żyjących sesji, brak przejęcia/odrzucenia cudzej kopii, kolejna konkurująca instancja, future/equal mtime, odczyt i powtórne otwarcie legacy, zachowanie profilu/nazw XML, nieudany zapis migracji i poprawny retry. Proces kończony w teście jest wyłącznie jego własnym dzieckiem z syntetycznymi danymi.
- [otbm_editing_storage_test.cpp](../../tests/otbm_editing_storage_test.cpp): kolizje OTBM/XML/backup, Save As, wariant case na Windows, brak zmiany plików/metadanych/historii po odmowie oraz normalny backup poprzedniej mapy.
- [map_rubber_band_checks.h](../../tests/map_rubber_band_checks.h): niezależny wzorzec przez projekcję pozycji, sparse i dense, granice chunków/świata, piętra7/9/15, trzy tryby pięter, compensation, Ctrl, reverse drag, undo/redo, bezpośrednie edycje readera z zachowaniem liczby kafelków.
- [CMakeLists.txt](../../CMakeLists.txt): rejestracja nowych testów i ich środowiska Qt.

## Pomiar przed / po

Ta sama scena i harness co w dalszej analizie: MapView1920×1080, tileSize1, mapa18 kafelków, po2 na piętrach7..15, bez assetów i widocznego okna. Mierzona obsługa jednego ruchu na GUI (`mouseMoveEvent` + `advancePointerFrame`) wraz ze sprawdzeniem wynikowego zaznaczenia. Każdy przypadek:1 warm-up +3 próbki, mediana. Zachowano SDK, Release, kompilator i konfigurację sceny; nie uruchamiano benchmarku równocześnie z kompilacją/testami.

| Prostokąt, pola | Piętra | Przed, ms | Po, ms |
|---|---:|---:|---:|
| 64×64 | 1 | 0.0240 | 0.0006 |
| 256×256 | 1 | 0.3475 | 0.0006 |
| 1024×512 | 1 | 3.0022 | 0.0006 |
| 1900×1000 | 1 | 10.4617 | 0.0006 |
| 64×64 | 9 | 0.2367 | 0.0012 |
| 256×256 | 9 | 3.3511 | 0.0010 |
| 1024×512 | 9 | 28.0402 | 0.0009 |
| 1900×1000 | 9 | 104.9900 | 0.0009 |

Dla największego przypadku usunięto około105ms skanowania pustych współrzędnych. Wyniki po zmianie są rzędu mikrosekund i podlegają szumowi timera — praktyczny wniosek to **poniżej0.01ms w tej konkretnej sparse scenie**, nie deklaracja takiego czasu dla dowolnej mapy ani mnożnika FPS. Duża liczba faktycznie zaznaczonych kafelków i oczekiwanie na mutex nadal mogą kosztować.

Dane: [przed](benchmarks/selection-continuation-results.csv), [po](benchmarks/selection-fixed-results.csv), [niezmieniony harness sceny](benchmarks/selection-continuation.cpp). Odtworzenie końcowego wariantu, po zbudowaniu `map_creature_refresh_test`:

```powershell
& docs/architecture/benchmarks/run-continuation.ps1 -BuildDirectory <build-Release> -QtPrefix <SDK-z-cache> -SelectionOnly
```

Historyczny harness IO z audytu dokumentuje stare błędy; po naprawach używać nowych testów CTest, a nie wymagać ponownego odtworzenia błędów.

## Ograniczenia i zgodność

Nie zmieniono formatu OTBM, map użytkownika ani wyglądu UI. Nowy build odczytuje stare sesje; stare EXE obsługujące tylko `session.json` nie znają nowych nazw manifestów. Przed downgrade trzeba obsłużyć oczekujące recovery w nowym buildzie; równoczesne działanie starego i nowego protokołu sesji nie było testowane.

Nie wykonywano testów na Linux/macOS, fizycznej awarii dysku/zasilania, wymuszonego ponownego użycia PID, sanitizerów ani pełnej interaktywnej sesji z prywatną mapą. Awarię procesu i błąd zapisu manifestu odtworzono na kontrolowanych fixture; QSaveFile/QLockFile nie stanowią transakcji odpornej na każdą awarię sprzętu. GUI i renderer mają testy regresji, ale pomiar zaznaczania nie obejmuje prezentacji klatki GPU ani bindingów całego okna QML.
