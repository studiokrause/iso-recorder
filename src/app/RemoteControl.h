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

    // --- MIDI learn: kazde z 91 polecen ma przypisanie (nuta albo CC, kanal MIDI); domyslnie nuta o numerze polecenia
    struct MidiBinding { int type = 0; int number = 0; int channel = 0; };      // type: 0 brak, 1 nuta, 2 CC; channel 0 = dowolny
    static constexpr int kMidiCommands = 91;
    static QString midiCommandName(int index);                                  // nazwa dla czlowieka (tlumaczona)
    static QList<MidiBinding> defaultBindings();
    static QList<MidiBinding> parseBindings(const QString& text);               // puste / uszkodzone = domyslne
    static QString serializeBindings(const QList<MidiBinding>& list);           // pusty tekst, gdy wszystko domyslne
    static QString describeBinding(const MidiBinding& b);
    // Nasluch nastepnej wiadomosci z wejscia o podanej nazwie; wynik w sygnale midiLearned (nic nie wykonuje).
    void beginLearn(const QString& inputName);
    void cancelLearn();
    bool learning() const { return m_learning; }

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
    void midiLearned(int type, int number, int channel);       // type 1 = nuta, 2 = CC; channel 1-16
    void learnEnded();

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
    void sendBinding(int index, bool on);
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
    QList<MidiBinding> m_bind;
    bool m_learning = false;
    quintptr m_learnIn = 0;                         // wejscie otwarte tylko na czas nasluchu (gdy inne niz robocze)
    bool m_cleanup = false;
};
