# DME Activity

The Work timer popup includes **My activity**, a personal daily server summary
with active/AFK time, edits, tile changes, saves and save errors. It refreshes
only while visible, identifies pending events, and links to the full dashboard.
Device credentials read only their own account, including admin credentials.
Manual Work timer tasks remain local and are never added to automatic totals.

Panel zespołu i automatyczna rejestracja aktywności na przypisanych mapach. Mapper widzi własne dane; administrator cały zespół. Liczniki opisują aktywność, nie jakość pracy.

## Lokalny UniServer (Windows)

1. W katalogu `activity` wykonaj `npm ci` oraz `npm run build`.
2. Skopiuj zawartość `dist/public` oraz `uniserver/api.php` do `UniServerZ/www/dme-activity`. Nie kopiuj tam bazy, ustawień ani danych logowania.
3. Uruchom dwuklikiem `Uruchom-DME-Activity.cmd` w Eksploratorze. Plik `.ps1` może otwierać się w Notatniku; `.cmd` uruchamia go prawidłowo i otwiera panel.
4. Otwórz `http://localhost/dme-activity/`. Dane kont znajdują się w `activity/.local-admin.txt`, generowanym przy pierwszym uruchomieniu.
5. Jako administrator utwórz konto mappera w panelu (e-mail, hasło min. 12 znaków, rola Mapper) i przekaż mu dane. Mapper otwiera w DME **View → DME Activity**, wpisuje adres `http://localhost:3000`, e-mail oraz hasło i wybiera **Sign in**. Hasło nie jest zapisywane; DME zapamiętuje token urządzenia.
6. W panelu administratora → Połączenie i zespół → Śledzenie map wpisz nazwę pliku (np. `blez_1.otbm`) i zaznacz „Śledź automatycznie”. Otwórz ten plik w zalogowanym DME — zostanie rozpoznany automatycznie, bez ręcznego przypisania w aplikacji.

UniServer udostępnia pliki i przekaźnik PHP; Node obsługuje API na loopback. Przekaźnik odrzuca połączenia spoza tego komputera. Serwer Node trzeba uruchomić po restarcie komputera. `stop-local.ps1` zamyka lokalny serwer i bazę bezpiecznie; uruchom go w PowerShell, nie przez otwarcie w Notatniku. Pilot tworzy projekt **Pilot lokalny**, mapę **Mapa pilotażowa** oraz konta **Mapper testowy 1/2**; nie generuje fikcyjnych statystyk.

Baza lokalna to PGlite (PostgreSQL) w `.local-db`. To wariant do pilotażu na jednym komputerze, nie do produkcyjnego zespołu. Przed kopiowaniem tego katalogu zatrzymaj proces Node wskazany w `.local-server.pid`. Nie uruchamiaj dwóch serwerów na tej samej bazie.

## VPS / Docker

Skopiuj `.env.example` do `.env`, ustaw domenę, silne hasła i e-mail administratora. Hasło bazy powinno składać się z znaków bezpiecznych w URL, np. losowych znaków alfanumerycznych. Domena musi wskazywać VPS, a porty 80/443 być dostępne.

```
docker compose up -d --build
```

Caddy obsługuje HTTPS. Baza PostgreSQL nie wystawia portu na zewnątrz. Administrator powstaje tylko przy pustej bazie; późniejsze zmiany zmiennych nie resetują hasła istniejącego konta. W panelu utwórz mapperów, projekt, mapy i przyznaj dostęp do projektu. Administrator również musi być członkiem projektu, jeśli chce rejestrować własną pracę w DME.

Kopia bazy: `docker compose exec -T db pg_dump -U activity activity > activity-backup.sql`. Aktualizacja: wykonaj kopię, potem `docker compose up -d --build`. Schemat początkowy jest idempotentny; zmiany istniejącego schematu w kolejnych wersjach wymagają migracji.

## Semantyka

- Czas aktywny obejmuje interakcje wyłącznie z DME. AFK zaczyna się po 5 minutach bez interakcji. Odcinki czasu mierzy zegar monotoniczny; UTC jest zakotwiczone przy starcie procesu.
- Edycja to zatwierdzona operacja, także grupa operacji masowych. Liczba tile'ów jest sumą dotkniętych pól, nie liczbą unikalnych pól. Undo/Redo nie zwiększają edycji ani tego licznika.
- DME pobiera reguły co 30 sekund i porównuje samą nazwę pliku bez rozróżniania wielkości liter. Wyłączenie reguły zatrzymuje rejestrację po odświeżeniu. Dwie pasujące reguły w dostępnych projektach powodują konflikt i brak rejestracji. Nieprzypisane pliki nie są śledzone. Stare lokalne przypisania są ignorowane; istniejące mapy nazwane `*.otbm` otrzymują automatycznie regułę o tej nazwie. Sesja kończy się przy zmianie aktywnej mapy, zmianie nazwy pliku, wylogowaniu lub zamknięciu DME. Przy awarii procesu nie dopisujemy czasu do następnego uruchomienia. Może brakować ostatnich mniej niż 30 sekund przed awarią.
- Autosave nieaktywnej zakładki jest przypisany do ostatniej zarejestrowanej sesji tej mapy, bez doliczania czasu.
- Równoległe sesje z różnych urządzeń są raportowane oddzielnie; sumy czasu sumują sesje, nie unikalne minuty człowieka.
- Heartbeat co 30 sekund, wysyłka co 15 sekund. Offline po 90 sekundach bez świeżego heartbeat. Dosłanie starego heartbeat nie oznacza obecności online.
- Zdarzenia mają UUID i kolejność w sesji. Duplikaty są potwierdzane bez ponownego naliczania. Baza lokalnej kolejki SQLite przechowuje także dane dostępu potrzebne do wysłania zaległych paczek; nie umieszczaj jej w publicznym katalogu.
- Cofnięcie dostępu lub usunięcie członkostwa blokuje wysyłkę. DME pokazuje błąd i zachowuje kolejkę. Wylogowanie nie usuwa niewysłanych zdarzeń. Wysyła je przed zdarzeniem `logout`, które cofa token urządzenia; przy braku sieci odbywa się to po odzyskaniu połączenia. Przywrócenie uprawnień pozwala wysłać zaległości; trwale odwołane urządzenie wymaga świadomej administracyjnej decyzji o zachowaniu/usunięciu kolejki.
- Raport ma maksymalnie 93 dni i 200 000 zdarzeń na zapytanie; dla większej liczby wybierz krótszy okres. Dane liczbowe pozostają niezmienne, notatki można edytować.
- Work Timer i jego historia pozostają niezależne. Nie wysyłamy plików map, sprite'ów, ścieżek ani treści wpisywanych klawiszy.

## API v1

Wszystkie ścieżki zaczynają się od `/api`. Panel używa ciasteczka HttpOnly/SameSite Strict; urządzenie używa `Authorization: Bearer <token>`.

- `POST /login`, `POST /logout`, `GET /me`, `GET /catalog`
- Administrator: `POST /users`, `POST /projects`, `POST /maps` (`name`, `projectId`, `filename`, `tracked`), `PATCH /maps/:id` (`filename`, `tracked`), `PUT /members`
- `POST /device/login` → `{email,password,name}` / `{deviceId,token,accountName,accountEmail}`
- `GET /device/maps`; `POST /device/events` → `{events:[...]}` / `{accepted:[UUID...]}` (maks. 500 zdarzeń)
- `POST /devices/:id/revoke`
- `GET /report?from=YYYY-MM-DD&to=YYYY-MM-DD&zone=Europe/Warsaw` z opcjonalnym `projectId`, `mapId`, `userId`
- `GET /sessions/:id`; `PATCH /sessions/:id/note` → `{note:"..."}`

Zdarzenie: `{id, sessionId, mapId, seq, at, kind}`. Czasy UTC w ISO 8601. `edit` dodaje `tiles`; `duration` dodaje `start` i dokładnie jeden z `activeMs`/`idleMs` (maks. godzina); `heartbeat` dodaje `status`. Pozostałe rodzaje: `start`, `end`, `logout`, `undo`, `redo`, `save`, `autosave`, `save_error`, `autosave_error`. Heartbeat bez przypisanej mapy ma `sessionId`/`mapId` równe `null`.

## Sprawdzenie pilotażu

Uruchom dwie kontrolowane sesje mapperów. W każdej: edycja grupowa, Undo/Redo, zapis, autosave, zmiana mapy, przerwa ponad 5 minut i powrót. Odłącz sieć, wykonaj edycje, przywróć połączenie i porównaj liczniki. Sprawdź, że mapper 1 nie widzi sesji mappera 2. Rozpocznij rzeczywiste użycie dopiero po porównaniu raportu z tym scenariuszem.

Automatycznie: `npm test`, `npm run build`; w projekcie DME test CTest `activity_service` i test uruchomienia `dme_startup`.
