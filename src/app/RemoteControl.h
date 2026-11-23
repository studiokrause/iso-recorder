#pragma once
#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>

#include "core/AppSettings.h"

class ChannelManager;
class MainWindow;
class QUdpSocket;

// Zdalne sterowanie: OSC (UDP) i MIDI (wejscie i wyjscie), z informacja zwrotna o stanie.
// Wszystkie polecenia - z OSC, z MIDI i z Bitfocus Companion (modul wysyla OSC) - trafiaja do jednego wykonawcy
// (runCommand), wiec dzialaja tak samo. Kanaly numerowane od 1, tak jak w interfejsie.
//
// Polecenia (sciezki OSC; nazwy kanalow nie maja znaczenia - tylko numer):
//   /isorec/ch/N/rec | stop | pause | split           kanal N (1-16)
//   /isorec/ch/N/select | select/on | select/off     przelacz / ustaw zaznaczenie kanalu
//   /isorec/selected/rec | stop | pause | split       zaznaczone kanaly
//   /isorec/select/all | none | invert
//   /isorec/view/2x2 | 3x3 | 4x4 | list               (albo /isorec/view <2|3|4|0|"list">)
//   /isorec/status                                    odeslij pelny stan (takze jako utrzymanie sesji klienta)
// Argument 0 (zwolnienie przycisku w TouchOSC itp.) jest ignorowany - polecenie dziala na nacisk.
//
// Informacja zwrotna (tylko zmiany; pelny stan po /isorec/status i po pierwszym poleceniu nowego klienta):
//   /isorec/session s, /isorec/version s, /isorec/view s ("2x2" "3x3" "4x4" "list"), /isorec/channels i,
//   /isorec/rec/count i, /isorec/selected/count i,
//   /isorec/ch/N/name s, state s, rec i, paused i, selected i, time s (HH:MM:SS), source s, mbps f
class RemoteControl : public QObject {
    Q_OBJECT
public:
    explicit RemoteControl(ChannelManager* mgr, QObject* parent = nullptr);
    ~RemoteControl() override;

    void setWindow(MainWindow* w);
    void apply(const AppSettings::RemoteConfig& c);
    const AppSettings::RemoteConfig& config() const { return m_cfg; }
    QString statusText() const;                 // "OSC :8000, MIDI in/out" itp. - do logu

    static QStringList midiInputs();
    static QStringList midiOutputs();

    // Wykonanie polecenia, jak z OSC; zwraca false dla nieznanej sciezki.
    bool runCommand(const QString& path, const QVariantList& args);
    // Wejscie MIDI (z watku MIDI przez kolejke albo z testu): status, dane 1, dane 2
    void handleMidi(int status, int d1, int d2);
    // Samokontrola kodowania OSC, mapy MIDI i polecen; zwraca liczbe bledow
    int selfTest(QString* report);
    // Test: ostatnie polecenia i zapytania, bez sieci
    QMap<QString, QVariant> snapshot() const;

signals:
    void notice(const QString& text);

private:
    void openOsc();
    void closeOsc();
    void openMidi();
    void closeMidi();
    void onDatagrams();
    void tick();
    void sendTo(const QHostAddress& host, quint16 port, const QString& path, const QVariantList& args);
    void sendAll(const QMap<QString, QVariant>& values);
    void sendMidiFeedback(const QMap<QString, QVariant>& values, bool all);
    void sendNote(int note, bool on);
    static QString midiCommand(int note);
    int viewNow() const;
    static QString viewName(int view);

    ChannelManager* m_mgr;
    QPointer<MainWindow> m_window;
    AppSettings::RemoteConfig m_cfg;
    QUdpSocket* m_socket = nullptr;
    QTimer m_timer;
    QMap<QString, QVariant> m_last;                 // ostatnio wyslany stan
    struct Client { QHostAddress host; quint16 port = 0; QDateTime seen; };
    QList<Client> m_clients;                        // klienci, ktorzy wyslali polecenie (odpowiedz na adres nadawcy)
    QHostAddress m_fbHost;
    int m_viewShown = -1;
    // MIDI
    quintptr m_midiIn = 0, m_midiOut = 0;
    QMap<int, int> m_midiLast;                      // nuta -> ostatnia wyslana wartosc
    bool m_forceMidi = true;
    bool m_cleanup = false;
};
