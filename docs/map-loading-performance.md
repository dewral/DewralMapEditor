# Ładowanie mapy: wdrożenie i pomiary

## Wdrożone zachowanie

- Paleta przygotowuje komplet informacji o widoczności, korzystając z dotychczasowej kompozycji warstw i kafelków. Nie koduje wszystkich miniatur do PNG. Przezroczyste i całkowicie czarne obrazy nadal są ukrywane; bardzo ciemna grafika pozostaje widoczna.
- `image://paletteitem/<clientId>/<generation>` wymusza asynchroniczne pobieranie obrazu. Miniatura powstaje na żądanie, a cache LRU ma limit 32 MiB rzeczywistych danych QImage. Powtarzane żądania tego samego obrazu dzielą wpis cache. Kolejka Qt obsługuje żądania poza GUI; każdy wątek żądający ma własny dekoder SPR i niezmienny snapshot potrzebnych definicji DAT. Zmiana profilu odrzuca stare wyniki.
- Zgodny atlas pozostaje między dokumentami, także podczas przejścia przez pusty dokument. Zgodność obejmuje kanoniczne ścieżki, rozmiary i daty modyfikacji DAT/SPR/OTB oraz wersję i flagi dekodowania. Dodawane są tylko brakujące sprite’y. Gdy przewidywany atlas przekroczy 256 MiB, jest budowany dla bieżącej mapy zamiast kumulowania poprzednich map. Sama bieżąca mapa wymagająca więcej niż 256 MiB nie jest obcinana.
- Granice pięter powstają razem z indeksem chunków. Dodawanie kafelka rozszerza granice; strukturalne undo/redo unieważnia granice odpowiedniego piętra. Ponowny odczyt granic przelicza tylko nieaktualne piętro.
- Parser compact pozostaje. Kafelki rezerwują miejsce dla gruntu i dzieci przed dodawaniem przedmiotów; nie wykonują końcowego `shrink_to_fit()`. Jeden przedmiot nadal korzysta z przechowywania inline.
- Zakończenie normalnego otwarcia dokumentu następuje po przedstawieniu kompletnej klatki głównego widoku: gotowe chunki, zakończony atlas, udane renderowanie i sygnał `frameSwapped`. Token dokumentu odrzuca potwierdzenie starej klatki po zmianie lub zamknięciu dokumentu. Odzyskiwanie wielu dokumentów zachowuje dotychczasową kolejkę odzyskiwania.
- Pasek zachowuje animację 25 ms; `loadDelay` pozostaje usunięty. Spóźniony wynik z niższym postępem nie zmienia procentu ani opisu bieżącego etapu. Kolejka postępu z parsera nie uruchamia zagnieżdżonego `processEvents()`.

## Diagnostyka

`DME.exe --load-profile` wypisuje na stderr rekordy `DME_LOAD`. `elapsed_ms` mierzy czas od wejścia do programu, `duration_ms` czas etapu (-1 oznacza znacznik), a `peak_ram_bytes` szczytowy working set procesu na Windows. Raport obejmuje QML, OTBM z XML i indeksami pozycji, indeks renderera, ładowanie DAT/SPR/OTB, widoczność palety, powiadomienie filtrów, miniatury na żądanie, definicje edytora, dekodowanie atlasu, zgłoszenie uploadu i pierwszą kompletną przedstawioną klatkę.

`atlas_upload_submit` oznacza zgłoszenie uploadu do QRhi, nie osobny pomiar czasu GPU. `first_complete_frame` obejmuje upload i przedstawienie klatki. Rekordy `map_counts`, `visibility_scan`, `map_sprite_counts` i `atlas_counts` podają wielkości danych; unikalne sprite’y bieżącej mapy obejmują również przygotowywane grafiki stworzeń i efektu edytora.

Automatyczne otwarcie:

```powershell
.\build\LoadProfile-dist\DME.exe --load-profile `
  --profile-map "C:\maps\RA.otbm" `
  --profile-client "C:\clients\960" --profile-version 960 --profile-exit
```

`--profile-repeat 6` mierzy pierwsze otwarcie oraz pięć ponownych odczytów tego samego pliku. Poprzedni dokument jest zamykany; mapa jest ponownie parsowana, a zgodny profil i atlas pozostają. Te opcje używają osobnych ustawień i stanu sesji w katalogu `profile-settings` obok programu. `--profile-exit` kończy proces po gotowym widoku, z limitem 60 s.

`--profile-scroll` po gotowym widoku przechodzi przez pięć stron palety All Items i przez 3 s mierzy odstępy timera GUI 16 ms. Rekord `palette_scroll_complete` podaje najdłuższą przerwę, liczbę gotowych/oczekujących/błędnych obrazów i rozmiar cache. Jest to test automatycznego pozycjonowania GridView, nie fizycznego przewijania myszą.

Powtarzalne uruchomienia i JSON:

```powershell
.\scripts\profile-map-loading.ps1 `
  -DmePath .\build\LoadProfile-dist\DME.exe `
  -MapPath "C:\maps\RA.otbm" -ClientFolder "C:\clients\960" `
  -ClientVersion 960 -Runs 5
```

Dodanie `-Warm` mierzy pięć ponownych otwarć w jednym procesie, pomijając pierwszy start w medianie. `-OutputDirectory` pozwala zapisywać osobno wyniki różnych buildów. Skrypt wymaga dokładnie zadanej liczby kompletnych klatek; timeout lub błędne zakończenie procesu przerywa pomiar.

## Wyniki, 2026-10-06

Release, MinGW 13.1, Qt 6.10.2, natywny renderer Windows. Nowy proces w każdej próbie; cache plików Windows nie był opróżniany. Ten sam build, domyślny początkowy widok i izolowane ustawienia diagnostyczne.

RA: 67 678 342 bajty, 6 328 689 kafelków, 8 886 339 instancji przedmiotów. Klient 960: 19 325 definicji przedmiotów, 130 844 sprite’y klienta. Atlas z grafikami stworzeń: 30 301 sprite’ów, 128 450 560 bajtów.

| Próba | Pełny start procesu do gotowego widoku RA |
|---|---:|
| 1 | 8,391 s |
| 2 | 7,564 s |
| 3 | 7,505 s |
| 4 | 7,746 s |
| 5 | 7,742 s |
| **Mediana** | **7,742 s** |

Pięć ponownych otwarć RA: 4,476 / 4,602 / 4,369 / 2,730 / 4,420 s; **mediana 4,420 s**. W logu nie ma ponownego skanowania palety, dekodowania atlasu ani uploadu atlasu. Szczytowy working set wynosił około 1,26 GiB dla świeżego procesu i 1,80 GiB podczas serii ponownych otwarć. Pozostawiony parser nadal tworzy pośrednie drzewo OTBM, co wpływa na szczyt RAM.

W rzeczywistej aplikacji koszt RA rozkłada się na około 1,2–1,7 s QML, 1,7–1,8 s OTBM, 0,45 s indeksu renderera, 1,7 s widoczności palety i 0,18–0,20 s atlasu. Od oczekiwania na pierwszy widok do jego przedstawienia upływa dodatkowo około 1,7–1,9 s. Te ostatnie wartości obejmują pokazanie głównego okna, przygotowanie sceny, chunki i upload; nie są samym czasem GPU.

Osobny wcześniejszy test etapów CPU, bez całego QML i renderowania: przed zmianą paleta z PNG zajmowała 3,7–4,1 s, po zmianie około 0,22–0,25 s; mediana całości etapów spadła z około 5,59 do 1,98 s. **To nie jest porównanie pełnego startu aplikacji.** Nie dysponujemy równoważnym pomiarem pełnej klatki starszego builda, więc nie przypisujemy tym wynikom potwierdzonego zejścia z 30 s.

Sprawdzono także starszy klient 772: rzeczywista mapa `jj.otbm`, około 7,98 MB, 5 828 definicji i 15 116 sprite’ów klienta; pojedynczy pełny start 5,036 s. Użyto standardowego DAT/SPR z dostępnego klienta OTCv8 i OTB z RME 760, w lokalnym katalogu testowym. Dla dostępnego klienta 1077 (21 864 definicje, 276 480 sprite’ów) mapa kontrolna z jednym kafelkiem osiągnęła kompletny widok w 5,510 s; widoczność zajęła 1,967 s. Mapa kontrolna bada koszt profilu, nie wydajność dużej mapy ani zgodność wszystkich jej przedmiotów.

Automatyczny test rzeczywistej palety RA w OpenGL: 330 nowo utworzonych miniatur, cache 3 010 560 bajtów, 70 gotowych obrazów na końcu, zero oczekujących i błędów. Najdłuższa przerwa GUI wyniosła 45 ms; nie oznacza to utrzymania każdej klatki w 16 ms. Nie potwierdzono interakcyjnie przewijania myszą. Dodatkowe próby ukrytego okna z D3D11 zatrzymały się po pierwszym zgłoszeniu uploadu i nie potwierdziły gotowości przed timeoutem; przyczyna pozostaje nieustalona. Pięć pomiarów w tabeli zakończyło się gotowym widokiem. Test palety w OpenGL zakończył się kodem 0.

**Cel mediany ≤5 s dla pełnego startu RA nie został osiągnięty.** Spełniono go dla ponownego otwarcia tego samego profilu. Obecne pomiary nie uzasadniają przebudowy atlasu pod początkowy viewport: czas atlasu CPU jest mały. Następny pomiar powinien dokładniej rozdzielić przygotowanie sceny głównego okna i pracę podczas skanowania widoczności w GUI oraz wyjaśnić timeout D3D11. Różnica pomiędzy testem CPU a pełną aplikacją pozostaje do wyjaśnienia.

## Weryfikacja

Dziewięć zestawów testów przeszło: `palette_visibility`, `otbm_compact_storage`, `otbm_editing_storage`, `otbm_background_load`, `atlas_background_build`, `map_creature_refresh`, `map_rhi_opengl`, `map_rhi_d3d11`, `advanced_brush_qml`.

Rozszerzone przypadki obejmują przezroczyste, czarne, bardzo ciemne, wielowarstwowe i wielokafelkowe grafiki, doodady, równoległe żądania, zmianę profilu podczas żądań, rzeczywistą eksmisję LRU po przekroczeniu 32 MiB, zachowanie atlasu przez pusty dokument, przebudowę po przekroczeniu 256 MiB kumulowanych grafik, zmianę klienta, granice po dodaniu kafelka i undo/redo, odrzucenie starego potwierdzenia widoku, anulowanie odczytu przy zmianie dokumentu oraz spóźniony postęp. Testy QML obejmują zakresy poza ekranem i tryb listy; testy QRhi sprawdzają piksele i upload w OpenGL oraz D3D11.

Przykład uruchomienia w środowisku z ograniczonym katalogiem TEMP:

```powershell
New-Item -ItemType Directory -Force build\test-tmp | Out-Null
$env:TEMP = (Resolve-Path build\test-tmp).Path
$env:TMP = $env:TEMP
$env:QT_FORCE_STDERR_LOGGING = '1'
ctest --test-dir build/Release-1.1.2 --output-on-failure `
  -R '^(palette_visibility|otbm_compact_storage|otbm_editing_storage|otbm_background_load|atlas_background_build|map_creature_refresh|map_rhi_opengl|map_rhi_d3d11|advanced_brush_qml)$'
```

Artefakty lokalnych pomiarów: `build/profile-ra-final-results`, `build/Release-1.1.2/profile-ra-warm-five.log`, `build/Release-1.1.2/profile-small772-final.log`, `build/profile-1077-results`. Build do sprawdzenia: `build/LoadProfile-dist/DME.exe`.
