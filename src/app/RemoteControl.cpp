#include "RemoteControl.h"

#include <QCoreApplication>
#include <QNetworkDatagram>
#include <QUdpSocket>

#include <algorithm>

#include <windows.h>
#include <mmsystem.h>

#include "MainWindow.h"
#include "core/ChannelManager.h"
#include "core/Lang.h"
#include "core/Osc.h"

namespace {
constexpr int kMaxCh = 16;
const QString kRoot = QStringLiteral("/isorec");

QString stateName(const Channel* c) {
    if (c->inputOff()) return QStringLiteral("OFF");
    if (c->signalLost() && c->state() != ChannelState::Error) return QStringLiteral("RECONNECTING");
    switch (c->state()) {
    case ChannelState::Recording: return QStringLiteral("REC");
    case ChannelState::Paused: return QStringLiteral("PAUSED");
    case ChannelState::Finalizing: return QStringLiteral("SAVING");
    case ChannelState::Preview: return c->waitingForSchedule() ? QStringLiteral("WAITING") : QStringLiteral("PREVIEW");
    case ChannelState::Error: return QStringLiteral("ERROR");
    case ChannelState::Reconnecting: return QStringLiteral("RECONNECTING");
    case ChannelState::Waiting: return QStringLiteral("WAITING");
    default: return QStringLiteral("NO SIGNAL");
    }
}

QString clock(qint64 ms) {
    const qint64 s = ms / 1000;
    return QStringLiteral("%1:%2:%3").arg(s / 3600, 2, 10, QLatin1Char('0')).arg(s / 60 % 60, 2, 10, QLatin1Char('0')).arg(s % 60, 2, 10, QLatin1Char('0'));
}

QString chKey(int n, const char* leaf) { return QStringLiteral("%1/ch/%2/%3").arg(kRoot).arg(n).arg(QLatin1String(leaf)); }

// ---- MIDI (winmm) ----
void CALLBACK midiInProc(HMIDIIN, UINT msg, DWORD_PTR instance, DWORD_PTR p1, DWORD_PTR) {
    if (msg != MIM_DATA) return;
    auto* self = reinterpret_cast<RemoteControl*>(instance);
    const int status = int(p1 & 0xFF), d1 = int((p1 >> 8) & 0xFF), d2 = int((p1 >> 16) & 0xFF);
    QMetaObject::invokeMethod(self, [self, status, d1, d2] { self->handleMidi(status, d1, d2); }, Qt::QueuedConnection);
}
}  // namespace

// Mapa MIDI (tylko Note On, kanal MIDI z ustawien):
//   0-15 REC kanalu 1-16, 16-31 STOP, 32-47 PAUSE, 48-63 SPLIT, 64-79 SELECT (przelacza)
//   80 REC zaznaczonych, 81 STOP, 82 PAUSE, 83 SPLIT
//   84 zaznacz wszystkie, 85 odwroc zaznaczenie, 86 zaznacz zadne
//   87 widok 2x2, 88 3x3, 89 4x4, 90 lista
QString RemoteControl::midiCommand(int n) {
    static const char* const act[] = {"rec", "stop", "pause", "split", "select"};
    if (n >= 0 && n < 80) return QStringLiteral("%1/ch/%2/%3").arg(kRoot).arg(n % 16 + 1).arg(QLatin1String(act[n / 16]));
    static const char* const sel[] = {"rec", "stop", "pause", "split"};
    if (n >= 80 && n < 84) return QStringLiteral("%1/selected/%2").arg(kRoot, QLatin1String(sel[n - 80]));
    if (n == 84) return kRoot + QStringLiteral("/select/all");
    if (n == 85) return kRoot + QStringLiteral("/select/invert");
    if (n == 86) return kRoot + QStringLiteral("/select/none");
    static const char* const views[] = {"2x2", "3x3", "4x4", "list"};
    if (n >= 87 && n < 91) return QStringLiteral("%1/view/%2").arg(kRoot, QLatin1String(views[n - 87]));
    return {};
}

RemoteControl::RemoteControl(ChannelManager* mgr, QObject* parent) : QObject(parent), m_mgr(mgr) {
    m_timer.setInterval(250);
    connect(&m_timer, &QTimer::timeout, this, &RemoteControl::tick);
}

RemoteControl::~RemoteControl() {
    m_cleanup = true;
    closeOsc();
    closeMidi();
}

void RemoteControl::setWindow(MainWindow* w) {
    m_window = w;
    if (w) connect(w, &MainWindow::viewChanged, this, [this] { m_last.remove(kRoot + QStringLiteral("/view")); });
}

QStringList RemoteControl::midiInputs() {
    QStringList out;
    for (UINT i = 0; i < midiInGetNumDevs(); ++i) {
        MIDIINCAPSW caps{};
        if (midiInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) out << QString::fromWCharArray(caps.szPname);
    }
    return out;
}

QStringList RemoteControl::midiOutputs() {
    QStringList out;
    for (UINT i = 0; i < midiOutGetNumDevs(); ++i) {
        MIDIOUTCAPSW caps{};
        if (midiOutGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) out << QString::fromWCharArray(caps.szPname);
    }
    return out;
}

QString RemoteControl::statusText() const {
    QStringList parts;
    if (m_socket) parts << QStringLiteral("OSC :%1").arg(m_cfg.oscPort);
    if (m_midiIn) parts << QStringLiteral("MIDI in");
    if (m_midiOut) parts << QStringLiteral("MIDI out");
    return parts.join(QStringLiteral(", "));
}

void RemoteControl::apply(const AppSettings::RemoteConfig& c) {
    closeOsc();
    closeMidi();
    m_cfg = c;
    m_last.clear();
    m_forceMidi = true;
    m_clients.clear();
    if (m_cfg.osc) openOsc();
    if (m_cfg.midi) openMidi();
    if (m_socket || m_midiIn || m_midiOut) {
        m_timer.start();
        emit notice(T("Remote control: %1").arg(statusText()));
    } else {
        m_timer.stop();
    }
}

void RemoteControl::openOsc() {
    m_socket = new QUdpSocket(this);
    if (!m_socket->bind(QHostAddress::AnyIPv4, quint16(m_cfg.oscPort), QUdpSocket::DefaultForPlatform)) {
        emit notice(T("OSC: cannot listen on port %1 (%2)").arg(m_cfg.oscPort).arg(m_socket->errorString()));
        delete m_socket;
        m_socket = nullptr;
        return;
    }
    m_fbHost = QHostAddress(m_cfg.fbHost.trimmed());
    connect(m_socket, &QUdpSocket::readyRead, this, &RemoteControl::onDatagrams);
}

void RemoteControl::closeOsc() {
    if (!m_socket) return;
    delete m_socket;
    m_socket = nullptr;
}

void RemoteControl::openMidi() {
    if (!m_cfg.midiIn.isEmpty()) {
        const int idx = int(midiInputs().indexOf(m_cfg.midiIn));
        HMIDIIN h = nullptr;
        if (idx >= 0 && midiInOpen(&h, UINT(idx), DWORD_PTR(&midiInProc), DWORD_PTR(this), CALLBACK_FUNCTION) == MMSYSERR_NOERROR) {
            midiInStart(h);
            m_midiIn = quintptr(h);
        } else {
            emit notice(T("MIDI: cannot open the input \"%1\"").arg(m_cfg.midiIn));
        }
    }
    if (!m_cfg.midiOut.isEmpty()) {
        const int idx = int(midiOutputs().indexOf(m_cfg.midiOut));
        HMIDIOUT h = nullptr;
        if (idx >= 0 && midiOutOpen(&h, UINT(idx), 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) m_midiOut = quintptr(h);
        else emit notice(T("MIDI: cannot open the output \"%1\"").arg(m_cfg.midiOut));
    }
    m_midiLast.clear();
}

void RemoteControl::closeMidi() {
    if (m_midiIn) {
        const HMIDIIN h = HMIDIIN(m_midiIn);
        midiInStop(h);
        midiInReset(h);
        midiInClose(h);
        m_midiIn = 0;
    }
    if (m_midiOut) {
        const HMIDIOUT h = HMIDIOUT(m_midiOut);
        midiOutReset(h);
        midiOutClose(h);
        m_midiOut = 0;
    }
}

int RemoteControl::viewNow() const { return m_window ? m_window->view() : 4; }

QString RemoteControl::viewName(int v) { return v == 0 ? QStringLiteral("list") : QStringLiteral("%1x%1").arg(v); }

// ---------------------------------------------------------------- polecenia

bool RemoteControl::runCommand(const QString& path, const QVariantList& args) {
    const QStringList p = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (p.size() < 2 || p.at(0).compare(QStringLiteral("isorec"), Qt::CaseInsensitive) != 0) return false;
    const QString group = p.at(1).toLower();
    const bool arg0 = !args.isEmpty() && args.first().isValid();
    const double value = arg0 ? args.first().toDouble() : 1.0;
    const bool press = value != 0.0;        // zwolnienie przycisku (0) nie wykonuje polecenia
    auto transport = [](const QString& name, Transport& t) {
        if (name == QLatin1String("rec")) t = Transport::Rec;
        else if (name == QLatin1String("stop")) t = Transport::Stop;
        else if (name == QLatin1String("pause")) t = Transport::Pause;
        else if (name == QLatin1String("split")) t = Transport::Split;
        else return false;
        return true;
    };
    Transport t = Transport::Rec;
    if (group == QLatin1String("ch") && (p.size() == 4 || p.size() == 5)) {
        bool ok = false;
        const int n = p.at(2).toInt(&ok);
        if (!ok || n < 1 || n > m_mgr->count()) return false;
        Channel* ch = m_mgr->channels().at(n - 1);
        const QString cmd = p.at(3).toLower();
        if (cmd == QLatin1String("select")) {
            if (p.size() == 5) {                                    // /select/on, /select/off: ustaw wprost
                const QString how = p.at(4).toLower();
                if (how != QLatin1String("on") && how != QLatin1String("off")) return false;
                if (press) ch->setSelected(how == QLatin1String("on"));
                return true;
            }
            if (press) ch->setSelected(!ch->isSelected());          // /select: przelacz
            return true;
        }
        if (p.size() == 5) return false;
        if (!transport(cmd, t)) return false;
        if (press) m_mgr->transport(ch, t);
        return true;
    }
    if (group == QLatin1String("selected") && p.size() == 3 && transport(p.at(2).toLower(), t)) {
        if (press) m_mgr->transportSelected(t);
        return true;
    }
    if (group == QLatin1String("select") && p.size() == 3) {
        const QString what = p.at(2).toLower();
        if (!press && (what == QLatin1String("all") || what == QLatin1String("none") || what == QLatin1String("invert"))) return true;
        if (what == QLatin1String("all")) m_mgr->selectAll();
        else if (what == QLatin1String("none")) m_mgr->selectNone();
        else if (what == QLatin1String("invert")) m_mgr->invertSelection();
        else return false;
        return true;
    }
    if (group == QLatin1String("view") && (p.size() == 3 || arg0)) {
        QString name = p.size() == 3 ? p.at(2).toLower() : args.first().toString().toLower();
        if (p.size() == 3 && arg0 && !press) return true;
        int v = -1;
        if (name == QLatin1String("2x2") || name == QLatin1String("2")) v = 2;
        else if (name == QLatin1String("3x3") || name == QLatin1String("3")) v = 3;
        else if (name == QLatin1String("4x4") || name == QLatin1String("4")) v = 4;
        else if (name == QLatin1String("list") || name == QLatin1String("0")) v = 0;
        if (v < 0) return false;
        if (m_window) m_window->setView(v);
        return true;
    }
    if (group == QLatin1String("status") && p.size() == 2) {
        m_last.clear();             // pelny stan przy najblizszym takcie, do wszystkich klientow
        m_forceMidi = true;
        return true;
    }
    return false;
}

void RemoteControl::handleMidi(int status, int d1, int d2) {
    const int type = status & 0xF0;
    // Tylko nuty (Note On): kontrolery CC to zwykle suwaki i galki, ktore przy ruchu wysylalyby setki "polecen".
    // Zwolnienie nuty (velocity 0) i Note Off sa pomijane.
    if (type != 0x90 || d2 == 0) return;
    if (m_cfg.midiChannel > 0 && (status & 0x0F) + 1 != m_cfg.midiChannel) return;
    const QString cmd = midiCommand(d1);
    if (!cmd.isEmpty()) runCommand(cmd, {});
}

// ---------------------------------------------------------------- stan i informacja zwrotna

QMap<QString, QVariant> RemoteControl::snapshot() const {
    QMap<QString, QVariant> v;
    v[kRoot + QStringLiteral("/version")] = QStringLiteral(APP_VERSION);
    v[kRoot + QStringLiteral("/session")] = m_mgr->session();
    v[kRoot + QStringLiteral("/view")] = viewName(viewNow());
    v[kRoot + QStringLiteral("/channels")] = m_mgr->count();
    int rec = 0, sel = 0;
    for (int i = 1; i <= kMaxCh; ++i) {
        const Channel* c = i <= m_mgr->count() ? m_mgr->channels().at(i - 1) : nullptr;
        const bool recording = c && c->isRecording() && c->state() != ChannelState::Paused;
        if (c && c->isRecording()) ++rec;
        if (c && c->isSelected()) ++sel;
        v[chKey(i, "name")] = c ? c->config().name : QString();
        v[chKey(i, "state")] = c ? stateName(c) : QStringLiteral("NONE");
        v[chKey(i, "rec")] = recording ? 1 : 0;
        v[chKey(i, "paused")] = c && c->state() == ChannelState::Paused ? 1 : 0;
        v[chKey(i, "selected")] = c && c->isSelected() ? 1 : 0;
        v[chKey(i, "time")] = c && c->isRecording() ? clock(c->recordMs()) : QStringLiteral("00:00:00");
        v[chKey(i, "source")] = c ? c->sourceText() : QString();
    }
    v[kRoot + QStringLiteral("/rec/count")] = rec;
    v[kRoot + QStringLiteral("/selected/count")] = sel;
    return v;
}

void RemoteControl::sendTo(const QHostAddress& host, quint16 port, const QString& path, const QVariantList& args) {
    if (m_socket) m_socket->writeDatagram(Osc::encode(path, args), host, port);
}

void RemoteControl::sendAll(const QMap<QString, QVariant>& values) {
    if (!m_socket) return;
    const QDateTime now = QDateTime::currentDateTime();
    m_clients.erase(std::remove_if(m_clients.begin(), m_clients.end(), [&](const Client& c) { return c.seen.secsTo(now) > 20; }), m_clients.end());
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (!m_fbHost.isNull() && m_cfg.fbPort > 0) sendTo(m_fbHost, quint16(m_cfg.fbPort), it.key(), {it.value()});
        if (m_cfg.reply)
            for (const Client& c : m_clients) sendTo(c.host, c.port, it.key(), {it.value()});
    }
}

void RemoteControl::sendNote(int note, bool on) {
    if (!m_midiOut) return;
    const int ch = std::max(0, m_cfg.midiChannel - 1);
    midiOutShortMsg(HMIDIOUT(m_midiOut), DWORD(0x90 | ch) | (DWORD(note) << 8) | (DWORD(on ? 127 : 0) << 16));
}

void RemoteControl::sendMidiFeedback(const QMap<QString, QVariant>& v, bool all) {
    if (!m_midiOut) return;
    QMap<int, int> want;
    const int count = v.value(kRoot + QStringLiteral("/channels")).toInt();
    bool anyRec = false, anyPaused = false;
    int selected = 0;
    for (int i = 1; i <= kMaxCh; ++i) {
        const bool exists = i <= count;
        const bool rec = v.value(chKey(i, "rec")).toInt() || v.value(chKey(i, "paused")).toInt();
        const bool paused = v.value(chKey(i, "paused")).toInt();
        const bool sel = v.value(chKey(i, "selected")).toInt();
        want[i - 1] = rec ? 1 : 0;                          // REC swieci, gdy kanal nagrywa (takze w pauzie)
        want[15 + i] = exists && !rec ? 1 : 0;              // STOP swieci, gdy kanal istnieje i nie nagrywa
        want[31 + i] = paused ? 1 : 0;
        want[47 + i] = 0;
        want[63 + i] = sel ? 1 : 0;
        if (sel) {
            ++selected;
            anyRec = anyRec || rec;
            anyPaused = anyPaused || paused;
        }
    }
    want[80] = anyRec ? 1 : 0;
    want[81] = selected > 0 && !anyRec ? 1 : 0;
    want[82] = anyPaused ? 1 : 0;
    want[83] = 0;
    want[84] = count > 0 && selected == count ? 1 : 0;
    want[85] = 0;
    want[86] = selected == 0 ? 1 : 0;
    const QString view = v.value(kRoot + QStringLiteral("/view")).toString();
    want[87] = view == QLatin1String("2x2");
    want[88] = view == QLatin1String("3x3");
    want[89] = view == QLatin1String("4x4");
    want[90] = view == QLatin1String("list");
    for (auto it = want.cbegin(); it != want.cend(); ++it)
        if (all || m_midiLast.value(it.key(), -1) != it.value()) {
            sendNote(it.key(), it.value() != 0);
            m_midiLast[it.key()] = it.value();
        }
}

void RemoteControl::tick() {
    if (m_cleanup) return;
    const QMap<QString, QVariant> now = snapshot();
    QMap<QString, QVariant> changed;
    for (auto it = now.cbegin(); it != now.cend(); ++it)
        if (!m_last.contains(it.key()) || m_last.value(it.key()) != it.value()) changed[it.key()] = it.value();
    m_last = now;
    if (!changed.isEmpty()) sendAll(changed);
    sendMidiFeedback(now, m_forceMidi);
    m_forceMidi = false;
}

void RemoteControl::onDatagrams() {
    while (m_socket && m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram dg = m_socket->receiveDatagram();
        QList<Osc::Message> messages;
        if (!Osc::decode(dg.data(), messages)) continue;
        bool known = false;
        for (const Client& c : m_clients) known = known || (c.host == dg.senderAddress() && c.port == dg.senderPort());
        for (const Osc::Message& m : messages) runCommand(m.path, m.args);
        if (!m_cfg.reply) continue;
        const QHostAddress from = dg.senderAddress();
        const quint16 port = quint16(dg.senderPort());
        bool found = false;
        for (Client& c : m_clients)
            if (c.host == from && c.port == port) {
                c.seen = QDateTime::currentDateTime();
                found = true;
            }
        if (!found) {
            m_clients.append({from, port, QDateTime::currentDateTime()});
            const QMap<QString, QVariant> full = snapshot();       // nowy klient dostaje pelny stan od razu
            for (auto it = full.cbegin(); it != full.cend(); ++it) sendTo(from, port, it.key(), {it.value()});
        }
        Q_UNUSED(known);
    }
}

// ---------------------------------------------------------------- samokontrola

int RemoteControl::selfTest(QString* report) {
    int bad = 0;
    QString out;
    auto check = [&](const QString& what, bool ok, const QString& got = {}) {
        out += QStringLiteral("%1  %2  %3\n").arg(ok ? QStringLiteral("ok  ") : QStringLiteral("BLAD"), what, got);
        if (!ok) ++bad;
    };
    // OSC: kodowanie i dekodowanie
    {
        const QByteArray b = Osc::encode(QStringLiteral("/isorec/ch/3/state"), {QStringLiteral("REC"), 7, 1.5});
        QList<Osc::Message> m;
        check(QStringLiteral("OSC: komunikat 's i f' przechodzi tam i z powrotem"),
              Osc::decode(b, m) && m.size() == 1 && m[0].path == QLatin1String("/isorec/ch/3/state") && m[0].args.size() == 3 &&
                  m[0].args[0].toString() == QLatin1String("REC") && m[0].args[1].toInt() == 7 && qAbs(m[0].args[2].toDouble() - 1.5) < 1e-6,
              QStringLiteral("(%1 B)").arg(b.size()));
        check(QStringLiteral("OSC: dlugosc komunikatu jest wielokrotnoscia 4"), b.size() % 4 == 0);
        QByteArray bundle("#bundle\0", 8);
        bundle.append(QByteArray(8, '\0'));
        for (const QByteArray& e : {Osc::encode(QStringLiteral("/isorec/select/all")), Osc::encode(QStringLiteral("/isorec/view/list"))}) {
            QByteArray len(4, '\0');
            len[3] = char(e.size());
            bundle.append(len);
            bundle.append(e);
        }
        QList<Osc::Message> bm;
        check(QStringLiteral("OSC: pakiet #bundle z dwoma komunikatami"), Osc::decode(bundle, bm) && bm.size() == 2 && bm[1].path == QLatin1String("/isorec/view/list"));
        QList<Osc::Message> junk;
        check(QStringLiteral("OSC: uszkodzone dane sa odrzucane"), !Osc::decode(QByteArray("abc"), junk) && !Osc::decode(QByteArray("/x\0\0,i\0\0", 8), junk));
    }
    // mapa MIDI
    check(QStringLiteral("MIDI: nuta 0 = REC kanalu 1, 16 = STOP kanalu 1, 79 = SELECT kanalu 16"),
          midiCommand(0) == QLatin1String("/isorec/ch/1/rec") && midiCommand(16) == QLatin1String("/isorec/ch/1/stop") &&
              midiCommand(79) == QLatin1String("/isorec/ch/16/select") && midiCommand(47) == QLatin1String("/isorec/ch/16/pause"));
    check(QStringLiteral("MIDI: nuty 80-90 = zaznaczone, zaznaczanie, widok"),
          midiCommand(80) == QLatin1String("/isorec/selected/rec") && midiCommand(83) == QLatin1String("/isorec/selected/split") &&
              midiCommand(84) == QLatin1String("/isorec/select/all") && midiCommand(85) == QLatin1String("/isorec/select/invert") &&
              midiCommand(87) == QLatin1String("/isorec/view/2x2") && midiCommand(90) == QLatin1String("/isorec/view/list") && midiCommand(91).isEmpty());
    // polecenia na zywych kanalach
    const int n = m_mgr->count();
    if (n >= 2) {
        runCommand(QStringLiteral("/isorec/select/none"), {});
        check(QStringLiteral("polecenie: select/none"), m_mgr->selectedCount() == 0);
        runCommand(QStringLiteral("/isorec/select/all"), {});
        check(QStringLiteral("polecenie: select/all"), m_mgr->selectedCount() == n);
        runCommand(QStringLiteral("/isorec/select/invert"), {});
        check(QStringLiteral("polecenie: select/invert po select/all"), m_mgr->selectedCount() == 0);
        runCommand(QStringLiteral("/isorec/ch/2/select"), {});
        check(QStringLiteral("polecenie: ch/2/select przelacza"), m_mgr->selectedCount() == 1 && m_mgr->channels().at(1)->isSelected());
        runCommand(QStringLiteral("/isorec/ch/2/select"), {0});
        runCommand(QStringLiteral("/isorec/ch/2/select"), {0});
        check(QStringLiteral("polecenie: ch/2/select z argumentem 0 (zwolnienie przycisku) jest ignorowane"), m_mgr->selectedCount() == 1);
        runCommand(QStringLiteral("/isorec/ch/2/select/off"), {});
        runCommand(QStringLiteral("/isorec/ch/2/select/on"), {});
        runCommand(QStringLiteral("/isorec/ch/2/select/on"), {});
        check(QStringLiteral("polecenie: select/on ustawia wprost (dwa razy to samo)"), m_mgr->selectedCount() == 1 && m_mgr->channels().at(1)->isSelected());
        handleMidi(0x90, 64, 127);        // MIDI: nuta 64 = SELECT kanalu 1
        check(QStringLiteral("MIDI: nuta 64 zaznacza kanal 1"), m_mgr->channels().at(0)->isSelected() && m_mgr->selectedCount() == 2);
        handleMidi(0x90, 64, 0);          // zwolnienie nuty nic nie robi
        check(QStringLiteral("MIDI: zwolnienie nuty nie przelacza"), m_mgr->selectedCount() == 2);
        handleMidi(0xB0, 85, 127);        // kontroler CC jest ignorowany (suwaki i galki)
        check(QStringLiteral("MIDI: CC (suwak, galka) jest ignorowany"), m_mgr->selectedCount() == 2);
        handleMidi(0x90, 85, 100);        // nuta 85 = odwroc zaznaczenie
        check(QStringLiteral("MIDI: nuta 85 odwraca zaznaczenie"), m_mgr->selectedCount() == n - 2);
        check(QStringLiteral("polecenie: nieznana sciezka i kanal poza zakresem sa odrzucane"),
              !runCommand(QStringLiteral("/isorec/ch/99/rec"), {}) && !runCommand(QStringLiteral("/isorec/foo"), {}) && !runCommand(QStringLiteral("/other/ch/1/rec"), {}));
    }
    const QMap<QString, QVariant> s = snapshot();
    check(QStringLiteral("stan: wersja, liczba kanalow i zaznaczonych"), s.value(kRoot + QStringLiteral("/version")).toString() == QLatin1String(APP_VERSION) &&
              s.value(kRoot + QStringLiteral("/channels")).toInt() == n && s.value(kRoot + QStringLiteral("/selected/count")).toInt() == m_mgr->selectedCount());
    if (report) *report = out;
    return bad;
}
