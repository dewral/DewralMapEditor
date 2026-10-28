---
name: dme-cmake-builds
description: >-
  Konfiguruj, kompiluj i diagnozuj build/test/deployment DME w CMake i Qt 6, w tym presety Windows/vcpkg i istniejące drzewa MinGW. Używaj dla problemów toolchainu, celów, shaderów i pakowania, nie do optymalizacji algorytmów.
---

# CMake i Qt SDK

Punkty wejścia i zależności są w [ARCHITECTURE.md](../../../ARCHITECTURE.md); o dostępności rozstrzygają aktualne `CMakeLists.txt`, `CMakePresets.json` i cache wybranego builda.

## Odkrywanie zamiast zgadywania
- Sprawdź źródła i istniejące drzewo konfiguracji: generator, kompilator/ABI, `CMAKE_BUILD_TYPE`, Qt6/GuiPrivate, `BUILD_TESTING`, `DME_RUNTIME_OUTPUT_DIRECTORY`. Nie konfiguruj cache innym kompilatorem.
- `cmake --list-presets` pokazuje zapisane presety. `windows-vcpkg` używa MSVC/vcpkg, `windows-release` buduje `package_all`. Nie przedstawiaj lokalnego katalogu Ninja jako presetu repozytorium.
- Minimum Qt i standard C++ odczytaj ze źródeł. Nie zakładaj C++20 ani zgodności GuiPrivate między SDK. Przy cross-buildzie rozróżnij host tools ShaderTools od bibliotek targetu.

## Build i testy
Wybierz istniejący target, zbuduj zależności i sprawdź kod wyjścia. Dla skonfigurowanego drzewa stosuj szablon:

```text
cmake --build <build-dir> --target <istniejacy-target> --parallel
ctest --test-dir <build-dir> -N
ctest --test-dir <build-dir> -R <regex-istniejacych-testow> --output-on-failure
```

Elementy w nawiasach są parametrami do ustalenia, nie gotowymi ścieżkami/API. Dla generatora wielokonfiguracyjnego dodaj `--config Release` oraz `-C Release`. Przykład z rzeczywistym presetem znajduje się w `ARCHITECTURE.md`.

Testy są zwykle `EXCLUDE_FROM_ALL`: zbuduj je jawnie przed CTest. Sprawdź warunki QuickTest, Windows i zewnętrznych fixture. Test pominięty lub niezarejestrowany nie jest zaliczony. Runner QML i mocki mają inne pokrycie niż aplikacja/GPU.

## Zasoby i deployment
Nowe źródło dodawaj do właściwego targetu; typy C++ QML do istniejącej rejestracji `Tibia`, widoki/assets do QRC, shadery do `qt_add_shaders` dla potrzebnych targetów aplikacji i testów. Nie edytuj wygenerowanych `moc`, `qrc`, `.qsb` ani cache jako trwałego rozwiązania.

Build `DewralMapEditor` wykonuje deployment i kopiuje dane. Ustal katalog wyjściowy; nie podmieniaj EXE innej instalacji. Blokada pliku nie oznacza błędu C++; zachowaj mapy i uzyskaj zamknięcie właściwego procesu. Nie obchodź blokady automatycznym zabijaniem aplikacji.

Pakowanie jest odrębne: `package_release`, `package_source`, `package_checksums`, `package_all` są zdefiniowane warunkowo na Windows. Sprawdź skrypty `cmake/` i manifest przed zmianą pakietu. Zgodność runtime Qt, kompilatora i updatera jest częścią wyniku. Nie publikuj ani nie wykonuj push/release workflow tylko dlatego, że lokalne pakowanie się udało.

Raportuj dokładne polecenia, konfigurację i wyniki. Rozróżnij błąd źródeł, brak SDK/fixture, niezbudowany test, blokadę EXE i brak GPU. Nie zmieniaj wersji, presetów ani zależności w ramach samej diagnozy, jeśli nie wynika to z zadania.
