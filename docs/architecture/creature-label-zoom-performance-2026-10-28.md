# Etykiety stworzeń: koszt zmiany zoomu

## Potwierdzona ścieżka i zmiana

`MapOverlay.qml` przelicza rozmiar logiczny `IngameMonsterOverlay` przy zmianie zoomu. Dotychczas osobne zmiany szerokości, wysokości i środka wywoływały synchroniczne zapytania `MapView::previewMonsterLabels`. Każdy wynik był dodatkowo serializowany przez `JSON.stringify`, również z adresami PNG base64. Ukrycie poniżej progu usuwało model, a powrót odtwarzał wszystkie delegaty.

`IngameMonsterOverlay.qml` łączy odświeżenia geometrii przez `Qt.callLater`, ponownie wykorzystuje pobrany prostokąt, zaokrągla zakres zapytania do 16 kafelków i zachowuje ostatni model po ukryciu. Zapytania nadal podlegają limitowi 512 kafelków na oś w C++; cache obejmuje jeden poprzedni obszar, nie historię widoków. Po zmianach danych, piętra, readerów i atlasu cache jest unieważniany także przy ukrytej nakładce. Delegaty poza kadrem są niewidoczne, aby dodatkowy obszar cache nie tworzył pasków przy krawędzi ekranu. Porównanie poszczególnych ról w `syncLabels` zastępuje serializację całej listy.

`CreatureInformation.qml` otrzymuje flagę NPC od modelu nakładki edytora: pasek NPC ma kolor #66CCFF, pozostałe #00C000. Rozmiary, próg zoomu, bitmapy i QRhi pozostają bez zmian.

## Pomiar i ograniczenia

Ten sam runner Release Qt 6.10.2, offscreen, scena syntetyczna z 1000 etykietami, 40 kroków oddalenia 100→30%, 40 przybliżenia 30→100%, 10 ukryć/pokazań, 5 prób po przygotowaniu modelu. Wyniki dotyczą czasu całej sekwencji obsługiwanej przez QML, a nie pojedynczej klatki GPU. Fixture ma puste bitmapy: izoluje geometrię, model i tworzenie delegatów; nie mierzy dekodowania prawdziwych nazw, skanowania prywatnej mapy ani kosztu QRhi.

| Próba | Przed, ms | Po, ms | Zapytania przed/po |
|---|---:|---:|---:|
| 1 | 7797 | 4054 | 144 / 4 |
| 2 | 7910 | 4668 | 144 / 4 |
| 3 | 7528 | 4668 | 144 / 4 |
| 4 | 7543 | 4175 | 144 / 4 |
| 5 | 6971 | 4458 | 144 / 4 |

Mediana 7543→4458 ms (~41% krótszy test). To pięć prób w jednym procesie dla każdego wariantu, nie niezależne uruchomienia aplikacji. Nie deklaruje poprawy FPS ani usunięcia wszystkich skoków czasu klatki na mapie użytkownika.

## Weryfikacja

- Release `DewralMapEditor`, istniejący build Ninja/MinGW.
- Natywne testy `tst_monster_labels.qml`: 13 zaliczonych (w tym kolor NPC poprzez model, ponowne użycie delegatów, unieważnienie przy edycji podczas ukrycia, łączenie zmian geometrii, odrzucanie etykiet poza kadrem).
- Natywne `tst_editor_creature_labels.qml`: 9 zaliczonych; próg zoomu, toggle i brak zapytań przy ukryciu.
- Ryzyko: ostatni szerszy obszar pozostaje w pamięci podczas przybliżania/ukrycia. Jest zastępowany po wyjściu kamery poza cache lub unieważnieniu danych. Pierwsze utworzenie wielu widocznych etykiet nadal kosztuje; docelowy pomiar GPU i percentyli klatek na rzeczywistej mapie pozostaje do wykonania.
