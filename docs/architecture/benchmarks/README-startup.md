# Diagnostyka startu UI DME

Pliki to narzędzia audytu, nie część normalnego builda ani pakietu wydania:

- `startup-audit.cpp` — izolowany punkt wejścia, rzeczywisty Backend i pełny Main.qml, offscreen/software, pomiary konstrukcji UI i opcjonalnego profilu 10.98.
- `compile-startup-audit.py` — połączenie diagnostyki z istniejącymi obiektami aplikacji i QRC; wyklucza produkcyjny main.cpp.obj.
- `make-startup-fixtures.py` — tworzy syntetyczną historię 1000 i 10 000 sesji.
- `startup-results-2026-10-27.csv` — 15 zanonimizowanych prób; nie zawiera danych klienta, map, kont ani lokalnych ścieżek SDK.

## Ograniczenia

Compiler sprawdzono z Ninja, jedną konfiguracją Release, Windows/MinGW i Qt 6.10.2. Odczytuje build.ninja, CMakeCache i lokalizację kompilatora. Nie jest przenośnym zamiennikiem CMake; nie zakładaj działania z MSVC, Debug lub Ninja Multi-Config. Przed użyciem potrzebny jest aktualny build targetu DewralMapEditor, jego bibliotek i QRC. Samo ponowne linkowanie nie aktualizuje starych obiektów.

Program powinien znajdować się w tym samym katalogu co DME.exe: korzysta z tego samego runtime Qt i katalogu data. Nie uruchamia produkcyjnego main. Domyślny tryb offscreen nie mierzy loadera Windows/DLL, natywnej prezentacji okna ani gotowości QRhi; opcjonalny test natywny opisano niżej. Użycie software dotyczy wyłącznie procesu diagnostycznego.

Każdy przebieg ma własną nazwę aplikacji UUID i dane testowe, osobne INI oraz recovery. Przed użyciem katalogów danych sprawdza, że nie istnieją, a po zakończeniu usuwa wyłącznie własne katalogi o tej nazwie. Nie korzysta z konfiguracji Activity użytkownika. Sieć jest skierowana do lokalnego listenera odrzucającego połączenia. W razie awarii nie usuwaj zbiorczo katalogów standardowych Qt.

## Odtworzenie pojedynczej próbki

Polecenia wykonuj z katalogu głównego repozytorium. `python` oznacza dostępny interpreter Python 3; `QtPrefix` ustaw na prefix zgodny z badanym buildem. Aktualny audyt używał katalogów build/DME-build i build/DME-dist — to sprawdzone lokalne drzewo, nie obowiązkowa nazwa dla wszystkich konfiguracji.

```powershell
# Ustaw wcześniej $QtPrefix na prefix Qt użyty przez ten build.
python docs/architecture/benchmarks/compile-startup-audit.py build/DME-build "$QtPrefix" build/DME-dist/DME-startup-audit.exe
if ($LASTEXITCODE -ne 0) { throw 'Kompilacja diagnostyki nie powiodła się' }
python docs/architecture/benchmarks/make-startup-fixtures.py build/architecture-audit/startup

$runRoot = (Resolve-Path build/architecture-audit/startup).Path
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_QPA_PLATFORM_PLUGIN_PATH = "$QtPrefix/plugins/platforms"
$env:QT_FORCE_STDERR_LOGGING = '1'
& ./build/DME-dist/DME-startup-audit.exe $runRoot fluent-dark - 1.1.43 - > "$runRoot/manual.json" 2> "$runRoot/manual.log"
```

Uruchamiaj w osobnej sesji terminala albo przywróć poprzednie wartości zmiennych Qt po badaniu. Jeśli runtime nie jest dostępny przez deploy/PATH, użyj tego samego środowiska Qt i MinGW, w którym zbudowano aplikację.

Argumenty EXE: katalog wyników/fixture, motyw, plik historii lub `-`, wersja aplikacji, folder klienta lub `-`. Dla historii podaj plik history-1000.json albo history-10000.json wygenerowany w katalogu pomiaru. Dla motywu Tibia podaj tibia-ui. Wersja 1.1.43 to wersja badanej konfiguracji — przy innym buildzie podaj jego wersję.

Opcjonalny ostatni argument odczytuje profil **10.98** z podanego folderu przez rzeczywisty kontroler QML. Nie otwiera mapy i nie zastępuje pomiaru pełnego atlasu ani GPU. Nie podawaj folderu innego protokołu, oczekując automatycznej detekcji tego testu.

Powtarzaj w nowych procesach, bez współbieżnych prób. Zachowaj w raporcie cache, motyw, fixture, build i tryb uruchomienia. Pole createdObjects to osiągalne QObject/elementy wizualne, nie liczba widocznych widgetów; warningCount obejmuje sygnał silnika, a nie wszystkie globalne komunikaty Qt. clientDefinitions oznacza liczbę definicji DAT. startupFrameFromQmlBeginMs nie jest czasem natywnej prezentacji; -1 oznacza brak zaobserwowanego sygnału.

Usuń wyłącznie utworzony przez siebie DME-startup-audit.exe z katalogu dystrybucji po badaniu, aby nie trafił do wydania. Surowe logi i prywatne pliki klienta pozostaw poza śledzonymi plikami. Raport zawiera wartości bazowe, nie wykazane przyspieszenie.

## Drugi etap audytu: kompilacja a tworzenie UI

Aktualny harness wstępnie ładuje QQmlComponent, zapisuje qmlCompileMs, a następnie mierzy engine.load tego samego URL jako qmlInstantiateMs. qmlCreateMs nadal obejmuje oba etapy. qmlCompileMs zawiera odczyt/rozwiązywanie typów i importów, walidację/cache oraz ewentualną kompilację; nie jest wyłącznie czasem generowania bytecode CPU. To zmiana instrumentacji względem pierwotnego CSV startup-results.

Opcjonalny szósty argument po nazwie programu to lokalny Main.qml lub URL qrc; kolejny to plik RCC rejestrowany przez QResource. Przykład kolejności: OUTPUT tibia-ui - VERSION - qrc:/audit/local-full/Main.qml VARIANTS_RCC.

make-ui-startup-variants.py tworzy nowe, oddzielne kopie QML i variants.qrc. Wymaga nieistniejącego katalogu wynikowego; nie edytuje editor/qml. Sprawdza oczekiwane 9/30 dopasowań, żeby zmiana źródeł nie przeszła niezauważona. variants.qrc kompiluj tym samym rcc --binary co pozostałe zasoby Qt. Wszystkie porównywane kopie używają tego samego RCC i odrębnych prefixów /audit/NAME.

Warianty url-generators/url-dialogs zastępują deklaracje nieaktywnych sourceComponent przez source URL i pomijają konfigurację używaną dopiero przy otwieraniu narzędzia. no-timer-popup pozostawia pasek timera, ale usuwa popupy i daje pustą metodę open. **To eksperymenty przyczynowe startu, nie poprawki gotowe do użycia.** Nie otwieraj narzędzi w tych wariantach i nie kopiuj ich do produkcji. Prawdziwa implementacja musi odtworzyć required properties, bindingi, sygnały i cykl życia.

Aby osobno sprawdzić cache, ustaw QML_DISK_CACHE_PATH na własny katalog eksperymentu: pierwszy przebieg zapisuje cache, kolejne go używają. Nie kasuj ani nie nadpisuj cache użytkownika. Porównując zimne warianty, każdy musi zaczynać bez własnych wpisów; przy porównaniu ciepłym najpierw rozgrzej każdy wariant i raportuj następne próbki osobno.

DME_STARTUP_AUDIT_NATIVE ustawione w środowisku włącza natywne okno i OpenGL QRhi 3.3, zamiast software. Ustaw wtedy QT_QPA_PLATFORM=windows. Flaga jest sprawdzana przez isSet: usuń ją z osobnej sesji, żeby wrócić do software. Test pokazuje własny ekran startowy i zamyka go automatycznie; nadal nie ładuje mapy ani ustawień użytkownika. To nie dokładna kopia produkcyjnego main (m.in. diagnostyczne fonty, sieć i profil). startupFrameFromQmlBeginMs to pierwszy zaobserwowany frameSwapped dowolnego okna testu; -1 oznacza brak obserwacji, nie błąd renderera. Nie jest dowodem pełnej klatki mapy.

Wyniki drugiego etapu: startup-ui-cause-results-2026-10-27.csv; trial 1 w grupach native-rcc oznacza pierwszy przebieg, trial 2–4 — ponowne użycie cache. W qrc-shared-cache i native-qrc-new-cache pierwszy przebieg jest zimny. native-qrc-user-cache-copy zaczyna od prywatnej kopii istniejącego cache użytkownika; oryginał był tylko odczytywany.


## Wdrożenie 2026-10-28

Aktualny `Main.qml` jest lekką powłoką; opis historycznego pełnego Main powyżej dotyczy bazowego audytu. Opcjonalny folder klienta przygotowuje teraz `ClientLoadService` w tle. `DME_STARTUP_AUDIT_MAP` zawiera ścieżkę syntetycznej mapy 8.60 i uruchamia pełne `profileOpen` powłoki. Pole `completeMapFromQmlBeginMs` mierzy czas do `editorPresented` po zakończeniu ładowania i pierwszej klatce. `completeMapMs` zaczyna się dopiero przy żądaniu otwarcia; nie porównuj go z całym startem.

`client_load_service_test --write-fixture OUTPUT` tworzy własny nowy katalog z syntetycznymi DAT/SPR/OTB/OTFI i mapą 4096 kafelków. Nie używa prywatnych zasobów. Wariant `--write-versioned-fixture OUTPUT` dodaje do OTB poprawny nagłówek wersji, wymagany w teście tworzenia i zapisu nowego dokumentu. Fixture pomiarowa pozostaje bez tego dodatkowego nagłówka; mapowa wersja 8.60 pochodzi z jej OTBM. `DME_STARTUP_AUDIT_TOOLS` otwiera/zamyka dwukrotnie 30 wrapperów; `DME_STARTUP_AUDIT_TIMER` otwiera wspólny popup i historię, porównując liczbę wierszy UI z rzeczywistym modelem. `DME_STARTUP_AUDIT_DOCUMENTS` sprawdza nowy dokument, zapis do własnego katalogu tymczasowego i powrót do poprzedniej karty. To diagnostyka funkcjonalna, której nie włączaj w serii pomiaru startu.

`measure-startup.ps1 -BeforeExe BEFORE -AfterExe AFTER -OutputRoot NEW_DIRECTORY -ClientFixture FIXTURE` uruchamia po pięć par cold/warm dla powitania i mapy. Wymaga odpowiedniego runtime w środowisku. Nie kasuje istniejących katalogów cache. Parametr `-Scenarios map` ogranicza serię do pierwszej mapy. Zamknij procesy kompilacji i testów przed pomiarem. Oba EXE muszą być zbudowane z pasującymi nagłówkami/obiektami, tym samym Qt i identycznymi definicjami profilu. Aktualnego źródła odwołującego się do `ClientLoadService` nie linkuj ze starym ABI Backend.

Wyniki wdrożenia: [CSV](startup-implementation-results-2026-10-28.csv) i [raport](../startup-implementation-2026-10-28.md). Rozgrzanie dotyczy cache QML, nie dowodu zimnego dysku/systemu. Próbki z niezauważonym welcome frame zachowują −1; nie usuwaj ich arbitralnie ani nie zastępuj zerem.
