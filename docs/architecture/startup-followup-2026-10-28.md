# Dalsze odchudzenie startu — 2026-10-28

## Przyczyna i zmiana

Po pierwszej refaktoryzacji `StartupWindow.qml` nadal tworzył ukryte `NewMapDialog`, systemowe dialogi plików/katalogu oraz karty recovery i postępu. W natywnym pomiarze powstawało 895 obiektów; katalog motywów nie oznaczał siedmiu równocześnie utworzonych interfejsów.

Trzy dialogi są teraz ładowane przez istniejący `LazyToolHost` i osobne URL `StartupFolderDialog.qml`, `StartupOpenMapDialog.qml`, `StartupNewMapDialog.qml`. Właściwości są przekazywane przed konstrukcją. Instancje pozostają po pierwszym użyciu, zachowując stan i aktualny profil. Recovery i postęp są tworzone tylko przy aktywnej funkcji; ich istniejące komponenty i układ zostały zachowane. Nie zmieniono wyglądu, rendererów, formatów danych ani dostępnych motywów.

## Wyniki i granice wniosku

Release/Qt 6.10.2/MinGW/Windows, Tibia-UI, natywny OpenGL QRhi; pięć nowych procesów na wariant/cache. Pusty cache to osobny katalog QML, nie zimny cache dysku. Ta seria odbyła się przy pozostawionym uruchomionym DME użytkownika; nie porównuj jej bezpośrednio z wcześniejszą serią 0,59 s.

- Obiekty przy starcie: **895 → 537**, mniej o 40%.
- Mediana tworzenia QML: **659,1 → 640,9 ms** przy pustym cache i **703,2 → 657,1 ms** przy rozgrzanym.
- Mediana pierwszej klatki przy pustym cache: około **0,87 → 0,85 s**. Rozrzuty nakładają się; nie ma podstaw do ogłaszania dużego lub stabilnego przyspieszenia czasu pierwszej klatki.
- Jedna próbka rozgrzanego wariantu bazowego nie zaobserwowała pierwszej klatki w krótkim oknie harnessu; zachowano −1 w CSV. Nie zastąpiono jej zerem.

[Wszystkie 20 próbek](benchmarks/startup-followup-results-2026-10-28.csv) nie zawiera prywatnych ścieżek. Przy tej wielkości UI dalsze zmiany należy poprzedzać pomiarem pozostałych etapów; nie zakładać, że usunięcie motywów rozwiąże czas inicjalizacji okna.

## Weryfikacja

- Kompilacja Release z AOT; jawny manifest zawiera nowe URL.
- `tst_startup_window.qml`: 6/6. Brak obiektów ukrytych funkcji na wejściu, ponowne otwarcie New Map bez duplikatu, binding profilu i tytułu katalogu, kontekst dialogów plików, anulowanie oraz przełączanie recovery/progress. Mocki zgłaszają brak testowej ikony QRC i ograniczenia platformy offscreen; nie traktujemy ich jako testu kompozycji natywnej.
- Natywny harness z produkcyjnym Backend i QRC: utworzenie trzech dialogów i otwarcie New Map we wszystkich siedmiu motywach, bez ostrzeżeń silnika QML. Nie otwierano systemowych pickerów podczas automatycznego testu.
- Natywny przepływ otwarcia mapy, nowego dokumentu, zapisu i powrotu do karty: exit 0, kompletna klatka i brak ostrzeżeń QML.
- CTest `dme_startup` i `tilemapgen_dialog`: 2/2.

Build zapisano osobno w `build/startup-implementation/dist-pass2`. Nie nadpisano uruchomionej instancji ani screenshotu dodanego przez użytkownika. Nie wykonywano push ani publikacji.
