---
name: dme-code-review
description: >-
  Przeprowadzaj przegląd zmian C++/Qt/QML/CMake/renderera DME i szukaj konkretnych błędów, regresji oraz luk testowych. Używaj na żądanie review, audytu lub oceny diffu; domyślnie nie edytuj ocenianego kodu.
---

# Przegląd oparty na dowodach

Ustal zakres: diff roboczy, staged, commit lub porównanie wskazanych refów. Nie przypisuj wszystkich lokalnych zmian ocenianemu zadaniu. Użyj [ARCHITECTURE.md](../../../ARCHITECTURE.md) jako mapy, aktualnego kodu jako dowodu. Nie wykonuj napraw ani publikacji bez polecenia użytkownika.

## Ocena zmiany
- Odtwórz konkretny trigger, przepływ danych i odbiorców API. Czytaj sąsiedni kod i testy, nie tylko izolowany fragment.
- C++/Qt: właściciel, usuwanie, iterator/view po mutacji, callbacki, queued publication, generacje, anulowanie i zamykanie dokumentu. Uwzględnij obecny C++17 i toolchain zamiast zakładać C++20.
- Dane: zgodność zapisu, subtype/client/server IDs, integralność indeksów, undo/redo. Sprawdź ścieżkę błędu i częściowo gotowy wynik.
- QRhi: wątek i urządzenie, odtworzenie zasobów, lifetime batchy, dirty flags/wersje, wyrównanie uniformów, kolejność drawów, blending i orientacja framebufferów. Deklarowany zysk wydajności porównaj z pomiarami.
- QML: bindingi/NOTIFY, resety modeli, fokus/Escape, popupy, resize, motywy, dock/float i niezależność Preview. Mock nie dowodzi rzeczywistego kontraktu C++.
- CMake/deployment: targety, rejestracja QML/QRC/shaderów, SDK/runtime, `EXCLUDE_FROM_ALL`, dostępność testów i zawartość pakietu. Zmiana CI na `main` może wpływać na publikację.

Dobierz istotne obszary, nie odtwarzaj całej listy w raporcie. Preferencja formatowania lub hipotetyczny problem bez ścieżki wykonania nie stanowi findingu. Odróżnij nową regresję od błędu istniejącego przed diffem.

## Weryfikacja i wynik
Jeśli wykonalne i objęte zakresem, uruchom istniejący test lub minimalną reprodukcję; wskaż brakujące fixture/SDK/GPU. Stare binarium lub inny build nie weryfikują diffu.

Najpierw przedstaw findings według wpływu. Każdy powinien mieć plik:linia, warunek wystąpienia, skutek, dowód i kierunek poprawy. Uzasadnij priorytet skutkiem dla użytkownika; nie wymyślaj błędu dla zapełnienia raportu. Oddziel ustalenia od pytań i hipotez. Jeśli nie ma findings, napisz to wprost i podaj zakres oraz pozostałe luki w testach; nie nazywaj kodu bezbłędnym.
