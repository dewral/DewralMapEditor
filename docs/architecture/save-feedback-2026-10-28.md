# Komunikat przed zapisem mapy — 2026-10-28

## Potwierdzona przyczyna

`DocumentController` wywoływał `OtbmReader::saveFile()` bezpośrednio po przygotowaniu klienta. `saveFileInternal()` wykonuje serializację OTBM, operacje plikowe i obsługę XML synchronicznie. GUI nie mogło w tym czasie pokazać nowego komunikatu; wcześniejszy toast informował dopiero po wyniku.

## Zakres poprawki

- Jeden `queueSave()` obsługuje Save, Save As i Save All. Modalny `SaveProgressDialog` pokazuje nazwę pliku i „Saving map…” przed rozpoczęciem właściwego zapisu. Nie ma udawanego procentowego postępu ani sztucznego opóźnienia czasowego.
- `DmeWindow::requestPresentedFrame()` nadaje token. `beforeSynchronizing` przechwytuje go w render thread, a `frameSwapped` zleca potwierdzenie na GUI. Stan współdzielony przechowuje wyłącznie atomiki; nie czyta obiektów QML na workerze. Sam poprzedni, zakolejkowany sygnał klatki nie uruchamia zapisu.
- Kontroler czeka na pasujący token i widoczny edytor, sprawdza tożsamość dokumentu oraz blokuje duplikaty i zmianę karty. W przypadku błędu znika komunikat, pojawia się informacja o przyczynie, a Save All nie zamyka aplikacji.
- Zminimalizowane/ukryte okno jest odrzucane z komunikatem o przywróceniu okna; plik nie jest wtedy zapisywany bez informacji. Zamknięcie aplikacji podczas oczekiwania na zapis jest blokowane.
- Nie zmieniono `OtbmReader`, formatu plików ani istniejącej obsługi bezpieczeństwa zapisów i odzyskiwania.

**Ograniczenie:** właściwa serializacja i zapis nadal blokują GUI. To poprawka informacji i kolejności operacji zgodna ze zgłoszeniem, nie pełny zapis w tle. Przy długim zapisie komunikat pozostaje statyczny. Pełna asynchroniczność wymaga osobnej kontroli snapshotów, zmian dokumentu w czasie zapisu, pamięci i kolejności publikacji plików.

## Weryfikacja

- Release/Qt 6.10.2/MinGW skompilowany z QML AOT.
- `tst_save_feedback.qml`: 13/13, w tym Save/Save As, dwie mapy Save All, niewłaściwy/stary token, duplikat Ctrl+S, błąd klienta/writera, zmiana dokumentu i minimalizacja.
- `dme_window` i `dme_window_scaled`: element testowy musi przejść przez `updatePaintNode` przed potwierdzeniem tokenu; sprawdzane także łączenie żądań i emisja potwierdzenia na GUI.
- CTest `otbm_rme_compatibility`, `otbm_legacy_subtypes`, `dme_startup`, oba testy okna: 5/5.
- Testy otwierania legacy map i sesji aplikacji: 6/6 oraz 5/5.
- Natywny OpenGL QRhi z rzeczywistymi zasobami i Backend: Save As przy widocznym komunikacie we wszystkich siedmiu motywach, brak ostrzeżeń silnika QML. Osobny przebieg Tibia-UI sprawdza także zwykły Save po Save As.
- Nie wykonano testu długiego zapisu prywatnej mapy ani osobnej walidacji D3D11/MSVC. Nie deklarujemy przyspieszenia samej serializacji.

Build jest w `build/startup-implementation/dist-save`; nie nadpisano uruchomionego EXE. Zachowano równoległe, niezwiązane modyfikacje repozytorium. Nie wykonywano push ani publikacji.
