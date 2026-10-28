---
name: dme-modern-cpp
description: >-
  Rozwijaj i naprawiaj kod C++ w DME, szczególnie własność zasobów, czas życia, kontenery i współbieżność. Stosuj przy implementacji C++ lub planowanej migracji C++20; nie do samych zmian QML, budowania ani przeglądu bez edycji.
---

# Nowoczesny C++ w DME

Sprawdź [mapę architektury](../../../ARCHITECTURE.md) dla dotykanego modułu. Workflow nie jest poleceniem migracji języka: źródłowy CMake obecnie wymaga C++17.

## Dobór zmiany
- Ustal właściciela danych i źródło wywołań. Wskaźnik może być obserwatorem, polem `QObject`, wynikiem należącym do rodzica lub własnością RAII; nie zastępuj ich mechanicznie `shared_ptr`.
- Preferuj wartości i `unique_ptr` dla wyłącznej własności zwykłych zasobów. Zachowuj rodziców `QObject` i kontrakt Qt. Przy odroczonym callbacku rozważ `QPointer`, kontekst połączenia i generację; samo przechwycenie `this` lub wspólnego wskaźnika nie zapewnia bezpieczeństwa wątkowego.
- Wybieraj Qt/STL zgodnie z istniejącymi interfejsami i czasem życia. Sprawdź unieważnianie wskaźników/iteratorów, implicit sharing i odłączenie kontenera przy mutacji; nie przechowuj widoków dłużej niż ich dane.
- Ograniczaj kopie i alokacje w potwierdzonym hot path. `reserve`, przenoszenie i reuse muszą respektować stabilność danych, peak RAM i czytelność; `std::move` na `const` nie usuwa kopii.
- Zachowuj istniejące raportowanie błędów (`bool`, tekst błędu, sygnały lub wynik). Nie narzucaj wyjątków przez granice Qt/QML. `noexcept` dodawaj tylko przy rzeczywistym kontrakcie niezgłaszania wyjątków.

## Współbieżność
- Oddziel obliczenia na snapshotach od publikacji do obiektów Qt. Użyj istniejącego workera/QtConcurrent, gdy pasuje do modułu; nie dodawaj `std::async`, lock-free ani nowej puli bez potrzeby.
- Prześledź anulowanie, ponowne otwarcie dokumentu, zmianę klienta, kolejność mutexów i shutdown. Nie rozwiązuj wyścigu czekaniem GUI na długie zadanie.
- Nie dereferencjonuj na workerze danych mapy tylko dlatego, że wskaźnik nie jest null. Potwierdź synchronizację, okres ważności i zachowanie przy zmianie dokumentu.

## C++20 — tylko uzgodniona migracja
Zweryfikuj standard targetu, kompilatory i pełny build. Dopiero wtedy dobieraj `span`, ranges, concepts czy `jthread` do konkretnej potrzeby. Nie dodawaj modułów, korutyn ani zamienników istniejących mechanizmów Qt wyłącznie z powodu dostępności nowego standardu.

## Dowód wyniku
Zbuduj dotknięte targety i uruchom testy z mapy w `ARCHITECTURE.md`. Dodaj test regresji zachowania, gdy zmieniasz kontrakt; przy lifetime/concurrency obejmij anulowanie i zamknięcie. Sanitizery stosuj, gdy używany toolchain/Qt je obsługują, bez ogłaszania nieistniejącego presetu. Podaj ograniczenia i niewykonane kontrole.
