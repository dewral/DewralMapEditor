# Pogoda — strefy i test na mapie

**View > Weather regions...** oraz **Map weather** otwierają jeden panel.
U góry wybierz **Pogoda namalowanej strefy** albo **Test pogody na całej mapie**.
Oba tryby używają tych samych suwaków z `WeatherControls.qml`; In-game Preview
zachowuje osobne ustawienia i ten sam zestaw kontrolek. Nie ma miniatury pogody.

## Pogoda strefy

1. Dodaj strefę, podając nazwę. Wybierz pędzel i **Rysuj strefę**.
2. Namaluj obszar na aktualnym piętrze. Ctrl wymazuje, Shift rysuje prostokąt,
   Escape kończy rysowanie. Kliknij przycisk pogody nad mapą, aby wrócić do panelu.
3. Ustaw efekty suwakami, tak jak w Map weather. Efekt jest widoczny bezpośrednio
   na wybranej strefie — także w czasie rysowania i przy wyłączonym kolorze stref.
   **Pokaż na mapie** zamyka panel, pozostawiając podgląd. Dachy i znany teren
   nadal ograniczają efekty; przy zoomie poniżej 8 px/kafelek pogoda jest ukryta.
4. Wprowadź minimalny i maksymalny czas w sekundach. Aby deszcz trwał dokładnie
   10 minut, ustaw **600 / 600**. Przy **Po tym czasie wyłącz efekty** eksport
   zaczyna od ustawionego miksu, a po terminie przechodzi na pogodę bez efektów.
   Bez tej opcji miks pozostaje aktywny. Serwer restartuje ten przebieg od początku
   po starcie lub udanym reloadzie konfiguracji.
5. **Zapisz pogodę strefy** zapisuje miks i czas. Samo przesuwanie suwaków nie
   zmienia konfiguracji. **Przywróć zapisane** odrzuca lokalny podgląd.
6. **Eksport na serwer** wybiera katalog nadrzędny i publikuje nowy katalog pakietu
   z `map.otbm`, plikami XML mapy i `environment.json`. Najpierw zapisuje snapshot
   do katalogu roboczego, odczytuje go ponownie i porównuje strefy. Błąd lub anulowanie
   przed publikacją usuwa wyłącznie katalog roboczy. Oryginały pozostają bez zmian.
   Na serwer należy przenieść mapę oraz konfigurację razem.
Rysowanie zapisuje geometrię osobno od wyboru pogody. Własny miks jest zwykłym
presetem v1 (`dme_` + skrót zawartości); czas należy do klimatu regionu.
Opcja wyłączenia używa pustego presetu jako jedynego kolejnego wyniku losowania.
Dotychczasowe presety i wielopresetowe klimaty z importu są zachowane do momentu
jawnego zapisania nowego miksu dla danego regionu. Inne regiony nie są zmieniane.

## Test lokalny

Wybierz **Test pogody na całej mapie**, ustaw suwaki i **Uruchom test**.
Efekt obejmuje widok mapy zgodnie z lokalną opcją dachów, bez maski strefy.
Możesz ustawić czas testu i automatyczne zatrzymanie, pauzować albo zakończyć
przyciskiem **Zatrzymaj test**. Ten tryb nie zmienia dokumentu pogody regionów,
ich czasu ani pliku eksportu. Przejście z testu do stref zatrzymuje test mapy.

## Wspólne ID i historia

Nowa strefa otrzymuje najniższy wolny ZoneID 1–65535. Uwzględniane są ID mapy
oraz puste definicje edytora. Lista pokazuje ID, nazwę, liczbę kafelków i oddzielne
obszary (łączność ośmiokierunkowa); przyciski obszarów przenoszą kamerę.
ID bez przypisania pogody ma nazwę „Strefa N”. Wybranie go pozwala przypisać klimat.
Usunięcie przypisania pogody zachowuje ID kafelków.

Pędzel numerowanych stref oraz pędzel pogody zmieniają te same dane OTBM,
także na kafelkach bez przedmiotów. Ctrl usuwa tylko wybrane ID, pozostałe ID
zostają. Ctrl+Z podczas rysowania korzysta z historii mapy. Przyciski **Cofnij
ustawienia / Ponów ustawienia** zmieniają wyłącznie metadane pogody. Geometrię
należy zapisać poleceniem zapisu mapy; ustawienia zapisują się do
`<map.otbm>.weather.json`. Save As przenosi ustawienia do nowej ścieżki.

Nowe powiązanie serwerowe ma postać:

```json
{"id":"swamp","name":"Swamp","priority":10,"zoneId":1}
```

Bez własnego `climate` dziedziczy klimat globalny. Piętra, otwory i rozłączne
fragmenty wynikają z kafelków OTBM. Indeks geometrii i cache oddzielnych obszarów
są aktualizowane po zmianach ID, undo/redo, imporcie i odczycie mapy. Renderer
nie buduje co klatkę klastrów numerowanych stref.

Konfiguracje legacy `polygon`/`areas` nadal działają. Polecenie **Przenieś pogodę
do wspólnych stref (kopia)** działa na snapshotach w tle, pokazuje postęp i pozwala
anulować. Tworzy nowy pakiet; nie zmienia otwartej mapy. Przynależność liczy według
środka kafelka, włączając granicę, bez zaokrąglania ułamkowych wierzchołków.
Czas strefy jest wspólnym czasem klimatu serwera, nie czasem od wejścia gracza.

Import mapy remapuje kolidujące ZoneID na wolne numery i odpowiednio przepisuje
powiązania pogody z pliku towarzyszącego lub sąsiedniego `environment.json`.
Różne strefy nie są łączone po numerze. Błędna pogoda źródłowa jest zgłaszana;
same ID zaimportowanej mapy pozostają dostępne. Geometria importu ma historię
mapy, ustawienia — osobną historię pogody. Import samego JSON zastępuje metadane
jako jedną odwracalną operację; błędna walidacja zachowuje poprzedni dokument.

Eksport powiązań ZoneID wymaga włączenia zapisu numerowanych stref. Eksport
samego JSON wymaga uprzedniego zapisania mapy; panel domyślnie eksportuje kompletny
pakiet. Puste definicje pozostają w sekcji `editor`, nie trafiają do serwera.
Limit JSON to 4 MiB; legacy podlega dodatkowo limitom obszarów i indeksu serwera.
**Pogoda na mapie** uses the existing DME particle renderer and the
selected climate's starting preset; it does not simulate server scheduling or
guarantee pixel-identical rendering to OTClient. Custom preset definitions and
global climate are retained on import/export. Region mixes and durations use the shared sliders.

Validation: build and run `weather_regions_test`, then run the editor smoke test.
The native model test checks painting, holes, floors, stroke grouping, cancel,
undo/redo, companion reopening, import/export shared map history, remapped imports and snapshot migration.

Native UI smoke: run DME.exe --smoke-test --smoke-weather-regions, optionally
with --weather-screenshot <image.png>. Set QSG_RHI_BACKEND to d3d11 or opengl
to select the backend. Painting, Ctrl+Z and Escape passed on both in verification.
The OpenGL capture still showed text artifacts in the editor; Direct3D displayed
the panel correctly. Other themes and a live server session have not been verified.

Timing integration: weather_regions_test <export.json> also writes
<export.json>.timed.json. From TeriaEngine run
luajit tests/weather_dme_timing_test.lua <export.json>.timed.json.
The native smoke test verifies the local one-second whole-map timeout, that test
mode leaves the region definition unchanged, and that drawing undo retains the
saved regional rain mix. Shared WeatherControls are also covered by
tests/qml/tst_weather.qml (use Qt Quick Controls Basic style for the test runner).

## Parametry wyglądu

Panel wspólnych kontrolek ma nowe ustawienia skali, dryfu i falowania mgły,
czasu podmuchów, prędkości, krycia i koloru deszczu. Ustawienia zaawansowane są
zwijane. Fog steruje ilością pasm; fogOpacity wyłącznie ich alfą. Zwykły zakres
krycia to 0–100; zachowano legacy 100–200 jako wzmocnienie. Nowe presety to
Gentle mist, Dense fog i Drizzle. Dotychczasowe identyfikatory są zachowane.

Wszystkie pola mieszczą się w environment.json v1. Zapis pogody strefy zachowuje
je w presecie; podgląd, eksport, import i ponowne otwarcie odczytują te same wartości.
Lokalny test całej mapy ma osobne ustawienia i timer. Wyłączenie jakości usuwa efekty.

Mgła używa teraz teksturowanych płatów oraz tego samego pliku weathervisuals.h co
TibiaClient: dwóch warstw, trzech oryginalnych masek i geometrii w jednostkach świata.
Maski ekspozycji są buforowane i odświeżane po edycji, ruchu kamery i zmianie piętra.
Renderowanie pozostaje w Qt scenegraph. Nie wymaga nowych plików prywatnych ani
wspólnego silnika graficznego z klientem. Zgodność jest znaczeniowa i wizualna,
a nie pikselowa. Test renderowania sprawdza dachy, otwory i wyłączenie efektów;
liczniki fragmentów oraz pokrycia pomagają ocenić koszt dużych płatów.