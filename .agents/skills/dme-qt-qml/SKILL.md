---
name: dme-qt-qml
description: >-
  Implementuj lub diagnozuj integrację Qt 6 i QML w DME, modele, bindingi, sygnały, motywy, fokus i okna Preview. Używaj dla zachowania GUI i granicy C++–QML, nie do samego profilowania GPU lub pakowania.
---

# Qt/QML w DME

Korzystaj z [ARCHITECTURE.md](../../../ARCHITECTURE.md), zwłaszcza przepływu C++–QML i mapy testów. Zachowaj moduł `Tibia 1.0`, singleton `Backend` i warstwę komponentów `Dme*`.

## Ustal kontrakt przed edycją
- Prześledź właściciela stanu: `Backend`/model C++, ustawienia w `controllers/`, stan lokalny komponentu lub Preview. Nie twórz drugiej kopii stanu tylko do odświeżenia kontrolki.
- Dla `Q_PROPERTY` sprawdź getter/setter/NOTIFY, emisję przy rzeczywistej zmianie i odbiorców. Nie zrywaj bindingu przypadkowym przypisaniem imperatywnym.
- `Backend::create()` zwraca instancję C++ o istniejącym czasie życia i wątku silnika. Nie twórz osobnego singletonu produkcyjnego na wzór mocka z testów.
- Nowy typ C++ z QML sprawdź w `qt_add_qml_module(SOURCES ...)`; nowy widok lub asset QML w `editor/resources.qrc`. Nie dopisuj wymyślonego modułu ani nie przenoś całej aplikacji do innego mechanizmu rejestracji.

## Implementacja
- Typuj nowe właściwości, gdy kontrakt jest znany, ale respektuj celowo ogólne interfejsy `var`. Nie zmieniaj istniejących API wyłącznie dla preferencji stylistycznej.
- Kosztowne operacje i skanowanie mapy pozostaw w odpowiedniej usłudze C++. Bindingi/delegaty mają przeliczać tylko potrzebny stan; sprawdzaj nadmierne resety modeli, tworzenie delegatów i nieaktywne timery.
- Używaj istniejących `style/`, `themes/` i tokenów motywu. Zmiana współdzielonej kontrolki dotyczy też innych motywów i okien.
- Zachowuj `required` properties, aliasy i `objectName` używane w testach, chyba że świadomie zmieniasz kontrakt wraz z jego odbiorcami.
- Publikuj modele i GUI w ich wątku; worker nie modyfikuje `QQuickItem` ani modelu używanego przez QML.

## Weryfikacja zachowania
Dobierz istniejący `tests/qml/tst_*.qml` i sprawdź importy w `tests/qml/imports/Tibia`. Obejmij zmieniony przepływ: mysz, klawiatura/Escape, fokus, popupy, enabled/disabled i resize; dla Preview także dock/float i niezależność ustawień mapy. Kontroluj warningi QML oraz binding loops.

Natywny `tibia_ui_qml_test` dodaje image provider, ale importuje mocki. Sam CTest `tibia_ui_qml` uruchamia tylko test motywu. Dla innych plików wybierz jawne `-input` i właściwe `-import`; ścieżkę programu odczytaj z drzewa builda. Nie traktuj testu software jako potwierdzenia kompozycji QRhi, DPI czy osobnego okna na rzeczywistym urządzeniu. Dla zmienionego UI sprawdź również właściwy build, jeśli środowisko pozwala; w przeciwnym razie wskaż brak.
