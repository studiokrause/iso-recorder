# ISO Recorder — stan projektu i instrukcje dla kolejnego modelu

Ten plik jest punktem wejścia. Przeczytaj go w całości, zanim cokolwiek zmienisz. Stan na 2026‑11‑12, wersja **3.5**. Wydania od 2.0 są publikowane jako Release na GitHubie.

## 1. Co to jest

Natywny program dla Windows do równoczesnego nagrywania 1–16 kanałów wideo z dźwiękiem („ISO recording”).
C++20, Qt 6 Widgets, GStreamer. Jeden kanał = jeden `GstPipeline`; awaria kanału nie dotyka pozostałych.

Właściciel projektu pisze po polsku i tak należy mu odpowiadać. Interfejs programu jest domyślnie po angielsku,
z tłumaczeniami PL, DE, ES, IT, FR.

## 2. Dokumenty — co jest czym

| Plik | Rola |
|---|---|
| `AGENTS.md` | ten plik: stan, zasady pracy, pułapki, następne etapy |
| `instrukcja_koncowa.md` | **obowiązująca** specyfikacja; nadpisuje `instrukcja.md`. Rozdz. 3 — mechanizmy (pauza, split, wejścia, wyjścia, segmenty), rozdz. 5 — interfejs, rozdz. 6 — plan wersji |
| `instrukcja.md` | pierwotna specyfikacja (tło, pełna lista funkcji docelowych) |
| `rozmowa.md` | krytyka pierwotnej specyfikacji przez inny model (tło) |
| `README.md` | opis programu dla GitHuba — **w całości po angielsku** (decyzja właściciela) |
| `CHANGELOG.md` | historia wersji po polsku, pisana dla użytkownika |
| `mockup/`, `interfejs.png` | wzorzec wyglądu |
| `docs/zrzut_<wersja>.png` | zrzut okna na każdą wersję |

## 3. Co działa (3.5)

- **Wejścia**: generator testowy, ekran i pojedyncze okno (`d3d11screencapturesrc`), kamera z wyborem rozmiaru obrazu
  (`mfvideosrc`), SRT, RTSP i NDI (`fallbacksrc`; NDI: lista źródeł z sieci w polu Device); dźwięk: sygnał testowy, cisza, dźwięk systemowy (loopback) i wejścia
  WASAPI, z regulacją głośności.
- **Nagrywanie**: H.264 + AAC do MKV lub MPEG‑TS; enkoder wybierany próbą (NVENC → Quick Sync → AMF → MF → OpenH264);
  split ręczny i automatyczny; pauza w tym samym pliku; plik opisu `.json` obok nagrania.
- **Grupa kanałów**: wspólna chwila startu, pauzy i stopu (dokładność do jednej klatki).
- **Zdalne sterowanie (3.5)**: `src/app/RemoteControl.*` (OSC UDP przez QUdpSocket, MIDI przez winmm, jeden wykonawca poleceń
  `runCommand`), kodek `src/core/Osc.*`, ustawienia `AppSettings::RemoteConfig`, widok `MainWindow::view()/viewChanged`.
  Klienci OSC dostają stan na adres nadawcy (wygasają po 20 s). MIDI: przypisania (nuta/CC + kanał) per polecenie, domyślnie nuty 0–90; MIDI learn w Settings (`RemoteControl::beginLearn`,
  `MidiBinding`, zapis `remote/midiMap` tylko różnic od domyślnych); CC działa tylko po przypisaniu (suwaki).
  Moduł Companion w `companion/isorecorder` (Node, API 2.x, `~2.1.3`), dokumentacja `docs/Remote-Control.md`.
  Testy: `--remote-selftest`, `tests\osc_test.ps1`, `npm test` i `node test/e2e.mjs` w module. Niesprawdzone: prawdziwy Companion,
  Stream Deck, fizyczny kontroler MIDI. Port OSC bez hasła — dokumentacja ostrzega.
- **Wyjście na żywo** kanału, równolegle do nagrania: NDI albo SRT; można przełączać w trakcie nagrywania.
- **Zanik sygnału sieciowego**: do wyboru — nagranie trwa (ostatnia klatka → czarny obraz), pauza albo nowa część;
  automatyczne ponowne łączenie.
- **Awaria źródła**: automatyczny restart kanału (5, 10, 20, potem co 30 s) i wznowienie nagrywania w nowym pliku.
- **Log zdarzeń**: plik dzienny w `%LOCALAPPDATA%\IsoRecorder\logs` i okno w programie (zawijanie, kreski, eksport).
- **Settings**: domyślne parametry nagrywania nowych kanałów, karta odsłuchu, szablon folderu splitu.
- **Odsłuch** jednego kanału na wybranej karcie dźwiękowej (ikona słuchawek na kaflu, w liście, w dużym podglądzie).
- **Split do nowego folderu** dla zaznaczonych kanałów; **zegary testowe** (cyfrowy z numerem klatki, analogowy).
- **Sesja i listy kanałów**: zapis / wczytanie `.isorec.json`, autozapis, przywracanie po awarii, ostatnie listy.
- **Interfejs**: kafle 2×2 / 3×3 / 4×4 i widok listy, inspektor (Input / Output), panel sterowania, duży podgląd
  (dwuklik), kolejność kanałów tylko przeciąganiem (kafle i wiersze listy; przycisków MOVE UP/DOWN nie ma od 2.3), okno Disks z testem prędkości zapisu, 6 języków.
- **Odporność**: start i stop kanału mają limit czasu — źródło, które nie odpowiada, nie zawiesza programu.

## 4. Następne etapy (rozdz. 6 `instrukcja_koncowa.md`)

| Wersja | Zakres |
|---|---|
| — | Plan z `instrukcja_koncowa.md` rozdz. 6 jest zrealizowany do 3.3. Dalsze prace: rzeczy niesprawdzone na sprzęcie (DeckLink, NVENC/AMF, pełny dysk) i uwagi właściciela |

Decyzje właściciela (2026‑11‑11) — **nie wracać do tych tematów**:
- wyjście na wirtualną kamerę nie będzie robione: wystarcza wyjście NDI + „Webcam” z NDI Tools;
- słowo „passthrough” właściciel rozumie jako przekazanie sygnału dalej (robi to wyjście na żywo). Zapis **bez
  ponownego kodowania** wrócił do planu jako opcja enkodera (2.9) — nazywaj go „stream copy”, nie „passthrough”,
  i zawsze z opisem, co użytkownik dostaje i co traci (tekst jest w oknie Settings);
- hasło RTSP zostaje w adresie (to standard w tego typu oprogramowaniu), bez magazynu poświadczeń dla RTSP.

Znane braki: źródło, które w ogóle nie odpowiada (limit czasu startu), nie jest ponawiane automatycznie;
sortowanie po kolumnach w widoku listy; wybór klatkażu kamery (jest tylko rozmiar).

Jak działa wejście NDI (2.5): `Channel::makeNdiSource()` buduje bin `ndisrc ! ndisrcdemux` + kolejki + konwertery ze
stałymi padami „video” i „audio” i podaje go jako `source` elementu `fallbacksrc` (ustawiane w `startPreview`, bo tej
właściwości nie da się wpisać w opis tekstowy). Pułapki: (1) pady demuksera pojawiają się od nowa po każdym ponownym
połączeniu — łączy je własna obsługa `pad-added`, łączenie z opisu tekstowego działa tylko raz; (2) `restart-on-eos=true`
jest konieczne, bo `ndisrc` po zaniku kończy strumień (EOS), a nie zgłasza błędu; (3) o zaniku decyduje tylko przełącznik
obrazu — źródło bez dźwięku stale dostaje ciszę zastępczą; (4) nazwa źródła siedzi w `InputConfig::uri`; (5) listę źródeł
daje `Devices::ndiSources()` — stale uruchomiony `GstDeviceMonitor` z filtrem `Source/Network`, źródła pojawiają się po
kilku sekundach. NDI|HX działa (sprawdzone 2026‑11‑12 na NDI Screen Capture HX):
środowisko NDI samo rozpakowuje obraz i `ndisrc` oddaje UYVY — ta kompilacja wtyczki nie ma trybu obrazu skompresowanego.

Zamykanie procesu (2.6): `main()` kończy się zawsze przez `TerminateProcess` po zamknięciu kanałów i zapisaniu ustawień.
Zwykłe wyjście (wyładowywanie bibliotek po `ExitProcess`) po nagrywaniu ze zdalnego źródła NDI zawieszało się na zawsze:
zostawał proces „zombie” (HasExited = true, 1 wątek, `taskkill /F` odpowiada „brak działającego wystąpienia”), który
blokuje plik EXE do wylogowania. Taki proces da się obejść tylko zmianą nazwy zablokowanego pliku. W testach nie ufaj
`WaitForExit` — sprawdzaj, czy proces zniknął z listy (`Get-CimInstance Win32_Process`).

Domyślny format nowych kanałów (3.4): `AppSettings::defaultFormat()` (szerokość, wysokość, klatkaż) stosowane w
`ChannelManager::addChannel()` tylko przy włączonej trwałości (nie w trybach `--demo`). WHEP: `fallbacksrc` ma dla niego
`restart-timeout` 9 s (reszta 3 s) — ICE na komputerze z Tailscale i kilkoma kartami trwa dłużej niż 3 s i źródło
restartowało się w kółko; `timeout` zostaje 2 s (szybkie przejście na obraz zastępczy). Test `net_test.ps1 -Type whep`
ma poluzowane progi: zanik do 16 s, powrót obrazu po 11 s, pauza do 17 s.

Growing file (3.4): `OutputConfig::growing`; `Channel::applyGrowing(muxer)` ustawia na `mp4mux`/`qtmux` `fragment-duration=1000` i
`fragment-mode=first-moov-then-finalise` (też na `mux2` w zapisie podwójnym). Bez tego MP4/MOV mają indeks dopiero na końcu
i są nieczytelne w trakcie oraz po awarii. Sprawdzone `ffprobe`, nie w programach montażowych. Odczyt w chwili zapisu
fragmentu bywa chybiony — test mierzy 6 odczytów i wymaga 5 udanych. Odporność na awarię rosnącego MP4/MOV jest ograniczona: przy zabijaniu programu w 8 różnych chwilach 3 pliki dały błąd
nagłówka w ffprobe/ffmpeg (różnica w przesunięciu audio/wideo w pierwszym `moov`, struktura boxów identyczna); naprawa
qtdemux→mp4mux zawiesza się, więc `Recovery` nadal raportuje je jako `lost`. Nie obiecuj odporności na awarię dla MP4/MOV.
Test: `tests\growing_test.ps1`. MXF OP1a (Avid/EVS) poza zakresem: GStreamer ogranicza kodeki.

Ekran powitalny (3.3): `app/Splash.*` — okno warstwowe Win32 w osobnym wątku, 2,5 s, obraz z zasobu `:/splash.png`
(`res/splash.png`, źródło: `ISO_Recorder_3.3_splash.png`). `QSplashScreen` się nie nadaje: wątek główny jest przy starcie
zajęty ok. 3 s i nie zamknąłby okna na czas. Nie pojawia się w trybach testowych (`--demo`, `--list`, tryby automatyczne);
`--no-splash` wyłącza go ręcznie. Licencja projektu: MIT (`LICENSE`).

WebRTC, odzyskiwanie, instalator (3.3): wejście `whep` — `Channel::makeWhepSource()` (bin z `whepclientsrc`, podawany
`fallbacksrc` jako `source`, tak jak NDI; `InputConfig::customSource()`). Pułapki: (1) `stun-server` jest zerowany —
domyślny publiczny STUN opóźnia połączenie o kilka sekund i wysyła zapytania do obcej usługi; (2) sprzętowe dekodery
VP8/VP9 (d3d11/d3d12/qsv, także warianty `...deviceNdec`) dostają w tym procesie rangę NONE, bo po ponownym połączeniu
odrzucają strumień (`not negotiated`) i obraz nie wraca. Testowy nadajnik: `whepserversink` z gst-launch.
Odzyskiwanie: `core/Recovery.*` — dziennik `recordings.journal.json` obok autozapisu; `Channel` pisze plik opisu od startu
nagrania i przy każdej nowej części (`inProgress`). Naprawa przepisuje tylko część przerwaną (bez `endMs` albo ostatnią);
przy przepisywaniu każda ścieżka musi mieć własną kolejkę, inaczej demukser i muxer blokują się nawzajem. Dziennik działa
w trybach testowych tylko z `ISOREC_PROFILE`. Instalator: `installer/IsoRecorder.iss` + `scripts\installer.ps1`
(Inno Setup 6 z winget). Wydanie = ZIP (przenośna) + `-setup.exe`.

Druga lokalizacja zapisu (3.2): `OutputConfig::dir2Mode` = `off / spare / mirror`. Folder zapasowy: `ChannelManager::checkDisks`
woła `Channel::switchToSpare()` (zwykłe `splitToFolder`), gdy wolne miejsce spada poniżej `spareMinMb` — przed regułą „Stop
poniżej 1 GB”. Zapis podwójny: za parserem tee (`mvt`/`mat`) rozdziela strumień na dwa `splitmuxsink` (`mux`, `mux2`).
Pułapki: (1) uszkodzony `mux2` zwraca stan „flushing”, którego `errorignore` nie zamienia — wraca on przez tee do enkodera
i zatrzymuje także zapis główny; dlatego na wejściu gałęzi kopii jest bramka (`onMirrorGate`), a flaga awarii jest
ustawiana od razu w `onBus`; (2) po awarii kopii bin zapisu nie zgłasza EOS — koniec nagrania rozpoznaje się po EOS
samego `mux` (bin zapisu ma `message-forward`); (3) komunikaty `splitmuxsink-fragment-*` z `mux2` są pomijane.
Test: `tests\dir2_test.ps1` (miejsce „kończy się” przez `--fake-free-at`, awaria kopii przez `--mirror-break-at`).

Kodeki i kontenery (3.1): `EncoderFactory` zna kodeki `h264 / h265 / av1 / prores` (`OutputConfig::codec`), parser za
enkoderem (`Choice::parser`) i zgodność kontenerów (`containerAccepts`, sprawdzona doświadczalnie: TS tylko H.264/H.265,
MP4 bez ProRes, MKV i MOV wszystko). Programowe enkodery tylko LGPL/BSD: SVT-AV1 i `avenc_prores` (szybki, profil 422);
`rav1enc` i `avenc_prores_ks` są wielokrotnie wolniejsze od czasu rzeczywistego. DNxHR: żaden muxer GStreamera nie
przyjmuje `video/x-dnxhd`. Enkoderów NVENC/AMF dla H.265 i AV1 nie dało się tu sprawdzić (brak sprzętu) — nazwy
właściwości wzięte przez analogię do H.264. Wejście DeckLink (`decklinkvideosrc` + `decklinkaudiosrc`, `Devices::decklinks`)
jest napisane bez dostępu do karty: sprawdzony jest tylko stan błędu. Ikona: `res/icon.png` (zasób Qt, okna),
`res/icon.ico` + `res/app.rc` (plik EXE), źródło: `iso recorder ikona.png`. Test: `tests\codec_test.ps1`.
Uwaga do skryptów: `build.ps1 | Select-Object -First N` przerywa budowanie w połowie — filtruj wynik przez `-Last`.

Poprawki 3.0: `Channel::reconnect()` (przycisk w zakładce Input) — jedyna droga do ponownego startu kanału porzuconego,
gdy źródło nie odpowiedziało przy starcie; `ChannelManager::sortChannels/sortedBy` (klik w tytuł kolumny listy);
`Channel::setFps` i tryby kamery `"WxH@licznik/mianownik"` (`Devices`); `Lang::buttonTranslator()` — `QTranslator`
dla kontekstu `QPlatformTheme`, dzięki któremu standardowe przyciski Qt biorą teksty z tabeli `Lang`.

Kopia strumienia (2.9): `Channel::copyMode()` = enkoder `"copy"` i wejście SRT/RTSP. Kanał ma wtedy inny tor źródła
(`sourceDescription`): `urisourcebin parse-streams=true` zamiast `fallbacksrc`; pady łączy `onCopyPad` → `linkCopyPad`
(dopiero przy zdarzeniu CAPS — w `pad-added` znany jest tylko szablon). Strumień skompresowany idzie przez tee `cv`/`ca`
(stąd gałąź zapisu: sam parser + `splitmuxsink`) i przez dekoder do `vraw`/`araw` (dalej wspólny tor: `tv`/`ta`, podgląd,
wyjście na żywo). Pułapki: (1) znaczniki SRT liczą się od początku strumienia — `onCopyTap` mierzy raz przesunięcie do
zegara pipeline'u (`m_copyOffset`), a `recBuffer` w trybie kopii liczy czas po DTS; (2) start i wznowienie czekają na
klatkę kluczową (`m_copyNeedKey`), dźwięk czeka na obraz; (3) `tsdemux` dostaje `skew-corrections=false`, inaczej znaczniki
mają kilka ms nierównomierności; (4) nie ma `fallbacksrc`, więc zanik wykrywa `pollSource` (4 s bez klatek) i woła
`sourceFailed`; nagranie wznawia się po połączeniu (`m_autoRecord` w `pollSource`, nie w timerze ponowień); (5) źródło bez
dźwięku: `plugCopySilence` podłącza ciszę do podglądu, plik jest bez dźwięku; (6) testowy nadajnik ffmpeg (SRT listener)
przyjmuje jedno połączenie — przełączenie enkodera na „copy” restartuje kanał i taki nadajnik się kończy.
Test: `tests\copy_test.ps1` — parametry przekazuj przez tablicę skrótów (`@{Type='srt';Split=$true}`), nie przez listę.

Scheduler (2.8): reguły to okna „od – do” (`core/Schedule.*`: `windowAt`, `nextWindow`, zapis JSON, `selfTest`). Silnik to
`ChannelManager::schedulerTick()` co 250 ms na zegarze ściennym; stan na kanał w `m_sched` (`handled` = początek okna już
obsłużonego — to daje idempotencję, `stopAt` = koniec okna). Dokładność: `startRecording` jest wołane do 2 s przed oknem,
a `beginRecordingAt` z dokładnego timera o początku okna (wspólna chwila dla grupy); Stop też z dokładnego timera.
Wyjście na żywo w trybie `live.when = "schedule"` otwiera `Channel::setLiveGate()`. Blokada uśpienia:
`SetThreadExecutionState` w tym samym takcie. Zakładka: `Inspector::buildSchedulePage` — lista jest poprawiana w miejscu,
gdy liczba reguł się nie zmienia (inaczej edytowane pole traciłoby fokus). Test: `tests\sched_test.ps1`.

Wejście wyłączone (2.7): typ `"none"` (`Channel::inputOff()`). `startPreview` nie buduje wtedy pipeline'u — kanał zostaje
w stanie Idle z flagą `m_wantPreview`, dzięki której `restart()` uruchomi go po zmianie typu wejścia.

Nazwa kanału wg źródła (2.6): `Inspector::commitInput` → `suggestedName` / `offerRename`; ustawienie
`AppSettings::renameOnSource()` (ask / always / never). Domyślna nazwa = `TEST n` albo nazwa nadana automatycznie w tej
sesji (`m_autoNames`). Pierwszy człon to krótka nazwa typu (Screen / Window / Camera / SRT / RTSP / NDI).

## 5. Stałe zasady pracy (ustalone z właścicielem)

1. **Każda zmiana = wersja.** Numeracja co 0.1. Każda runda poprawek po uwagach dostaje własny numer, a pozycje
   planu poniżej przesuwają się o jedną. Przy przesunięciu popraw: tabelę planu w `instrukcja_koncowa.md`,
   stałe `Roadmap` w `src/core/Lang.h` (napisy „od wersji X” w interfejsie) i dwa odwołania w rozdz. 8.
2. **Rutyna wydania**, w tej kolejności:
   - `CMakeLists.txt`: `project(... VERSION x.y.0)` i `APP_VERSION_SHORT`;
   - wpis w `CHANGELOG.md` (po polsku, dla użytkownika: co się zmieniło, test, czego jeszcze nie ma);
   - aktualizacja `instrukcja_koncowa.md` (plan + opis mechanizmu, jeśli doszedł nowy);
   - zrzut okna do `docs/zrzut_<wersja>.png` — **bez zawartości ekranu właściciela** (używaj generatora testowego);
   - commit `Wersja x.y: …`, tag `vx.y`, `git push origin main --tags` (repo **publiczne** `studiokrause/iso-recorder`, zdalne `origin`; dawne repo z historią sprzed 2026‑11‑12 jest prywatne jako `studiokrause/iso-recorder-private`, zdalne `old` — nic tam nie wypychaj). Repo jest publiczne: przed każdym pushem sprawdź, że w commitach nie ma danych prywatnych; folder `/media/` (zdjęcia Pixabay do makiety) jest tylko lokalny i ignorowany;
   - od 2.0 także **Release na GitHubie**: `dist\IsoRecorder` spakowane do `IsoRecorder-<wersja>-win64.zip`,
     `gh release create vx.y <zip> --title "ISO Recorder x.y" --notes-file <notatki>` (notatki = wpis z CHANGELOG);
   - `scripts\package.ps1` → `dist\IsoRecorder\` (poza repo), potem testy na paczce z `-Exe dist\IsoRecorder\IsoRecorder.exe`.
3. **Commity**: autor `studiokrause <15332134+studiokrause@users.noreply.github.com>`. Wiadomości commitów bez polskich
   znaków. **Autorzy programu** (README, okno About, notatki wydania): „Studio Krause + Claude Fable 5.2” — **bez adresu
   e‑mail** (decyzja właściciela z 2026‑11‑11). Prywatnego adresu właściciela nie wpisuj nigdzie i nigdzie nie wysyłaj.
4. **Jeden język interfejsu naraz.** Każdy nowy napis: klucz po angielsku przez `T("…")` + wiersz w `src/core/Lang.cpp`
   z pięcioma tłumaczeniami. Po dodaniu napisów obejrzyj zrzut w PL i w DE lub FR (dłuższe teksty).
5. **Raportuj uczciwie**: co sprawdzone i czym, a czego nie. Kliknięć myszą nie da się tu testować — pisz to wprost.
   Wejście kamery i DeckLink nie są sprawdzone na sprzęcie (na maszynie są tylko wirtualne kamery).
6. **Nie ruszaj działającego programu właściciela.** `package.ps1` składa paczkę w `dist\IsoRecorder.new` i podmienia
   starą tylko wtedy, gdy żaden proces z niej nie działa (kod wyjścia 2 = nie podmieniono). Zabijaj tylko procesy,
   które sam uruchomiłeś.

## 6. Środowisko

- Windows 10 LTSC, 6 rdzeni logicznych, tylko Intel Quick Sync (enkoder `qsvh264enc`), dwa monitory.
- MSVC (VS Build Tools 18), CMake + Ninja, Qt 6.10.3 w `C:\Qt\6.10.3\msvc2022_64`.
- GStreamer 1.28.8 MSVC x64 w `%LOCALAPPDATA%\Programs\gstreamer\1.0\msvc_x86_64` — **bez** zmiennej
  `GSTREAMER_1_0_ROOT_MSVC_X86_64`; skrypty szukają tej ścieżki same.
- NDI 6 Tools i OBS są zainstalowane; `ffmpeg` / `ffprobe` w `D:\VideoTools\ffmpeg`.
- Program startuje zmaksymalizowany; uruchomienia z argumentami testowymi mają stałe okno 1600×900 (zrzuty).
- Paczka (`package.ps1`) pomija wtyczki GPL z instalacji GStreamera — nie dodawaj zależności od `x264enc` / `x265enc`.
- Budowanie: `scripts\build.ps1` (komunikat „'vswhere.exe' is not recognized” jest nieszkodliwy).
- Wydajność: 8 × 720p25 nagrywanych = ok. 2,3 rdzenia; 16 kanałów jest blisko granicy tej maszyny.
  **Mierz to po zmianach w torze sygnału** (patrz pułapka 7.6).

## 7. Pułapki, które już kosztowały czas

1. **Heredoc w Bashu** psuje apostrofy i `\n`. Większe zmiany w źródłach rób skryptem Pythona zapisanym narzędziem
   do plików i uruchamianym `python -X utf8`; w ścieżkach używaj `/` albo `\\`.
2. **PowerShell 5.1**: `$Input` i `$PROFILE` to zmienne automatyczne (nie nazywaj tak parametrów), `Try` to słowo
   kluczowe; stderr programu przy `$ErrorActionPreference='Stop'` przerywa skrypt (`ffprobe -v quiet`).
3. **`QApplication::quit()` w Qt 6 wywołuje `closeEvent`** — okno z pytaniem („Zapisać zmiany?”) zawiesi każdy test.
   Uruchomienia sterowane argumentami ustawiają właściwość aplikacji `automation`, która wyłącza pytania.
4. **Testy nie mogą dotykać ustawień właściciela.** Tryby `--demo` / `--list` wyłączają autozapis i pamięć list;
   gdy test ich potrzebuje, ustaw `ISOREC_PROFILE=<nazwa>` (osobny klucz rejestru i folder) i posprzątaj po sobie.
5. **Segmenty czasu.** Źródło sieciowe po ponownym połączeniu zaczyna nowy segment. Gałąź zapisu ma własną oś czasu:
   sondy zapamiętują segment źródła, podmieniają zdarzenie SEGMENT na stały segment od zera i przeliczają znaczniki
   na running‑time. Samo przepisywanie PTS kończy się odrzucaniem klatek przez enkoder.
6. **Nie zatrzymuj zdarzenia RECONFIGURE** idącego od `tee` do źródła. „Osłona źródła” z 1.6 zablokowała uzgodnienie
   formatu, podniosła zużycie procesora o 45% i rozregulowała 16 kanałów. Po każdej zmianie w torze sygnału porównaj
   zużycie procesora z poprzednią wersją (worktree z tagu + ten sam przebieg).
7. **Zmiany stanu GStreamera mogą nie wrócić** (sterownik kamery bez sygnału). Start i stop pipeline'u idą przez
   `setStateTimed()`; nie wołaj `gst_element_set_state` na całym pipeline'ie wprost z wątku okna.
8. **`splitmuxsink`**: muxer przez właściwość `muxer` ustawioną przed żądaniem padów; `async-finalize=false`.
9. **`gst_parse_launch`**: format z nawiasami (`video/x-raw(memory:D3D11Memory)`) tylko jako
   `capsfilter caps="…"`; zapis skrócony to błąd składni.
10. **Odbiornik NDI potrzebuje ok. 4 s** na znalezienie źródła; test, który czeka krócej, daje fałszywy błąd.
11. **Wynik testu to nie dowód.** Dwa razy „test przeszedł”, bo przez błąd parametru badał coś innego niż miał.
    Gdy wynik jest podejrzanie dobry albo sprzeczny z innym — zrób ślad (jasność klatek co sekundę, znaczniki czasu).
13. **Gałąź z `errorignore` nie izoluje awarii sinka**: błąd karty dźwiękowej w gałęzi odsłuchu zatrzymał tor dźwięku
    kanału i zepsuł nagranie (FLUSHING nie jest zamieniany). Wyjście, którego awaria nie może dotknąć nagrania, rób jako
    osobny pipeline zasilany z sondy (`appsrc`), tak jak odsłuch od 2.2.
14. **Zrzuty i testy nie mogą zapisywać ustawień.** Okna otwierane do zrzutu (`--shot-*`) działają na prawdziwym
    profilu, jeśli nie ustawisz `ISOREC_PROFILE`; przełączniki zmieniane kodem blokuj `QSignalBlocker`.
15. **Odsłuch w testach gra z głośników właściciela.** Testuj z `--audio none` (cisza) albo z kartą podaną przez
    `--monitor-device`.
12. **Szyna GStreamera**: handler synchroniczny zwraca `GST_BUS_DROP` i przekazuje wynik do wątku okna przez
    `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`; nigdzie nie wolno używać `gst_bus_pop` na tej szynie.

## 8. Testy (PowerShell, wymagają `ffprobe`)

| Skrypt | Co sprawdza |
|---|---|
| `tests\rec_test.ps1` | nagrywanie: `-Channels -Seconds -Cycles`, split (`-SplitEvery`, `-AutoSplit`), `-Container ts`, pauza (`-PauseAt -PauseFor -PauseCount`), przełączanie wyjścia (`-LiveCycle`), źródła (`-Source screen`, `-Audio loopback`) |
| `tests\sync_test.ps1` | wspólny start / pauza / stop grupy, domyślnie 16 kanałów |
| `tests\live_test.ps1` | odbiór wyjścia SRT (ffprobe) i NDI w trakcie nagrania |
| `tests\net_test.ps1` | wejście SRT / RTSP / NDI z zanikiem i powrotem nadajnika (`build\rtsp_test_server.exe`; NDI: gst-launch) |
| `tests\growing_test.ps1` | growing file: MP4 / MOV czytelne i rosnące w trakcie zapisu, split, pauza, zapis podwójny, awaria |
| `tests\recovery_test.ps1` | odzyskiwanie po zabiciu programu w trakcie nagrania: MKV / TS / MP4 |
| `tests\dir2_test.ps1` | druga lokalizacja: zapis podwójny, awaria drugiego folderu, folder zapasowy |
| `tests\codec_test.ps1` | kodeki H.264 / H.265 / AV1 / ProRes w MKV / TS / MP4 / MOV, split i pauza, ProRes 1080p |
| `tests\copy_test.ps1` | zapis bez ponownego kodowania SRT / RTSP: plik zgodny ze strumieniem, split, pauza, zanik |
| `tests\sched_test.ps1` | harmonogram: obliczenia okien, start co do klatki, podział, spóźniony start, wyjście na żywo w oknie |
| `tests\list_test.ps1` | lista kanałów: zapis → wczytanie → identyczny plik; autozapis, przywracanie, ostatnia lista |

Każdy skrypt przyjmuje `-Exe` (test paczki). Przed wydaniem uruchom przynajmniej: `rec_test` z pauzą i splitem,
`sync_test`, `list_test`, a przy zmianach w torze sygnału także `net_test` i `live_test`.

Program ma argumenty do testów i zrzutów (pełna lista w komentarzu na początku `src/main.cpp`), m.in.
`--demo N`, `--rec-test S`, `--screenshot plik`, `--shot-delay ms`, `--view list`, `--tab N`, `--lang xx`,
`--input screen|camera|srt|rtsp`, `--live ndi|srt`, `--list plik`, `--empty`.

## 9. Układ kodu

```
src/main.cpp            start, argumenty testowe, paczka samodzielna (wtyczki GStreamera obok EXE)
src/core/Channel        jeden kanał: źródła, podgląd, gałąź nagrywania, pauza, split, wyjście na żywo, zanik sygnału
src/core/ChannelManager lista kanałów, operacje grupowe, sesja i listy, autozapis, pilnowanie dysków
src/core/Lang           tłumaczenia (T("…")) i stałe Roadmap
src/core/DiskMonitor, DiskTest, EnvCheck
src/media/EncoderFactory  wybór enkodera próbą;  src/media/Devices  ekrany, kamery, urządzenia dźwięku
src/util/FileNameTemplate szablon nazw plików
src/app/MainWindow      nagłówek, panel sterowania, pasek stanu, przełączanie widoków
src/app/ChannelGrid + ChannelTile   widok kafli (malowany QPainterem, przeciąganie)
src/app/ChannelList     widok listy (nagłówek z przyciskami grupowymi, wiersze)
src/app/Inspector       zakładki Input / Output;  PreviewDialog  duży podgląd;  DisksDialog;  Widgets;  Theme
tests/                  skrypty testowe; tests/tools/rtsp_test_server.cpp
scripts/                build.ps1, run.ps1, package.ps1
```
