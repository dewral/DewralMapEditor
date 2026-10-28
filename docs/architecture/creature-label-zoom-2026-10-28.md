# Minimalny zoom nazw stworzeń — 2026-10-28

W `Preferences → Interface → Creature names` dodano suwak `Minimum zoom` odpowiadający istniejącym ustawieniom tooltipów i oświetlenia. Domyślnie 25%, zakres 0–100%, krok 5%; 0% oznacza brak ograniczenia. Wartość jest właściwością istniejącego `AppSettings` (Qt Settings), bez osobnego magazynu konfiguracji.

`MapOverlay` uzależnia widoczność etykiet od tego progu oraz flagi `showCreatures`. Granica jest włącznie: przy 25% nazwy są widoczne, poniżej nie. Ukrywana jest grupa nazwy + pasek HP; sprite’y stworzeń nadal podlegają dotychczasowej fladze renderera. In-game Preview nie ma tego ograniczenia — jego wspólny komponent nie został zmieniony.

Ukrycie uruchamia istniejące czyszczenie modelu `IngameMonsterOverlay`, więc nie powstają delegaty i nie jest odpytywana lista etykiet przy przesuwaniu oddalonej mapy. Nie zmieniono zakotwiczenia ani skalowania tekstu przy widocznych nazwach. Nie deklarujemy konkretnego zysku FPS.

## Weryfikacja

- Release/AOT aplikacji zbudowany do osobnego `build/startup-implementation/dist-creature-zoom`.
- Etykiety edytora: 9/9 sprawdzeń, w tym progi 25/50%, 0%, master toggle, odtwarzanie etykiet i brak zapytań poniżej progu.
- Preferencje i suwak: 3/3, w tym aktualizacja wartości i zapis do ustawień.
- Wspólny komponent etykiet/Preview: 9/9, w tym dotychczasowe pozycjonowanie i pasek HP.
- Natywny OpenGL QRhi z rzeczywistymi AppSettings i widokiem: zoom 6,25% ukrywa etykiety przy pozostawionym `showCreatures=true`; 25% je włącza, rzeczywisty suwak działa i próg 50% wyłącza je przy 25%. `creatureZoomOk=true`, brak ostrzeżeń silnika QML. Fixture nie zawiera rzeczywistych outfitów stworzeń; pokrycie delegatów pochodzi z testów QML.
- Smoke na natywnym Windows przeszedł. Próba odziedziczająca `QT_QPA_PLATFORM=offscreen` zakończyła się kodem 0xc0000602; nie potwierdzono działania tego wariantu smoke na Windows.

Zachowano pozostałe lokalne zmiany. Nie publikowano wydania ani nie nadpisywano uruchomionego EXE.
