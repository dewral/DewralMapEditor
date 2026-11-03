# Stabilność obrazu i światła w In-game Preview

## Diagnoza

Przejrzano klatki całego 17,73-sekundowego nagrania użytkownika. Drobne falowanie tekstur podczas przesuwania wymaga ostrożnej interpretacji z powodu kompresji H.264 i skalowania podglądu. Nie przypisujemy wszystkich artefaktów nagrania jednemu mechanizmowi.

W `IngamePreviewScene::build` sprite’y otrzymywały pozycje o ułamkowej fazie piksela logicznego. Ponadto światła były próbkowane na siatce związanej z ekranem, podczas gdy ich pozycje przesuwały się z kamerą. Interpolowany wynik dla nieruchomego świata zmieniał się podczas chodzenia. Izolowany test przed zmianą zakończył się błędem `Preview camera motion resamples stationary world lights`.

Porównano źródła [OTClientV8 LightView](https://github.com/OTAcademy/otclientv8/blob/master/src/client/lightview.cpp) i [MapView](https://github.com/OTAcademy/otclientv8/blob/master/src/client/mapview.cpp). Podobny model zaniku i maksimum kanałów koloru nie był główną różnicą: klient używa wspólnego wycinka framebufferu i przesunięcia chodzenia dla mapy oraz tekstury światła. DME zachowuje QRhi i istniejący `MapRhiBackend`; nie przeniesiono implementacji OpenGL klienta.

## Poprawka

- `ingamepreviewscene.cpp`: wspólne wyrównanie kamery do pikseli logicznego render targetu, również dla postaci; próbki światła zakotwiczone w środkach kafelków świata.
- `ingamepreviewscene.h`: `IngamePreviewFrame::lightOrigin` opisuje położenie tekstury światła względem widoku w jednostkach kafelków.
- `ingamepreviewview.cpp`: renderer pobiera obraz światła i jego przesunięcie z tego samego snapshotu klatki i przekazuje właściwe UV do istniejącego shadera.
- Referencja CPU używa tego samego przesunięcia. Dodatkowy wiersz i kolumna światła zapewniają interpolację na krawędziach przy przesunięciu siatki.

Nie zmieniono UI, prędkości chodzenia, modelu mapy, kolejności sprite’ów ani architektury QRhi. Stałe wyrównanie kamery redukuje zmianę fazy próbkowania sprite’ów; nie jest interpolacją ruchu ani gwarancją wyeliminowania wszystkich artefaktów skalowania niecałkowitego.

## Weryfikacja

Release MinGW, Qt 6.10.2, Windows:

- `DewralMapEditor` i `map_creature_refresh_test`: kompilacja poprawna.
- `map_creature_refresh_test --preview-motion-only`: test przechodzi po poprawce. Sprawdza nieruchome światła przy ruchu dodatnim/ujemnym, przekroczenie granicy kafelka, wyrównanie sprite’ów i położenie siatki.
- `map_creature_refresh_test --preview-motion-gpu`, natywny QRhi OpenGL: 94 przechwycone klatki podczas chodzenia w trzech kierunkach, zgodność próbek obrazu z referencją CPU, tolerancja 3 poziomów kanału.
- Ten sam test QRhi D3D11: 95 przechwyconych klatek, test przechodzi. Różna liczba klatek wynika z czasu wykonywania i timerów, nie z innego scenariusza ruchu.
- CTest `map_creature_refresh`: przechodzi, 21,41 s; `map_goto_render`: przechodzi, 2,10 s. Pierwsze uruchomienie pełnego testu przed poprawką przekroczyło jego limit 30 s; błąd światła odtworzono następnie izolowanym testem, a końcowa pełna regresja przeszła.

Test GPU używa syntetycznych danych i wybranej centralnej siatki punktów, nie całej prywatnej mapy. Pozostaje wizualna ocena tej samej trasy na mapie użytkownika, zwłaszcza przy niecałkowitym skalowaniu Preview i różnych częstotliwościach odświeżania. Nie wykonano pomiaru wzrostu FPS.
