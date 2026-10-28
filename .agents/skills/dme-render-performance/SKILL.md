---
name: dme-render-performance
description: >-
  Diagnozuj i optymalizuj wydajność mapy DME w QRhi/OpenGL: draw calle, uploady, atlas, chunki, cache, alokacje i opóźnienia klatek. Używaj przy lagach lub benchmarkach renderera; nie do kosmetyki QML ani ogólnego przeglądu bez celu wydajnościowego.
---

# Wydajność istniejącego renderera

Przeczytaj ścieżkę renderowania i ograniczenia pomiarów w [ARCHITECTURE.md](../../../ARCHITECTURE.md). OpenGL jest backendem QRhi, nie miejscem na osobny zestaw `gl*` omijający `MapRhiBackend`.

## Rozpoznanie i baseline
1. Ustal objaw i niezmienniki obrazu: kolejność kafelków/przedmiotów/pięter, blending, światło, selekcje i nakładki, animacje, atlas oraz osobne Preview.
2. Potwierdź backend, SDK/toolchain i Release/RelWithDebInfo bez pomylenia go z Debug. Zapisz mapę i wersję zasobów, viewport/DPI, piętro/zoom, ustawienia, trasę kamery, VSync/FPS limit i GPU. Nie dodawaj prywatnych danych sceny do repozytorium.
3. Zmierz powtarzane równoważne próby po rozgrzewce, osobno start, statyczną scenę, pan/zoom i edycję. Czasy CPU, GPU i oczekiwanie na present to różne wielkości. Raportuj rozkład czasów klatki, liczbę próbek i rozrzut, nie tylko średni FPS.

## Potwierdź przyczynę
- CPU: sprawdź `MapView`/workery chunków, `MapRhiRenderer::synchronize()`, generowanie instancji/list drawów i nakładek oraz uniformów. GUI jest zatrzymane podczas synchronizacji; dłuższy `synchronize()` nie jest darmową pracą w tle.
- GPU/QRhi: sprawdź w `MapRhiBackend` faktyczne drawy i passy, zmiany pipeline'ów/bindingów, rozmiar/częstotliwość uploadów i wzrost buforów/tekstur. Instancing, dirty buffers i cache już istnieją; rozmiar mapy nie dowodzi konkretnego bottlenecku.
- Pamięć: oddziel przejściowe alokacje CPU, staging, quad-cache i atlas od GPU. Badaj peak i stan ustalony; cache bez limitu nie jest automatycznie dobrą optymalizacją.
- Połącz pomiar ze ścieżką kodu. Niezmierzone przyczyny nazwij hipotezami; nie przypisuj im wymyślonych kosztów cyklowych lub procentowego zysku.

## Bezpieczna zmiana
Wybierz najmniejszą zmianę usuwającą wykazaną pracę: zbędną przebudowę, kopię, upload lub alokację. Zachowaj wersje/generacje i oddzielne unieważnianie sceny/metadata/pointer. Nie sortuj drawów po teksturze kosztem kolejności obrazu; nie zmieniaj precyzji, shaderów ani blendingu bez kontroli pikseli.

Zasoby QRhi pozostają w rendererze i na właściwym wątku. Zachowaj staging do odtworzenia urządzenia, wyrównanie/dynamiczne offsety uniformów, clip-space correction, orientację cache i niezależne snapshoty atlasu. Nie dodawaj do produkcyjnej klatki readbacku, `glFinish`, blokującego oczekiwania ani logowania każdej instancji. Pomiar i walidacja readbackiem mogą odbywać się poza ścieżką produkcyjną, z ujawnieniem narzutu.

## Porównanie i decyzja
Zbuduj dotknięte targety. Dobierz `map_rhi_backend_test`, `atlas_background_build_test`, `map_creature_refresh_test` i testy danych/QML z `ARCHITECTURE.md`; GPU sprawdzaj na dostępnych backendach. Wykonaj równoważny pomiar po zmianie i kontrolę obrazu/interakcji.

`scripts/profile-map-loading.ps1` mierzy ładowanie i pierwszą kompletną klatkę, nie płynność ciągłego panningu. Jeśli brakuje sceny, GPU lub baseline'u, przedstaw analizę i ograniczenia zamiast deklarować przyspieszenie. Cofaj tylko własny eksperyment przy potwierdzonej regresji; wynik w granicach szumu pozostaje nierozstrzygający.
