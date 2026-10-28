# Konkretna przyczyna opóźnienia startu UI — 2026-10-27

> Raport historyczny sprzed refaktoryzacji. Stan po wdrożeniu: [2026-10-28](startup-implementation-2026-10-28.md).

## Wniosek

Problem można odtworzyć **bez mapy, bez DAT/SPR/OTB i z pustą historią timera**. Główne QML ma duży graf zależności, obejmujący także nieotwarte narzędzia. Nieaktywne Loadery używają inline sourceComponent: odraczają tworzenie okien, ale nie przygotowanie ich typów jako zależności Main.qml. Jednocześnie badany build nie kompiluje widoków QML podczas budowania. Gdy nie ma aktualnego cache, koszt ten przypada na start aplikacji.

To ważniejsze ustalenie niż wcześniejsza liczba obiektów. Trzy kopie popupów timera są rzeczywistym dodatkowym kosztem, ale **nie wyjaśniają samodzielnie kilkusekundowego opóźnienia**.

## Dowody w kodzie

1. [editor/main.cpp:142](../../editor/main.cpp#L142) wywołuje engine.load całego Main.qml przed wejściem do głównej pętli zdarzeń. Ekran startowy jest częścią tego samego głównego komponentu: [editor/qml/Main.qml:1429](../../editor/qml/Main.qml#L1429). Nie ma lekkiego, niezależnego punktu wejścia pokazującego wyłącznie ekran startowy.
2. [editor/qml/Main.qml:1021](../../editor/qml/Main.qml#L1021), [editor/qml/Main.qml:1119](../../editor/qml/Main.qml#L1119), [editor/qml/Main.qml:1276](../../editor/qml/Main.qml#L1276) oraz pozostałe Loadery w Main.qml zawierają `active:false` i `sourceComponent: TypOkna { ... }`. Zidentyfikowano 30 takich deklaracji, w tym 9 ciężkich narzędzi: biblioteka proceduralna, TileMapGen, custom generator, Lua, AI, terrain, ground clusters, dungeon i edytor brushy.
3. Główna paleta, workspace i pasek statusu także są deklarowane bezwarunkowo: [editor/qml/Main.qml:544](../../editor/qml/Main.qml#L544), [editor/qml/Main.qml:863](../../editor/qml/Main.qml#L863), [editor/qml/Main.qml:883](../../editor/qml/Main.qml#L883). Ukrycie głównego okna przez visibility/opacity nie usuwa kosztu przygotowania jego wnętrza.
4. [CMakeLists.txt:229](../../CMakeLists.txt#L229) rejestruje przez SOURCES typy C++; widoki są w [CMakeLists.txt:208](../../CMakeLists.txt#L208). Brakuje QML_FILES i reguł qmlcachegen w istniejącym build.ninja. Samo umieszczenie QML w QRC nie jest kompilacją jego kodu do bytecode podczas builda.
5. [editor/qml/components/WorkTimerStatus.qml:196](../../editor/qml/components/WorkTimerStatus.qml#L196) deklaruje ciężki popup i własny ActivityDialog, a różne warianty pasków tworzą trzy WorkTimerStatus. Trzy nieotwarte zestawy popupów powstają przy starcie.

Qt 6.10 opisuje generowanie kodu podczas builda, fallback do kompilacji przy uruchomieniu i automatyczny cache: [QML Disk Cache](https://doc.qt.io/qt-6.10/qmldiskcache.html). Cache jest używany tylko przy zgodności wersji Qt i źródeł. Nie znaleziono w DME kodu celowo wyłączającego lub kasującego cache QML.

## Jak odizolowano przyczynę

Osobny program diagnostyczny używa istniejących obiektów Release DME, pełnego produkcyjnego QRC, usług Backend i singletonu C++. Nie modyfikowano editor/, libs/, CMake ani DME.exe. Wszystkie konta, ustawienia, recovery i baza diagnostyki były izolowane; brak otwierania map i brak pobierania aktualizacji z internetu.

Nowa instrumentacja rozdziela:

- `qmlCompileMs`: wstępne ładowanie QQmlComponent — importy, rozwiązywanie zależności, cache i ewentualna kompilacja. To nie wyłącznie czas generowania bytecode CPU.
- `qmlInstantiateMs`: engine.load po przygotowaniu tego samego komponentu; obejmuje tworzenie obiektów, bindingi, onCompleted i pracę z platformą.
- `qmlCreateMs`: oba etapy łącznie. Nie obejmuje Windows loadera/DLL przed main.

W testowych kopiach podmieniano wyłącznie deklaracje UI. Kontrola i warianty porównawcze były umieszczone w jednym binarnym RCC, z różnymi prefixami qrc. Dzięki temu nie porównywano odczytu plików z dysku z odczytem zasobów EXE. Wstępne próby z kopią file:// miały znacznie większy narzut; wyłączono je z tabeli wynikowej.

Zmiana sourceComponent na source w eksperymencie pomija konfigurację używaną dopiero po otwarciu narzędzia. **To wariant diagnostyczny, nie gotowa poprawka funkcjonalna.** Nie otwierano w nim narzędzi. Rzeczywista poprawka musi zachować required properties, bindingi, sygnały i cykl życia.

## Wyniki przy pustym cache, ten sam Tibia-UI i RCC

Software/offscreen; mediana trzech nowych procesów. Systemowy cache plików nie był czyszczony.

| Wariant | Przygotowanie QML | Tworzenie obiektów | Razem | Obiekty |
|---|---:|---:|---:|---:|
| Pełny interfejs | 2550,7 ms | 488,6 ms | 3039,4 ms | 18 276 |
| Odroczone definicje 9 ciężkich narzędzi | 2124,0 ms | 528,8 ms | 2652,8 ms | 18 267 |
| Odroczone definicje 30 ukrytych okien | 1717,7 ms | 461,6 ms | 2184,7 ms | 18 246 |
| Pasek timera bez jego popupów | 2429,1 ms | 381,0 ms | 2802,5 ms | 13 734 |

Odroczenie 30 definicji zmniejsza czas łączny o **około 855 ms / 28%** przy prawie tej samej liczbie utworzonych obiektów. To bezpośredni dowód kosztu przygotowania nieotwartych typów, a nie kosztu mapy czy dużej historii. Usunięcie popupów timera eliminuje 4542 obiekty, ale daje mniejszą zmianę całego startu.

Pierwsza próbka pełnego interfejsu była odstająca: 6124 ms, kolejne 3039 i 2869 ms. Pozostałe wnioski opierają się na medianach i kontroli źródeł; nie są gwarancją konkretnego czasu na innym komputerze.

## Cache i kontrola na natywnym Windows/QRhi

Dla niezmienionego pełnego produkcyjnego QRC, w osobnych procesach, wskazano jeden własny katalog cache. Offscreen: pierwszy przebieg **3841 ms**, następne **742 i 705 ms**. Powstało 156 plików cache; liczba obiektów pozostała 18 276. Spadek dotyczył przede wszystkim przygotowania QML: 3426 ms → 296 / 276 ms.

Następnie użyto pluginu windows i tego samego domyślnego backendu **OpenGL QRhi 3.3**, bez mapy:

| Próba | Przygotowanie QML | Tworzenie / platforma | Razem |
|---|---:|---:|---:|
| Nowy pusty cache | 2208 ms | 1597 ms | **3806 ms** |
| Kolejny proces, ten sam cache | 235 ms | 1349 ms | **1584 ms** |
| Kopia istniejącego cache użytkownika | 1094 ms | 1102 ms | **2195 ms** |
| Kolejny proces na tej kopii | 217 ms | 1285 ms | **1502 ms** |

Oryginalny cache użytkownika istniał i zawierał 190 plików. Był wyłącznie odczytany i skopiowany do własnego katalogu eksperymentu. Zatem **nie ma dowodu, że DME w ogóle nie zapisuje cache**. Kopia wymagała częściowego przygotowania jednostek; nie ustalono osobno, które były brakujące lub nieaktualne. Zmiana źródeł/Qt może wymagać ponownego przygotowania kodu — nie należy uzależniać akceptowalnego startu wyłącznie od rozgrzanego cache.

Dodatkowe natywne porównanie wariantów RCC: po pierwszej próbie rozgrzewającej, mediany kolejnych trzech procesów wyniosły 1403 ms dla pełnego UI, 1297 ms bez popupów timera i 1300 ms z odroczonymi 30 definicjami. Wpływ odraczania kompilacji jest znacznie większy bez cache; po rozgrzaniu pozostaje koszt tworzenia dużego UI i platformy. Zmienność próbek nie pozwala obiecać dokładnie 100 ms poprawy.

Natywny pierwszy frameSwapped był zaobserwowany w 15 z 16 prób. Jedna próba z pustym cache nie zgłosiła sygnału w krótkim oknie obserwacji. Nie raportuję tego jako czasu pełnej klatki mapy ani dokładnego czasu od kliknięcia produkcyjnego EXE. Natywny harness ma diagnostyczne ustawienia, fonty i sieć; nie jest pełnym uruchomieniem ze stanem użytkownika.

## Kolejność właściwej poprawki

1. **Rzeczywiście odroczyć ładowanie kodu ukrytych narzędzi.** URL w Loaderze lub mały osobny komponent-wrapper ładowany przez URL; konfiguracja przez setSource/onLoaded/Connections, z zachowaniem obecnych kontraktów. Sama flaga asynchronous lub visible:false nie rozwiązuje opisanej zależności typów.
2. **Dodać build-time kompilację QML** w istniejącym qt_add_qml_module, z zachowaniem aliasów zasobów i importów. Najpierw poprawić wykryte kolizje QML, m.in. reset w AdvancedBrushEditor. Przetestować cache hit i cache miss, wszystkie motywy oraz paczkę wydania. Nie dodawać wymuszenia korzystania z nieaktualnego cache.
3. **Odciążyć start ekranu powitalnego.** W pierwszej kolejności pokazać lekki ekran; ciężkie wnętrze edytora załadować dopiero wtedy, kiedy jest potrzebne, z użyciem istniejących usług. Nie zmieniać projektu graficznego ani renderera.
4. **Jeden host popupów Work timera**, tworzony przy użyciu, zamiast trzech nieotwartych zestawów.

Nie wdrażano tych zmian w produkcyjnym UI w zadaniu diagnostycznym. Samo mechaniczne skopiowanie wariantu eksperymentalnego zepsułoby późniejsze otwieranie narzędzi.

## Artefakty i sprawdzenie

- [31 istotnych prób CSV](benchmarks/startup-ui-cause-results-2026-10-27.csv); wyniki z zimnym i ciepłym cache są oznaczone i należy rozpatrywać je osobno.
- [Harness](benchmarks/startup-audit.cpp), [generator wariantów](benchmarks/make-ui-startup-variants.py), [sposób odtworzenia](benchmarks/README-startup.md).
- Łącznie 36 uruchomień diagnostycznych, w tym 5 wstępnych prób file:// / bazowego QRC; 16 prób natywnych. Wszystkie zakończyły się poprawnie i potwierdziły sprzątnięcie własnych danych. Globalne ostrzeżenie o AdvancedBrushEditor jest odrębne od pustej listy QQmlEngine::warnings.
- Nie zmieniono źródeł aplikacji, oryginalnego cache ani ustawień/map użytkownika. Nie uruchamiano całego zestawu regresji aplikacji, ponieważ nie wdrażano poprawki. Nie testowano dużej mapy ani pełnej pierwszej klatki edytora.
