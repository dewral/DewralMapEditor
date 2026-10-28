# DewralMapEditor — zasady pracy

## Zakres i źródła prawdy
- Najpierw sprawdź stan roboczy i kod dotyczący zadania. Zachowuj niezwiązane zmiany użytkownika; nie resetuj ani nie formatuj całego repozytorium.
- [ARCHITECTURE.md](ARCHITECTURE.md) zawiera mapę kodu, zależności i weryfikacji. Przy rozbieżności rozstrzygają aktualne źródła, CMake i używany cache, nie historyczne opisy ani przykłady innych projektów.
- Obecny CMake wymaga **C++17**. C++20 jest kierunkiem możliwej migracji, nie obowiązującym standardem. Nie wprowadzaj funkcji wymagających C++20 bez osobno uzgodnionej zmiany toolchainu i weryfikacji zależności.
- Zachowuj istniejący podział `libs/otformats`, `editor/core`, `editor/map`, `editor/preview` i `editor/qml`. Nowa zależność lub wymiana podsystemu wymaga konkretnego uzasadnienia wynikającego z zadania.

## Poprawność i zasoby
- Preferuj RAII i jednoznacznego właściciela. Zachowuj własność rodzic–dziecko `QObject`, `CppOwnership` singletonu `Backend` i własność zasobów QRhi; nie obejmuj tego samego obiektu niezależnymi mechanizmami usuwania.
- Respektuj powinowactwo wątkowe Qt. Stan GUI/QML publikuj w wątku właściciela, wynik zadania sprawdzaj pod kątem czasu życia, anulowania i generacji. Zachowuj istniejące granice synchronizacji renderera.
- Zachowuj zgodność OTBM, identyfikatory server/client, kolejność kafelków i stosu przedmiotów, piętra, przezroczystość, światło, zaznaczenia, undo/redo oraz zapisywanie danych. Nie modyfikuj danych użytkownika jako sposobu na przyspieszenie renderowania.
- Rozwijaj istniejące usługi chunków, atlasu i `MapRhiBackend`; nie dodawaj bezpośredniego OpenGL do QML ani równoległego renderera bez potrzeby wynikającej z zadania.
- Dostosowuj styl do sąsiedniego kodu. Nie narzucaj masowego formatowania, wyjątków, STL, nowego frameworka testów ani refaktoryzacji tylko w celu ujednolicenia.

## Weryfikacja i wynik
- Dobieraj kompilację i testy do zmiany, korzystając z celów w `CMakeLists.txt`. Sprawdź ich dostępność: większość testów ma `EXCLUDE_FROM_ALL`, część zależy od SDK, systemu lub fixture.
- Optymalizacje poprzedzaj pomiarem oraz potwierdzeniem przyczyny w kodzie. Porównuj tę samą scenę, build, backend, zoom i ustawienia; rozdziel start od pracy ustalonej. Hipoteza nie jest pomiarem ani wykazanym przyspieszeniem.
- Zmiany QML sprawdzaj pod kątem bindingów, fokusu, klawiatury, zmiany rozmiaru i motywów. Test na mockach lub software backendzie nie dowodzi poprawności natywnego GPU.
- W podsumowaniu podawaj zmienione pliki, wykonane kontrole, wyniki i niewykonane kroki. Nie ogłaszaj pomyślnego builda, działania UI ani wzrostu wydajności bez dowodów.
- Nie dodawaj prywatnych map, `Tibia.dat`, `Tibia.spr`, `items.otb`, tokenów, logów ani lokalnych ścieżek SDK do śledzonych plików. Zachowuj `LICENSE` i `NOTICE`.
- Nie publikuj, nie wypychaj commitów ani nie kończ procesu z potencjalnie niezapisanymi mapami bez upoważnienia użytkownika. Push na `main` uruchamia workflow publikacji wydania.

## Skills
Ładuj tylko workflow odpowiadający zadaniu z `.agents/skills/`: `dme-modern-cpp`, `dme-qt-qml`, `dme-render-performance`, `dme-cmake-builds`, `dme-code-review`. Nie odczytuj wszystkich skills przy każdym zadaniu.
