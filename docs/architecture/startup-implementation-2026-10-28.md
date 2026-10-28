# Wdrożenie architektury startu DME — 2026-10-28

## Wynik

Wdrożono lekką powłokę startową, URL-owe ładowanie edytora i 30 narzędzi, kompilację QML, jeden popup timera, model historii oraz asynchroniczne przygotowanie klienta. Zachowano C++17, singleton Backend, istniejące modele, formaty danych, wygląd i QRhi. Ten raport dotyczy refaktoryzacji startu; wspólny commit obejmuje także wcześniejsze zmiany robocze na wyraźne życzenie użytkownika.

## Zmierzone efekty

Release, Qt 6.10.2, MinGW 13.1.0, Windows, motyw Tibia-UI, natywny OpenGL przez QRhi. Każda komórka tabeli to mediana **pięciu nowych procesów**. Warianty uruchamiano kolejno, bez równoległego builda/testów. Pusty cache oznacza świeży prywatny katalog QML, nie opróżniony cache systemu operacyjnego. Rozgrzany przebieg wykorzystuje katalog poprzedniego przebiegu tej samej próby. Nie zmieniano cache użytkownika.

| Metryka | Przed | Po | Zmiana |
|---|---:|---:|---:|
| Pierwsza klatka powitalna, pusty cache | 3523,8 ms | 594,4 ms | −83,1% |
| Pierwsza klatka powitalna, rozgrzany cache | 1002,8 ms | 587,0 ms | −41,5% |
| Osiągalne obiekty przed żądaniem edytora | 18 276 | 895 | −95,1% |
| Working set przed żądaniem, pusty cache | 221,7 MiB | 146,2 MiB | −34,1% |
| Od rozpoczęcia QML do pełnej klatki mapy, pusty cache | 9540,3 ms | 6244,4 ms | −34,5% |
| Od rozpoczęcia QML do pełnej klatki mapy, rozgrzany cache | 7626,1 ms | 6022,5 ms | −21,0% |

Cel skrócenia zimnego ekranu powitalnego o 50% został przekroczony w tym scenariuszu. Wyniki mapy nie wskazują regresji ukrytej przez odroczenie edytora. Scena mapy jest syntetyczna: 4096 kafelków, klient 8.60, dziewięć definicji DAT i dziewięć pozycji OTB. Definicje profilu `data/860` w obu dystrybucjach były identyczne według SHA-256.

Surowe wartości bez ścieżek i danych użytkownika: [CSV](benchmarks/startup-implementation-results-2026-10-28.csv), 40 prób. Seria powitalna i seria pełnej mapy są osobnymi seriami czasowymi; nie należy sumować ich median. Harness ma wspólne dla obu wariantów 300 ms obserwacji po utworzeniu QML i liczenie obiektów przed żądaniem mapy. Czas pełnej mapy zawiera ten narzut. Mierzono sygnały Qt, nie moment fizycznego wyświetlenia piksela. Wyniki nie obejmują ładowania EXE/DLL przed wejściem do `main`.

W pięciu próbach serii mapowej nie zaobserwowano pierwszego `frameSwapped` welcome w krótkim oknie obserwacji (CSV: −1). Nie zastąpiono tych wartości zerami. Wszystkie 20 prób mapowych potwierdziło pełną klatkę mapy, wszystkie 20 powitalnych — pierwszą klatkę welcome, a silnik QML nie zgłosił w tych próbach ostrzeżeń.

## Podział odpowiedzialności i pliki

- `cmake/QmlFiles.cmake`, `CMakeLists.txt`, `editor/resources.qrc`: jawny manifest QML/JS, kompilacja AOT, zachowane adresy `qrc:/qml/...`, grafiki bez podwójnego pakowania QML. Metodę `AdvancedBrushEditor.reset` przemianowano na `resetDraft`.
- `Main.qml`, `EditorScene.qml`, `controllers/ApplicationSession.qml`, `StartupWindow.qml`: lekki start, wspólny katalog profili, bufor żądania open/new/recovery, URL-owy Loader edytora, rozróżnienie komponentu i pierwszej klatki. Edytor jest zachowywany po zamknięciu dokumentów.
- `components/LazyToolHost.qml`, `tools/*Tool.qml`: właściwości przed konstrukcją, bindingi konfiguracji, obsługa błędu, brak duplikatu i dotychczasowa retencja sesji proceduralnej. Wrappery zawierają konfigurację pierwotnych dialogów, a nie uproszczone zamienniki funkcjonalności.
- `core/clientloadservice.*`, `core/backend.*`, nagłówki czytników/stores: worker z lokalnymi czytnikami i przenoszalnym stanem. Nie przenosi QObject ani wskaźnika mapowania SPR. Publikacja zachowuje tożsamość modeli i poprzednie dane w przypadku błędu. Identyfikator żądania, anulowanie i walidacja fingerprintu odrzucają nieaktualny wynik. Backend niszczy usługę przed swoimi modelami.
- `ClientProfileController.qml`, `AppController.qml`, `DocumentController.qml`: kontynuacje dla operacji wymagających danych klienta. Katalog profili nie jest ponownie wczytywany przy tworzeniu edytora. Powtórne otwarcie tej samej mapy podczas przygotowania nie uruchamia drugiego parsera.
- `core/worktimerservice.*`: asynchroniczny `ensureLoaded()`, gotowość i błędy, `QAbstractListModel`, cache agregatów. Zamknięcie przed odczytem nie nadpisuje historii; zapis JSON i eksporty zachowane. Błąd można ponowić w popupie.
- `components/WorkTimerStatus.qml`, `WorkTimerPopupHost.qml`, `WorkTimerPopupContent.qml`, `WorkTimerExportDialog.qml`, status bar/workspace: trzy lekkie wskaźniki i jeden wspólny popup, również w osobnym Preview. Historia i Activity powstają dopiero po wejściu na stronę; wybór ikony oraz eksporty są ładowane przez URL.
- `editor/main.cpp`: diagnostyczne żądanie edytora przed podpięciem istniejących sond profilera; smoke testy mają własne ustawienia/dane i czekają na klatkę.

## Weryfikacja i ograniczenia

- Aplikacja Release i zmienione testy skompilowane, w tym wygenerowane jednostki `qmlcachegen`. Nie wymuszano ignorowania walidacji cache Qt.
- CTest: czytnik legacy DAT, OTFormats reader, paleta, ładowanie OTBM w tle, atlas w tle, klient, timer, odzyskiwanie i sesje dokumentów, creature store, manifest wydania, okno zwykłe i skalowane, start aplikacji, TileMapGen oraz start updatera.
- Test klienta używa 20 009 definicji DAT: publikacja, ciągłość obsługi zdarzeń GUI podczas przygotowania, stała tożsamość modeli, cache hit bez przebudowy obrazów, anulowanie/zastąpienie, błędny DAT, zmiana pliku podczas odczytu i niszczenie Backend w czasie zadania.
- Historia 0/1000/10 000 sesji: liczba i kolejność wierszy, zgodność wartości z legacy getterem, suma czasu, eksporty CSV/JSON i brak zapisu przy zamknięciu przed końcem odczytu. Przerwana aktywna sesja pozostaje wstrzymana.
- Produkcyjny tryb CLI profilera: dwa kolejne otwarcia, dwa sygnały `first_complete_frame`, exit 0 po oddzieleniu powłoki i edytora.
- Natywny build: otwarcie, nowy dokument, Save As i powrót do wcześniejszej karty przeszły na syntetycznym profilu z poprawnym nagłówkiem wersji OTB; pełna mapa z zerem ostrzeżeń QML; jeden popup z rzeczywistym modelem 10 000 sesji; 30 narzędzi otwieranych, zamykanych i ponownie otwieranych z zasobów aplikacji, w Tibia-UI i Fluent Dark. Test szybkiego zamknięcia narzędzi zgłasza również istniejące ostrzeżenia analizatora oraz anulowanej inkubacji Preferences; nie traktujemy go jako przebiegu bez ostrzeżeń.
- QML: ostatni pełny przebieg **699 passed / 10 failed**. Błędy dotyczą sześciu wariantów CreateCollection i czterech układów InlineWallBrushEditor; odtworzono je na źródłach sprzed wdrożenia. Wcześniejszy przebieg ujawnił także niestabilny test MapAnalyzer, odtworzony oddzielnie na kopii bazowej. Nie poprawiano niezwiązanych funkcji ani nie wyciszano asercji. Test timera między dwoma natywnymi oknami: 12/12.
- Paczka przenośna utworzona istniejącym skryptem CMake w oddzielnym katalogu: EXE po strip 16 335 887 B, ZIP około 55,1 MB. Z paczki uruchomiono start aplikacji, TileMapGen, Generation Library i updater (wszystkie exit 0), bez Qt/MinGW w `PATH` i bez zmiennych importu Qt. Pakowanie pomija lokalne `profile-settings` profilera (nowa asercja testu manifestu). Nie nadpisano użytkowanego `build/DME-dist/DME.exe`.
- Testy kontraktów QML nie zastępują testów GPU. Natywne próby dotyczą Windows/OpenGL QRhi. Nie zweryfikowano MSVC, D3D11, macOS ani Linux.
- Nie wykonano porównania na wszystkich rzeczywistych profilach klienta ani dużej prywatnej mapie. Brak podstaw, by deklarować identyczny procent przyspieszenia w tych scenariuszach.
- Podczas przygotowania istnieją jednocześnie poprzednie i nowe dane klienta. To koszt pamięci potrzebny do bezpiecznej publikacji i zachowania poprzedniego stanu po błędzie. Zamiana dużych kontenerów i sygnały resetu na GUI nadal wymagają pomiarów z największymi profilami.

## Odtwarzanie

[README pomiarów](benchmarks/README-startup.md) oraz [measure-startup.ps1](benchmarks/measure-startup.ps1) opisują osobne procesy, cache i izolację. Wariant bazowy pochodzi z kopii lokalnego drzewa sprzed wdrożenia, zawierającej wcześniejsze zmiany użytkownika; nie jest utożsamiany z `HEAD` GitHuba. Kopia, stare obiekty, diagnostyczne EXE i prywatne logi są artefaktami lokalnymi poza commitem.

Nie wykonywano push ani publikacji wydania. Wspólny lokalny commit zastępuje pierwotnie planowane commity etapów zgodnie z późniejszą decyzją użytkownika.
