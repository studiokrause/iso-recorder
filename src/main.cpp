#include <QApplication>
#include <QIcon>
#include <QFile>
#include <QMessageBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDir>
#include <QAbstractItemView>
#include <QComboBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QStandardPaths>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTimer>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <gst/gst.h>
#include <windows.h>

#include "app/ChannelTile.h"
#include "app/DisksDialog.h"
#include "app/Inspector.h"
#include "app/MainWindow.h"
#include "app/PreviewDialog.h"
#include "app/RemoteControl.h"
#include "core/AppSettings.h"
#include "app/Theme.h"
#include "app/Widgets.h"
#include "core/ChannelManager.h"
#include "core/DiskTest.h"
#include "core/EnvCheck.h"
#include "core/EventLog.h"
#include "core/Lang.h"
#include "core/Recovery.h"
#include "app/Splash.h"
#include "media/Devices.h"

// Argumenty:
//   --check-env           raport o elementach GStreamera (kod wyjscia 1, gdy brakuje obowiazkowych)
//   --demo N              start z N kanalami testowymi
//   --lang xx             jezyk interfejsu na te sesje (en, pl, de, es, it, fr)
//   --view list|2|3|4     widok kanalow na te sesje
//   --shot-combo N plik   rozwin N-ta liste rozwijana inspektora (od 0), zapisz jej zrzut, wypisz polozenie wzgledem ekranu
//   --screenshot plik.png zrzut okna po 4 s i wyjscie (do porownania z mockupem)
//   --rec-test S          test nagrywania: wszystkie kanaly nagrywaja S sekund, potem Stop i wyjscie po finalizacji
//   --cycles N            ile razy powtorzyc cykl Rec/Stop w --rec-test (domyslnie 1)
//   --out folder          folder nagran dla kanalow --demo
//   Shift przy starcie    nie otwieraj ostatniej listy ani autozapisu (start z pusta lista, gdy lista sprawia klopot)
//   --list plik           wczytaj liste kanalow (tryb testowy: bez autozapisu i pamieci ostatnich list)
//   --save-list plik      po 2,5 s zapisz kopie biezacej listy do pliku i wyjdz
//   --persist             z --demo / --list: mimo trybu testowego wlacz autozapis i pamiec ostatnich list
//   --restore             przywroc liste z autozapisu bez pytania
//   --save-list-real plik po 2,5 s zapisz liste tak jak przycisk SAVE (staje sie biezaca i ostatnia) i wyjdz
//   Zmienna ISOREC_PROFILE=nazwa: osobny profil ustawien i autozapisu (testy nie dotykaja prawdziwych ustawien).
//   --container ts|mkv    kontener dla kanalow --demo
//   --input screen [--audio loopback|device|none]  kanaly --demo biora obraz z ekranu (kolejne monitory) i dzwiek z WASAPI
//   --input srt|rtsp|ndi --uri adres   kanaly --demo odbieraja strumien sieciowy (wszystkie ten sam adres; ndi: nazwa zrodla)
//   --loss pause|split    reakcja kanalow --demo na zanik sygnalu sieciowego
//   --input window --window tytul  kanaly --demo przechwytuja okno o tym tytule (albo zawierajace ten tekst)
//   --shot-log plik       otworz okno logu zdarzen po 6 s, zapisz jego zrzut i wyjdz
//   --shot-settings plik  otworz okno ustawien po 3 s, zapisz jego zrzut i wyjdz
//   --pattern N           wzor generatora dla kanalow --demo (100 zegar cyfrowy, 101 analogowy)
//   --monitor N           wlacz odsluch kanalu N (od 1) po 4 s
//   --split-folder-every S  w --rec-test: co S sekund split wszystkich kanalow do nowego folderu
//   --live ndi|srt [port] wyjscie na zywo dla kanalow --demo: NDI o nazwie "ISO <kanal>" albo SRT na portach port+1, port+2...
//   --auto-split S        auto-split co S sekund dla kanalow --demo
//   --split-every S       w --rec-test: reczny split wszystkich kanalow co S sekund
//   --live-at S           z --live: wyjscie wlaczane dopiero S sekund po starcie nagrania (zamiast od razu)
//   --live-cycle S        z --live: w trakcie nagrania wyjscie jest co S sekund na przemian wlaczane i wylaczane
//   --pause-at S --pause-for P [--pause-count N]  w --rec-test: N pauz po P sekund, kazda po S sekundach nagrywania
//   --drag-test A B [plik] przeciagniecie kafla A na miejsce kafla B (numery od 1) zdarzeniami myszy;
//                         kolejnosc kanalow na stdout, opcjonalny zrzut okna w trakcie przeciagania
//   --shot-big N plik.png zrzut okna duzego podgladu kanalu N (od 1), otwartego dwuklikiem w kafel;
//                         z --big-rec: przed zrzutem klikniecie REC w oknie, po zrzucie STOP (stan kanalu na stdout)
//   --sched-selftest      sprawdzenie obliczen harmonogramu (zmiana czasu, okna przez polnoc); kod wyjscia = liczba bledow
//   --sched-in S --sched-len L  kanaly --demo dostaja regule jednorazowa: start za S sekund (ujemne = okno juz trwa), L sekund
//   --sched-split N       ta regula dzieli plik co N sekund;  --sched-nocatch: bez spoznionego startu
//   --sched-live          z --live: wyjscie na zywo w trybie "jak w harmonogramie"
//   --sched-report        co sekunde na stdout: czas, stan kanalu 1, czy jego wyjscie na zywo nadaje
//   --quit-after S        wyjscie po S sekundach (po zamknieciu plikow nagran)
//   --disk-test folder [S] test predkosci zapisu (domyslnie 10 s), wynik na stdout
// Paczka samodzielna: gdy obok EXE lezy katalog gstreamer-1.0\, GStreamer bierze wtyczki tylko stamtad
// (instalacja systemowa jest ignorowana), a rejestr wtyczek trafia do %LOCALAPPDATA%\IsoRecorder.
static void useBundledGStreamer() {
    namespace fs = std::filesystem;
    wchar_t exe[MAX_PATH * 4];
    if (!GetModuleFileNameW(nullptr, exe, DWORD(std::size(exe)))) return;
    const fs::path dir = fs::path(exe).parent_path();
    std::error_code ec;
    if (!fs::is_directory(dir / L"gstreamer-1.0", ec)) return;
    _wputenv_s(L"GST_PLUGIN_SYSTEM_PATH_1_0", (dir / L"gstreamer-1.0").c_str());
    _wputenv_s(L"GST_PLUGIN_PATH_1_0", L"");
    _wputenv_s(L"GST_PLUGIN_PATH", L"");
    _wputenv_s(L"GST_PLUGIN_SCANNER_1_0", (dir / L"gst-plugin-scanner.exe").c_str());
    _wputenv_s(L"GIO_MODULE_DIR", (dir / L"gio" / L"modules").c_str());
    wchar_t local[MAX_PATH * 4];
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", local, DWORD(std::size(local)))) {
        const fs::path data = fs::path(local) / L"IsoRecorder";
        fs::create_directories(data, ec);
        _wputenv_s(L"GST_REGISTRY_1_0", (data / L"gst-registry.bin").c_str());
    }
}

int main(int argc, char* argv[]) {
    SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
    useBundledGStreamer();
    gst_init(&argc, &argv);
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("IsoRecorder"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icon.png")));
    app.installTranslator(Lang::buttonTranslator());     // OK / Cancel / Yes / No w jezyku interfejsu
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    const QStringList args = app.arguments();
    // Uruchomienie sterowane argumentami (zrzuty, testy) konczy sie samo - bez pytan o zapis listy.
    for (const char* a : {"--screenshot", "--save-list", "--save-list-real", "--rec-test", "--shot-big", "--shot-disks", "--drag-test", "--shot-combo", "--shot-log", "--shot-settings", "--list-ndi", "--shot-rename", "--quit-after", "--recover", "--remote-selftest"})
        if (args.contains(QLatin1String(a))) app.setProperty("automation", true);

    // Ekran powitalny: 2,5 s przy zwyklym uruchomieniu. Nie pojawia sie w trybach testowych i narzedziowych.
    // --shot-splash plik: zapisuje obraz ekranu powitalnego do pliku i konczy (sprawdzenie, ze zasob jest w programie).
    {
        bool plain = !app.property("automation").toBool() && !args.contains(QStringLiteral("--no-splash"));
        for (const char* a : {"--demo", "--list", "--check-env", "--disk-test", "--sched-selftest", "--recover"})
            plain = plain && !args.contains(QLatin1String(a));
        const int shotAt = int(args.indexOf(QStringLiteral("--shot-splash")));
        const QImage image(QStringLiteral(":/splash.png"));
        if (shotAt >= 0 && shotAt + 1 < args.size()) return image.save(args.at(shotAt + 1)) ? 0 : 1;
        if (plain) {
            // obraz ma 1000 x 420 punktow; na ekranie o wiekszej gestosci jest odpowiednio powiekszany
            const qreal dpr = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
            Splash::show(dpr > 1.01 ? image.scaled(image.size() * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation) : image, 2500);
        }
    }

    if (args.contains(QStringLiteral("--check-env"))) {
        const EnvCheck::Result r = EnvCheck::run();
        // Gdy stdout jest przekierowany do pliku/potoku, zostaje; w przeciwnym razie piszemy do konsoli rodzica.
        const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        const bool redirected = out && out != INVALID_HANDLE_VALUE && GetFileType(out) != FILE_TYPE_UNKNOWN;
        if (!redirected && AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE* f = nullptr;
            freopen_s(&f, "CONOUT$", "w", stdout);
            SetConsoleOutputCP(CP_UTF8);
        }
        const QByteArray utf8 = r.report.toUtf8();
        fwrite(utf8.constData(), 1, size_t(utf8.size()), stdout);
        fflush(stdout);
        return r.requiredOk ? 0 : 1;
    }

    if (args.contains(QStringLiteral("--sched-selftest"))) {
        int failed = 0;
        const QByteArray utf8 = Schedule::selfTest(&failed).toUtf8();
        fwrite(utf8.constData(), 1, size_t(utf8.size()), stdout);
        fflush(stdout);
        return failed;
    }

    const int diskAt = int(args.indexOf(QStringLiteral("--disk-test")));
    if (diskAt >= 0 && diskAt + 1 < args.size()) {
        const int seconds = diskAt + 2 < args.size() && args.at(diskAt + 2).toInt() > 0 ? args.at(diskAt + 2).toInt() : 10;
        DiskTest test(args.at(diskAt + 1), seconds, 4);
        DiskTestResult result;
        QObject::connect(&test, &DiskTest::done, &app, [&](const DiskTestResult& r) { result = r; app.quit(); });
        test.start();
        app.exec();
        test.wait();
        const QByteArray line = (result.ok ? QStringLiteral("ok avg=%1 MB/s worst=%2 MB/s longest_write=%3 ms seconds=%4 streams=%5\n")
                                                 .arg(result.avgMBs, 0, 'f', 1).arg(result.worstMBs, 0, 'f', 1)
                                                 .arg(result.longestWriteMs).arg(result.seconds).arg(result.streams)
                                           : QStringLiteral("error: %1\n").arg(result.error)).toUtf8();
        fwrite(line.constData(), 1, size_t(line.size()), stdout);
        fflush(stdout);
        return result.ok ? 0 : 1;
    }

    // Strzalki list rozwijanych: dwa male obrazki rysowane raz i zapisywane w katalogu danych programu.
    auto arrowFile = [](const QString& name, const QColor& color) {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/IsoRecorder");
        QDir().mkpath(dir);
        const QString path = dir + QLatin1Char('/') + name;
        QPixmap pm(18, 12);                     // 2x, zeby byla ostra takze przy skalowaniu ekranu
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPolygon(QPolygonF() << QPointF(1, 1) << QPointF(17, 1) << QPointF(9, 11));
        p.end();
        pm.save(path, "PNG");
        return path;
    };
    app.setStyleSheet(Theme::styleSheet(arrowFile(QStringLiteral("combo_arrow.png"), QColor(0x9a, 0x9a, 0x9a)),
                                        arrowFile(QStringLiteral("combo_arrow_off.png"), QColor(0x55, 0x55, 0x55))));
    QSettings settings(ChannelManager::settingsOrg(), QStringLiteral("IsoRecorder"));
    const int langAt = int(args.indexOf(QStringLiteral("--lang")));
    Lang::set(langAt >= 0 && langAt + 1 < args.size() ? args.at(langAt + 1)
                                                       : settings.value(QStringLiteral("language"), QStringLiteral("en")).toString());
    int rc = 0;
    {
        ChannelManager manager;
        // Log zdarzen: wszystkie komunikaty programu; do pliku tylko przy zwyklym uruchomieniu (testy - sama pamiec).
        QObject::connect(&manager, &ChannelManager::notice, &manager, [](const QString& text) { EventLog::add(text); });
        // Zmiana jezyka buduje okno od nowa; kanaly i ich pipeline'y zostaja nietkniete.
        // Zdalne sterowanie (OSC / MIDI): jedno na caly program, przezywa przebudowe okna przy zmianie jezyka.
        RemoteControl remote(&manager);
        QObject::connect(&remote, &RemoteControl::notice, &manager, [](const QString& text) { EventLog::add(text); });
        std::unique_ptr<MainWindow> w;
        std::function<void()> makeWindow = [&] {
            MainWindow* old = w.release();
            w = std::make_unique<MainWindow>(&manager);
            remote.setWindow(w.get());
            QObject::connect(w.get(), &MainWindow::settingsSaved, &remote, [&remote] { remote.apply(AppSettings::remoteConfig()); });
            QObject::connect(w.get(), &MainWindow::languageRequested, &manager, [&](const QString& code) {
                Lang::set(code);
                settings.setValue(QStringLiteral("language"), Lang::current());
                makeWindow();
            }, Qt::QueuedConnection);
            if (old) {
                w->setGeometry(old->geometry());
                if (old->isMaximized()) w->showMaximized(); else w->show();
                old->hide();
                old->deleteLater();
            } else if (app.property("automation").toBool()) {
                w->show();              // testy i zrzuty: staly rozmiar 1600 x 900
            } else {
                w->showMaximized();     // zwykle uruchomienie: okno na caly ekran
            }
        };
        makeWindow();

        // Zwykle uruchomienie: autozapis, propozycja przywrocenia listy po awarii, otwarcie ostatniej listy.
        const bool testMode = (args.contains(QStringLiteral("--demo")) || args.contains(QStringLiteral("--list"))) &&
                              !args.contains(QStringLiteral("--persist"));
        // Awaryjny start z pusta lista: Shift trzymany przy uruchomieniu albo argument --empty.
        const bool safeStart = args.contains(QStringLiteral("--empty")) || (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        if (!testMode)
            EventLog::enableFile(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Char('/') +
                                 ChannelManager::settingsOrg() + QStringLiteral("/logs"));
        EventLog::add(T("Program started — version %1").arg(QLatin1String(APP_VERSION)));
        // Dziennik nagran w toku (odzyskiwanie po awarii): w trybach testowych tylko z osobnym profilem ustawien.
        Recovery::setEnabled(!testMode || !qEnvironmentVariableIsEmpty("ISOREC_PROFILE"));
        if (args.contains(QStringLiteral("--recover"))) {       // --recover: napraw przerwane nagrania bez pytania i wyjdz
            const int n = int(Recovery::pending().size());
            w->offerRecovery(false);
            const QByteArray line = "recovered=" + QByteArray::number(n) + " left=" + QByteArray::number(Recovery::pending().size()) + "\n";
            fwrite(line.constData(), 1, size_t(line.size()), stdout);
            fflush(stdout);
            QTimer::singleShot(300, &app, [] { QApplication::quit(); });
        } else if (!app.property("automation").toBool() && !testMode) {
            QTimer::singleShot(3000, w.get(), [&w] { w->offerRecovery(); });     // po ekranie powitalnym
        }
        if (!testMode && safeStart) {
            manager.setPersistence(true);
            manager.discardAutosave();
        } else if (!testMode) {
            manager.setPersistence(true);
            const QString last = settings.value(QStringLiteral("lastList")).toString();
            if (QFile::exists(ChannelManager::autosavePath())) {
                Splash::close();        // pytanie nie moze sie schowac pod ekranem powitalnym
                if (args.contains(QStringLiteral("--restore")) || QMessageBox::question(w.get(), QStringLiteral("ISO Recorder"),
                        T("The program did not close properly last time. Restore the unsaved channel list?")) == QMessageBox::Yes)
                    manager.restoreAutosave();
                else
                    manager.discardAutosave();
            } else if (settings.value(QStringLiteral("openLastList"), true).toBool() && !last.isEmpty() && QFile::exists(last)) {
                manager.loadList(last);
            }
        }
        // --list-ndi plik: po 7 s zapisuje do pliku nazwy zrodel NDI widocznych w sieci i konczy program (test wyszukiwania)
        if (const int ndiAt = int(args.indexOf(QStringLiteral("--list-ndi"))); ndiAt >= 0 && ndiAt + 1 < args.size()) {
            Devices::ndiSources();
            const QString path = args.at(ndiAt + 1);
            QTimer::singleShot(7000, &app, [path] {
                QFile f(path);
                if (f.open(QIODevice::WriteOnly))
                    for (const Devices::Info& d : Devices::ndiSources()) f.write(d.name.toUtf8() + '\n');
                f.close();
                QCoreApplication::quit();
            });
        }
        if (const int listAt = int(args.indexOf(QStringLiteral("--list"))); listAt >= 0 && listAt + 1 < args.size()) {
            QString err;
            if (!manager.loadList(args.at(listAt + 1), &err)) {
                const QByteArray line = (QStringLiteral("error: ") + err + QLatin1Char('\n')).toUtf8();
                fwrite(line.constData(), 1, size_t(line.size()), stdout);
                fflush(stdout);
            }
        }
        const int demoAt = int(args.indexOf(QStringLiteral("--demo")));
        if (demoAt >= 0 && demoAt + 1 < args.size()) {
            const int n = qBound(0, args.at(demoAt + 1).toInt(), ChannelManager::MaxChannels);
            for (int i = 0; i < n; ++i) manager.addChannel();
            if (n > 0) {
                manager.setActive(manager.channels().first());
                for (int i = 0; i < qMin(n, 4); ++i) manager.channels().at(i)->setSelected(true);
                // kilka kanalow o innych proporcjach - widac wpisanie w kadr 16:9
                const struct { int index, w, h; } odd[] = {{2, 1024, 768}, {3, 720, 1280}, {6, 1920, 800}};
                for (const auto& o : odd)
                    if (o.index < n) manager.channels().at(o.index)->setFormat(o.w, o.h);
            }
        }
        auto argAfter = [&args](const char* name) {
            const int at = int(args.indexOf(QLatin1String(name)));
            return at >= 0 && at + 1 < args.size() ? args.at(at + 1) : QString();
        };
        if (argAfter("--input") == QLatin1String("window")) {
            for (Channel* c : manager.channels()) {
                InputConfig in = c->config().in;
                in.type = QStringLiteral("window");
                in.windowTitle = argAfter("--window");
                c->setInput(in);
            }
        }
        if (argAfter("--input") == QLatin1String("srt") || argAfter("--input") == QLatin1String("rtsp") || argAfter("--input") == QLatin1String("ndi") ||
            argAfter("--input") == QLatin1String("none") || argAfter("--input") == QLatin1String("decklink") ||
            argAfter("--input") == QLatin1String("whep")) {
            for (Channel* c : manager.channels()) {
                InputConfig in = c->config().in;
                in.type = argAfter("--input");
                in.uri = argAfter("--uri");
                if (!argAfter("--encoder").isEmpty()) {     // enkoder przed wejsciem: kanal od razu laczy sie wlasciwym torem
                    OutputConfig o = c->config().out;
                    o.encoder = argAfter("--encoder");
                    c->setOutput(o);
                }
                if (!argAfter("--loss").isEmpty()) in.lossPolicy = argAfter("--loss");
                c->setInput(in);
            }
        }
        if (argAfter("--input") == QLatin1String("camera")) {      // --camera N: numer kamery z listy urzadzen (od 0)
            const QList<Devices::Info> cams = Devices::list().cameras;
            const int n = argAfter("--camera").toInt();
            for (Channel* c : manager.channels()) {
                InputConfig in = c->config().in;
                in.type = QStringLiteral("camera");
                if (n >= 0 && n < cams.size()) {
                    in.cameraPath = cams.at(n).id;
                    in.cameraName = cams.at(n).name;
                }
                c->setInput(in);
            }
        }
        if (argAfter("--input") == QLatin1String("screen") || !argAfter("--audio").isEmpty()) {
            int i = 0;
            for (Channel* c : manager.channels()) {
                InputConfig in = c->config().in;
                if (argAfter("--input") == QLatin1String("screen")) {
                    in.type = QStringLiteral("screen");
                    in.screenIndex = i++ % qMax(1, int(QGuiApplication::screens().size()));
                }
                if (!argAfter("--audio").isEmpty()) in.audioType = argAfter("--audio");
                c->setInput(in);
            }
        }
        if (const QString live = argAfter("--live"); !live.isEmpty()) {
            const int liveAt = int(args.indexOf(QStringLiteral("--live")));
            const int port = liveAt + 2 < args.size() && args.at(liveAt + 2).toInt() > 0 ? args.at(liveAt + 2).toInt() : 9700;
            const int liveLater = argAfter("--live-at").toInt(), liveCycle = argAfter("--live-cycle").toInt();
            int i = 0;
            for (Channel* c : manager.channels()) {
                LiveConfig l;
                l.type = live;
                l.ndiName = QStringLiteral("ISO ") + c->config().name;
                l.srtAddress = QStringLiteral(":%1").arg(port + ++i);
                if (args.contains(QStringLiteral("--sched-live"))) l.when = QStringLiteral("schedule");
                if (liveLater > 0 || liveCycle > 0) {
                    // nagranie rusza 3 s po starcie programu
                    if (liveCycle > 0) {
                        auto* t = new QTimer(c);
                        QObject::connect(t, &QTimer::timeout, c, [c, l] {
                            LiveConfig next = l;
                            if (c->liveOn()) next.type = QStringLiteral("off");
                            c->setLive(next);
                        });
                        QTimer::singleShot(3000, t, [t, liveCycle] { t->start(liveCycle * 1000); });
                    } else {
                        QTimer::singleShot(3000 + liveLater * 1000, c, [c, l] { c->setLive(l); });
                    }
                    continue;
                }
                c->setLive(l);
            }
        }
        for (Channel* c : manager.channels()) {
            OutputConfig o = c->config().out;
            if (!argAfter("--out").isEmpty()) o.dir = argAfter("--out");
            if (!argAfter("--container").isEmpty()) o.container = argAfter("--container");
            if (!argAfter("--encoder").isEmpty()) o.encoder = argAfter("--encoder");      // np. --encoder copy
            if (!argAfter("--codec").isEmpty()) o.codec = argAfter("--codec");            // h264 | h265 | av1 | prores
            if (args.contains(QStringLiteral("--growing"))) o.growing = true;             // MP4 / MOV rosnace (fragmenty)
            // --dir2 folder --dir2-mode spare|mirror [--spare-mb N]: druga lokalizacja zapisu
            if (!argAfter("--dir2").isEmpty()) o.dir2 = argAfter("--dir2");
            if (!argAfter("--dir2-mode").isEmpty()) o.dir2Mode = argAfter("--dir2-mode");
            if (argAfter("--spare-mb").toInt() > 0) o.spareMinMb = argAfter("--spare-mb").toInt();
            if (argAfter("--auto-split").toInt() > 0) o.splitSeconds = argAfter("--auto-split").toInt();
            c->setOutput(o);
        }
        if (!argAfter("--sched-in").isEmpty() && argAfter("--sched-len").toInt() > 0) {
            const QDateTime start = QDateTime::currentDateTime().addSecs(argAfter("--sched-in").toInt());
            const QDateTime stop = start.addSecs(argAfter("--sched-len").toInt());
            ScheduleRule r;
            r.weekly = false;
            r.date = start.date();
            r.start = QTime(start.time().hour(), start.time().minute(), start.time().second());
            r.stop = QTime(stop.time().hour(), stop.time().minute(), stop.time().second());
            r.splitSeconds = qMax(0, argAfter("--sched-split").toInt());
            ScheduleConfig sc;
            sc.rules = {r};
            sc.catchUp = !args.contains(QStringLiteral("--sched-nocatch"));
            for (Channel* c : manager.channels()) c->setSchedule(sc);
            const QByteArray line = "sched-start=" + QByteArray::number(QDateTime(r.date, r.start).toMSecsSinceEpoch()) + '\n';
            fwrite(line.constData(), 1, size_t(line.size()), stdout);
            fflush(stdout);
        }
        if (args.contains(QStringLiteral("--sched-report"))) {
            auto* t = new QTimer(&manager);
            auto* n = new int(0);
            QObject::connect(t, &QTimer::timeout, &manager, [&manager, n] {
                if (manager.count() == 0) return;
                Channel* c = manager.channels().first();
                const QByteArray line = QStringLiteral("t=%1 state=%2 live=%3 waiting=%4\n").arg(++*n).arg(int(c->state()))
                                            .arg(c->liveActive() ? 1 : 0).arg(c->waitingForSchedule() ? 1 : 0).toUtf8();
                fwrite(line.constData(), 1, size_t(line.size()), stdout);
                fflush(stdout);
            });
            t->start(1000);
        }
        if (const int after = argAfter("--quit-after").toInt(); after > 0) {
            QTimer::singleShot(after * 1000, &manager, [&manager] {
                manager.stopAllRecording();
                auto* wait = new QTimer(&manager);
                QObject::connect(wait, &QTimer::timeout, &manager, [&manager] {
                    if (manager.recordingCount() == 0) QApplication::quit();
                });
                wait->start(100);
            });
        }
        // --fps N: klatkaz kanalow --demo;  --sort name|source|format|state [x2]: po 2 s ulozenie listy (drugie = odwrocenie);
        // --reconnect-at S: po S sekundach "Reconnect source" na kanale 1; kolejnosc i stan kanalow ida na stdout
        // --fake-free-at S MB: po S sekundach program "widzi" tylko MB wolnego miejsca na dyskach folderow glownych
        // --mirror-break-at S: po S sekundach kopia lustrzana kanalu 1 dostaje niemozliwa sciezke (awaria drugiego dysku)
        if (const int at = argAfter("--fake-free-at").toInt(); at > 0) {
            const int fa = int(args.indexOf(QStringLiteral("--fake-free-at")));
            const qint64 mb = fa + 2 < args.size() ? args.at(fa + 2).toLongLong() : 0;
            QTimer::singleShot(at * 1000, &manager, [&manager, mb] { manager.setFakeFreeBytes(mb * 1024 * 1024); });
        }
        if (const int at = argAfter("--mirror-break-at").toInt(); at > 0)
            QTimer::singleShot(at * 1000, &manager, [&manager] {
                if (manager.count() > 0) manager.channels().first()->breakMirrorForTest();
            });
        if (const QStringList wh = argAfter("--size").split(QLatin1Char('x')); wh.size() == 2 && wh.at(0).toInt() > 0 && wh.at(1).toInt() > 0)
            for (Channel* c : manager.channels()) c->setFormat(wh.at(0).toInt(), wh.at(1).toInt());      // --size 1920x1080
        if (const int fps = argAfter("--fps").toInt(); fps > 0)
            for (Channel* c : manager.channels()) c->setFps(fps);
        if (const QString key = argAfter("--sort"); !key.isEmpty())
            QTimer::singleShot(2000, &manager, [&manager, key] {
                const SortKey k = key == QLatin1String("source") ? SortKey::Source : key == QLatin1String("format") ? SortKey::Format
                                : key == QLatin1String("state") ? SortKey::State : SortKey::Name;
                manager.sortChannels(k);
                if (QApplication::arguments().contains(QStringLiteral("x2"))) manager.sortChannels(k);
                QStringList names;
                for (Channel* c : manager.channels()) names << c->config().name;
                const QByteArray line = "order=" + names.join(QLatin1Char(',')).toUtf8() + " sorted=" + QByteArray::number(manager.sortedBy(k)) + "\n";
                fwrite(line.constData(), 1, size_t(line.size()), stdout);
                fflush(stdout);
            });
        if (const int at = argAfter("--reconnect-at").toInt(); at > 0)
            QTimer::singleShot(at * 1000, &manager, [&manager] {
                if (manager.count() == 0) return;
                Channel* c = manager.channels().first();
                c->reconnect();
                QTimer::singleShot(2500, &manager, [c] {
                    const QByteArray line = "after-reconnect state=" + QByteArray::number(int(c->state())) + "\n";
                    fwrite(line.constData(), 1, size_t(line.size()), stdout);
                    fflush(stdout);
                });
            });
        if (const int every = argAfter("--split-every").toInt(); every > 0) {
            auto* t = new QTimer(&manager);
            QObject::connect(t, &QTimer::timeout, &manager, [&manager] {
                for (Channel* c : manager.channels()) c->split();
            });
            QTimer::singleShot(3000, t, [t, every] { t->start(every * 1000); });   // liczone od startu nagrania
        }
        const int recAt = int(args.indexOf(QStringLiteral("--rec-test")));
        if (recAt >= 0 && recAt + 1 < args.size()) {
            const int seconds = qMax(1, args.at(recAt + 1).toInt());
            const int cycAt = int(args.indexOf(QStringLiteral("--cycles")));
            auto* cyclesLeft = new int(cycAt >= 0 && cycAt + 1 < args.size() ? qMax(1, args.at(cycAt + 1).toInt()) : 1);
            auto* startAll = new std::function<void()>;
            *startAll = [&manager, seconds, cyclesLeft, startAll] {
                manager.transportGroup(manager.channels(), Transport::Rec);
                const QStringList av = QApplication::arguments();
                auto num = [&av](const char* name, int def) {
                    const int at = int(av.indexOf(QLatin1String(name)));
                    return at >= 0 && at + 1 < av.size() ? av.at(at + 1).toInt() : def;
                };
                const int pauseAt = num("--pause-at", 0), pauseFor = num("--pause-for", 0), pauseCount = num("--pause-count", 1);
                for (int k = 0; pauseAt > 0 && pauseFor > 0 && k < pauseCount; ++k) {
                    const int t0 = (k * (pauseAt + pauseFor) + pauseAt) * 1000;
                    QTimer::singleShot(t0, &manager, [&manager] { manager.transportGroup(manager.channels(), Transport::Pause); });
                    QTimer::singleShot(t0 + pauseFor * 1000, &manager, [&manager] { manager.transportGroup(manager.channels(), Transport::Pause); });
                }
                QTimer::singleShot(seconds * 1000, &manager, [&manager, cyclesLeft, startAll] {
                    manager.stopAllRecording();
                    auto* wait = new QTimer(&manager);          // czekaj na finalizacje wszystkich plikow
                    QObject::connect(wait, &QTimer::timeout, &manager, [&manager, cyclesLeft, startAll, wait] {
                        if (manager.recordingCount() > 0) return;
                        wait->deleteLater();
                        if (--*cyclesLeft > 0) QTimer::singleShot(300, &manager, *startAll);
                        else QTimer::singleShot(500, &manager, [] { QApplication::quit(); });
                    });
                    wait->start(100);
                });
            };
            // --rec-delay S: nagranie rusza S sekund po starcie programu (domyslnie 3)
            QTimer::singleShot(qMax(1, argAfter("--rec-delay").isEmpty() ? 3 : argAfter("--rec-delay").toInt()) * 1000, &manager, *startAll);
        }
        // --switch-lang xx: po 2 s przelacza jezyk ta sama droga co menu (test przebudowy okna)
        const int switchAt = int(args.indexOf(QStringLiteral("--switch-lang")));
        if (switchAt >= 0 && switchAt + 1 < args.size()) {
            const QString code = args.at(switchAt + 1);
            QTimer::singleShot(2000, &manager, [&w, code] { emit w->languageRequested(code); });
        }
        // --shot-disks plik.png: zrzut okna Disks po 2,5 s i wyjscie
        if (!argAfter("--shot-disks").isEmpty()) {
            auto* dlg = new DisksDialog(&manager, w.get());
            dlg->show();
            const QString path = argAfter("--shot-disks");
            QTimer::singleShot(2500, dlg, [dlg, path] { dlg->grab().save(path); QApplication::quit(); });
        }
        if (const QString out = argAfter("--save-list-real"); !out.isEmpty())
            QTimer::singleShot(2500, &manager, [&manager, out] {
                manager.saveList(out);
                manager.setPersistence(false);      // wyjscie bez pytania o zapis
                QTimer::singleShot(200, &manager, [] { QApplication::quit(); });
            });
        if (const QString out = argAfter("--save-list"); !out.isEmpty())
            QTimer::singleShot(2500, &manager, [&manager, out] {
                manager.exportList(out);
                manager.setPersistence(false);      // wyjscie bez pytania o zapis
                QTimer::singleShot(200, &manager, [] { QApplication::quit(); });
            });
        const int bigAt = int(args.indexOf(QStringLiteral("--shot-big")));
        if (bigAt >= 0 && bigAt + 2 < args.size()) {
            const int n = args.at(bigAt + 1).toInt() - 1;
            const QString path = args.at(bigAt + 2);
            QTimer::singleShot(2500, &manager, [&w, &manager, n, path] {
                for (ChannelTile* t : w->findChildren<ChannelTile*>()) {
                    if (n < 0 || n >= manager.count() || t->channel() != manager.channels().at(n) || !t->isVisible()) continue;
                    const QPoint local(t->width() / 2, t->height() / 3);
                    QMouseEvent e(QEvent::MouseButtonDblClick, QPointF(local), QPointF(t->mapToGlobal(local)),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(t, &e);
                }
                const bool rec = QApplication::arguments().contains(QStringLiteral("--big-rec"));
                auto press = [&w](int i) {
                    PreviewDialog* d = w->findChild<PreviewDialog*>();
                    const QList<KeyButton*> b = d ? d->findChildren<KeyButton*>() : QList<KeyButton*>();
                    if (i < b.size()) b.at(i)->click();
                };
                auto report = [&manager, n](const char* when) {
                    if (n < 0 || n >= manager.count()) return;
                    const QByteArray line = QStringLiteral("%1: state=%2\n").arg(QLatin1String(when)).arg(int(manager.channels().at(n)->state())).toUtf8();
                    fwrite(line.constData(), 1, size_t(line.size()), stdout);
                };
                if (rec) QTimer::singleShot(500, &manager, [press] { press(0); });
                QTimer::singleShot(rec ? 4500 : 2500, &manager, [&w, path, rec, press, report] {
                    PreviewDialog* dlg = w->findChild<PreviewDialog*>();
                    if (rec) report("after REC");
                    const QByteArray line = dlg ? QStringLiteral("%1x%2\n").arg(dlg->width()).arg(dlg->height()).toUtf8()
                                                : QByteArray("no dialog\n");
                    fwrite(line.constData(), 1, size_t(line.size()), stdout);
                    fflush(stdout);
                    if (dlg) dlg->grab().save(path);
                    if (rec) press(2);
                    QTimer::singleShot(rec ? 2500 : 300, w.get(), [&w, rec, report] {
                        if (rec) report("after STOP");
                        fflush(stdout);
                        if (PreviewDialog* d = w->findChild<PreviewDialog*>()) d->close();
                        QTimer::singleShot(1200, w.get(), [] { QApplication::quit(); });
                    });
                });
            });
        }
        const int dragAt = int(args.indexOf(QStringLiteral("--drag-test")));
        if (dragAt >= 0 && dragAt + 2 < args.size()) {
            const int a = args.at(dragAt + 1).toInt() - 1, b = args.at(dragAt + 2).toInt() - 1;
            const QString shot = dragAt + 3 < args.size() && !args.at(dragAt + 3).startsWith(QLatin1String("--")) ? args.at(dragAt + 3) : QString();
            QTimer::singleShot(2500, &manager, [&w, &manager, a, b, shot] {
                auto tileOf = [&](int i) -> ChannelTile* {
                    if (i < 0 || i >= manager.count()) return nullptr;
                    for (ChannelTile* t : w->findChildren<ChannelTile*>())
                        if (t->channel() == manager.channels().at(i) && t->isVisible()) return t;
                    return nullptr;
                };
                ChannelTile* src = tileOf(a);
                ChannelTile* dst = tileOf(b);
                if (src && dst) {
                    auto send = [src](QEvent::Type type, const QPoint& global, Qt::MouseButton btn, Qt::MouseButtons btns) {
                        QMouseEvent e(type, QPointF(src->mapFromGlobal(global)), QPointF(global), btn, btns, Qt::NoModifier);
                        QApplication::sendEvent(src, &e);
                    };
                    const QPoint from = src->mapToGlobal(src->rect().center());
                    // cel: prawa cwiartka kafla B przy ruchu do przodu listy, lewa przy ruchu do tylu
                    const QPoint to = dst->mapToGlobal(QPoint(b > a ? dst->width() * 3 / 4 : dst->width() / 4, dst->height() / 2));
                    send(QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
                    send(QEvent::MouseMove, from + QPoint(30, 10), Qt::NoButton, Qt::LeftButton);
                    send(QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton);
                    if (!shot.isEmpty()) w->grab().save(shot);
                    send(QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
                }
                QStringList names;
                for (Channel* c : manager.channels()) names << c->config().name;
                const QByteArray line = (names.join(QStringLiteral(", ")) + QLatin1Char('\n')).toUtf8();
                fwrite(line.constData(), 1, size_t(line.size()), stdout);
                fflush(stdout);
                QTimer::singleShot(300, &manager, [] { QApplication::quit(); });
            });
        }
        if (const int comboAt = int(args.indexOf(QStringLiteral("--shot-combo"))); comboAt >= 0 && comboAt + 2 < args.size()) {
            const int n = args.at(comboAt + 1).toInt();
            const QString path = args.at(comboAt + 2);
            QTimer::singleShot(3000, &manager, [&w, n, path] {
                const QList<QComboBox*> combos = w->inspector()->findChildren<QComboBox*>();
                if (n < 0 || n >= combos.size()) { QApplication::quit(); return; }
                QComboBox* c = combos.at(n);
                c->showPopup();
                QTimer::singleShot(500, c, [c, path] {
                    QWidget* pop = c->view()->window();
                    pop->grab().save(path);
                    const QRect g = pop->frameGeometry(), s = pop->screen()->availableGeometry();
                    const QRect f = QRect(c->mapToGlobal(QPoint(0, 0)), c->size());
                    const QByteArray line = QStringLiteral("items=%1 popup=%2x%3 at %4,%5 field=%6 wide at %7 screen=%8x%9 inside=%10\n")
                        .arg(c->count()).arg(g.width()).arg(g.height()).arg(g.x()).arg(g.y()).arg(f.width()).arg(f.x())
                        .arg(s.width()).arg(s.height()).arg(s.contains(g) ? 1 : 0).toUtf8();
                    fwrite(line.constData(), 1, size_t(line.size()), stdout);
                    fflush(stdout);
                    c->hidePopup();
                    QTimer::singleShot(200, [] { QApplication::quit(); });
                });
            });
        }
        // --shot-rename plik [check|no]: po 3 s wybiera w inspektorze typ wejscia "Screen grab" tak jak klikniecie w liste,
        // zapisuje zrzut pytania o zmiane nazwy (albo "no dialog"), odpowiada Yes ("no": No; "check": z "Don't ask again")
        // i wypisuje nazwe kanalu
        if (const int rnAt = int(args.indexOf(QStringLiteral("--shot-rename"))); rnAt >= 0 && rnAt + 1 < args.size()) {
            const QString path = args.at(rnAt + 1);
            const QString how = rnAt + 2 < args.size() ? args.at(rnAt + 2) : QString();
            QTimer::singleShot(3000, &manager, [&w, &manager, path, how] {
                for (QComboBox* c : w->findChildren<QComboBox*>()) {
                    const int i = c->findData(QStringLiteral("screen"));
                    if (i < 0 || c->findData(QStringLiteral("srt")) < 0) continue;
                    c->setCurrentIndex(i);
                    emit c->activated(i);
                }
                QTimer::singleShot(700, &manager, [&w, &manager, path, how] {
                    QMessageBox* box = w->findChild<QMessageBox*>();
                    QByteArray line = box ? "dialog\n" : "no dialog\n";
                    if (box) {
                        box->grab().save(path);
                        if (how == QLatin1String("check") && box->checkBox()) box->checkBox()->setChecked(true);
                        box->button(how == QLatin1String("no") ? QMessageBox::No : QMessageBox::Yes)->click();
                    }
                    QTimer::singleShot(1500, &manager, [&manager, line] {
                        const QByteArray out = line + "name=" + manager.channels().first()->config().name.toUtf8() + "\n" +
                                               "state=" + QByteArray::number(int(manager.channels().first()->state())) + "\n";
                        fwrite(out.constData(), 1, size_t(out.size()), stdout);
                        fflush(stdout);
                        QApplication::quit();
                    });
                });
            });
        }
        if (const QString shot = argAfter("--shot-settings"); !shot.isEmpty()) {
            app.setProperty("shotSettings", shot);
            QTimer::singleShot(3000, &manager, [&w] {
                w->openSettings();
                QTimer::singleShot(200, [] { QApplication::quit(); });
            });
        }
        if (const int pat = argAfter("--pattern").toInt(); pat > 0)
            for (Channel* c : manager.channels()) c->setPattern(pat);
        if (const int mon = argAfter("--monitor").toInt(); mon > 0)
            QTimer::singleShot(4000, &manager, [&manager, mon] {
                // --monitor-device id: karta podana wprost (test na wirtualnej karcie, zeby nic nie zagralo z glosnikow)
                const QStringList av = QApplication::arguments();
                const int devAt = int(av.indexOf(QStringLiteral("--monitor-device")));
                if (mon <= manager.count()) {
                    if (devAt >= 0 && devAt + 1 < av.size()) manager.channels().at(mon - 1)->setMonitor(true, av.at(devAt + 1));
                    else manager.setMonitored(manager.channels().at(mon - 1));
                }
                Channel* m = mon <= manager.count() && manager.channels().at(mon - 1)->monitorOn() ? manager.channels().at(mon - 1) : nullptr;
                const QByteArray line = QStringLiteral("monitor=%1\n").arg(m ? m->config().name : QStringLiteral("none")).toUtf8();
                fwrite(line.constData(), 1, size_t(line.size()), stdout);
                fflush(stdout);
            });
        if (const int every = argAfter("--split-folder-every").toInt(); every > 0) {
            auto* t = new QTimer(&manager);
            QObject::connect(t, &QTimer::timeout, &manager, [&manager] { manager.splitToNewFolder(manager.channels()); });
            QTimer::singleShot(3000, t, [t, every] { t->start(every * 1000); });
        }
        if (const QString shot = argAfter("--shot-log"); !shot.isEmpty()) {
            app.setProperty("shotLog", shot);
            QTimer::singleShot(6000, &manager, [&w] {
                w->openEventLog();
                QTimer::singleShot(200, [] { QApplication::quit(); });
            });
        }
        // Zdalne sterowanie: zwykle uruchomienie bierze ustawienia z okna Settings; tryby testowe tylko z osobnym profilem
        // (ISOREC_PROFILE) albo z argumentow:  --osc-port N [--osc-fb host:port] [--no-reply]  i
        // --midi-in "nazwa" --midi-out "nazwa" [--midi-channel N];  --remote-selftest = samokontrola kodeka OSC, mapy MIDI i polecen
        {
            AppSettings::RemoteConfig rcfg;
            const bool fromSettings = !testMode || !qEnvironmentVariableIsEmpty("ISOREC_PROFILE");
            if (fromSettings) rcfg = AppSettings::remoteConfig();
            if (const int port = argAfter("--osc-port").toInt(); port > 0) {
                rcfg.osc = true;
                rcfg.oscPort = port;
            }
            if (const QString fb = argAfter("--osc-fb"); !fb.isEmpty() && fb.contains(QLatin1Char(':'))) {
                rcfg.fbHost = fb.section(QLatin1Char(':'), 0, 0);
                rcfg.fbPort = fb.section(QLatin1Char(':'), 1).toInt();
            }
            if (args.contains(QStringLiteral("--no-reply"))) rcfg.reply = false;
            if (!argAfter("--midi-in").isEmpty() || !argAfter("--midi-out").isEmpty()) {
                rcfg.midi = true;
                rcfg.midiIn = argAfter("--midi-in");
                rcfg.midiOut = argAfter("--midi-out");
                rcfg.midiChannel = argAfter("--midi-channel").toInt();
            }
            remote.apply(rcfg);
        }
        if (args.contains(QStringLiteral("--remote-selftest")))
            QTimer::singleShot(2500, &manager, [&remote] {
                QString report;
                const int bad = remote.selfTest(&report);
                const QByteArray utf8 = report.toUtf8() + "errors=" + QByteArray::number(bad) + "\n";
                fwrite(utf8.constData(), 1, size_t(utf8.size()), stdout);
                fflush(stdout);
                QApplication::exit(bad);
            });
        if (const QString view = argAfter("--view"); !view.isEmpty()) w->setView(view == QLatin1String("list") ? 0 : view.toInt());
        const int tabAt = int(args.indexOf(QStringLiteral("--tab")));       // 0 Input, 1 Output, 2 Scheduler
        if (tabAt >= 0 && tabAt + 1 < args.size()) w->inspector()->setTab(args.at(tabAt + 1).toInt());
        if (args.contains(QStringLiteral("--tab-end")))     // przewin zakladke inspektora na koniec (do zrzutow)
            QTimer::singleShot(2000, &manager, [&w] {
                if (auto* sa = w->inspector()->findChild<QScrollArea*>()) sa->verticalScrollBar()->setValue(sa->verticalScrollBar()->maximum());
            });
        if (const int px = argAfter("--tab-scroll").toInt(); px > 0)     // przewin zakladke inspektora o N pikseli (do zrzutow)
            QTimer::singleShot(2000, &manager, [&w, px] {
                if (auto* sa = w->inspector()->findChild<QScrollArea*>()) sa->verticalScrollBar()->setValue(px);
            });
        const int shotAt = int(args.indexOf(QStringLiteral("--screenshot")));
        if (shotAt >= 0 && shotAt + 1 < args.size()) {
            const QString path = args.at(shotAt + 1);
            const int delay = argAfter("--shot-delay").toInt() > 0 ? argAfter("--shot-delay").toInt() : 4000;   // ms
            QTimer::singleShot(delay, &manager, [&w, path] {
                w->grab().save(path);
                QApplication::quit();
            });
        }
        rc = app.exec();
        if (const QString dump = argAfter("--dump-log"); !dump.isEmpty()) {     // --dump-log plik: log zdarzen tego uruchomienia
            QFile f(dump);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) f.write(EventLog::recent().join(QLatin1Char('\n')).toUtf8());
        }
        if (!testMode) manager.discardAutosave();    // poprawne wyjscie - autozapis niepotrzebny
    }   // kanaly i pipeline'y sa zamykane tutaj, przed gst_deinit
    // Jesli jakies zrodlo nie odpowiedzialo i jego pipeline zostal porzucony, jego watki moga wisiec bez konca -
    // zwykle zamykanie GStreamera i procesu czekaloby na nie. Wszystkie pliki sa juz zamkniete, wiec konczymy proces od razu.
    // To samo po nagrywaniu ze zdalnego zrodla NDI: zwykle wyjscie (wyladowywanie bibliotek po ExitProcess) potrafi
    // zawisnac na zawsze - po programie zostaje proces z jednym watkiem, ktorego nie da sie zabic i ktory blokuje plik EXE.
    // Dlatego proces konczy sie zawsze od razu, bez gst_deinit i bez wyladowywania bibliotek.
    fflush(nullptr);
    TerminateProcess(GetCurrentProcess(), UINT(rc));
    return rc;
}
