#pragma once
#include <QStringList>
#include <QPointer>
#include <QWidget>

#include "Shortcuts.h"

class ChannelGrid;
class ChannelList;
class QButtonGroup;
class QStackedWidget;
class ChannelManager;
enum class Transport;
class Inspector;
class KeyButton;
class QLabel;
class QProgressBar;
class QPushButton;
class RemoteControl;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(ChannelManager* mgr, QWidget* parent = nullptr);

    Inspector* inspector() const { return m_inspector; }
    void openEventLog() { showEventLog(); }
    void openSettings() { showSettings(); }
    // Nagrania przerwane awaria (dziennik Recovery): pytanie o naprawe plikow; ask = false naprawia bez pytania.
    void offerRecovery(bool ask = true);
    void setView(int view);                 // 2, 3, 4 = siatka n x n; 0 = lista
    int view() const { return m_view; }
    void setRemote(RemoteControl* r) { m_remote = r; }      // dla okna Settings (MIDI learn)
    static QList<ShortcutMap::Action> shortcutActions();     // dzialania ze skrotami i ich domyslne klawisze
    int keysSelfTest(QString* report);                       // skroty na zywych kanalach; liczba bledow

signals:
    void viewChanged(int view);
    void settingsSaved();                   // okno Settings zamkniete przyciskiem OK
    void languageRequested(const QString& code);

protected:
    void resizeEvent(QResizeEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void closeEvent(QCloseEvent*) override;

private:
    QWidget* buildHeader();
    QWidget* buildControlPanel();
    QWidget* buildStatus();
    void refreshCounts();
    void refreshActive();
    void refreshRecording();
    void transportSelected(Transport t);
    void removeSelected();
    void tickSecond();
    void notice(const QString& text);
    void showDiagnostics();
    void showDisks();
    void showEventLog();
    void showSettings();
    void showRecordings(QWidget* anchor);
    void loadShortcuts();
    bool runShortcut(const QString& id);
    // lista kanalow
    void refreshSession();
    void renameSession();
    bool maybeSave();                       // false = uzytkownik anulowal
    bool saveList(bool askPath);
    void loadList(const QString& path = {});
    void newList();
    bool confirmReplace();
    QWidget* m_sessionCell = nullptr;
    QLabel* m_autosave = nullptr;
    bool eventFilter(QObject* watched, QEvent* e) override;
    QWidget* m_diskCell = nullptr;

    ChannelManager* m_mgr;
    ChannelGrid* m_grid;
    ChannelList* m_list = nullptr;
    int m_view = 4;
    QPointer<RemoteControl> m_remote;
    ShortcutMap m_keys;
    QStackedWidget* m_views = nullptr;
    QButtonGroup* m_viewGroup = nullptr;
    Inspector* m_inspector;
    QWidget* m_right;
    static constexpr int HeaderH = 72, StatusH = 22, RightMin = 384, RightMax = 560;

    QLabel* m_session;
    QLabel* m_recCap;
    QLabel* m_recTime;
    QLabel* m_recBadge;
    QLabel* m_rtc;
    QLabel* m_diskCap;
    QLabel* m_diskTime;
    QLabel* m_diskFree;
    QProgressBar* m_diskBar;
    QLabel* m_cpu;
    QLabel* m_write;
    QLabel* m_hwEnc;
    bool m_closing = false;

    QLabel* m_selCap;
    KeyButton* m_btnTransport[4]{};
    KeyButton* m_btnSelect[5]{};
    KeyButton* m_btnAdd;
    KeyButton* m_btnSplitFolder = nullptr;
    QLabel* m_count;
    QLabel* m_legend;

    QLabel* m_events;
    QLabel* m_hint;
    QStringList m_log;
    quint64 m_cpuIdle = 0, m_cpuTotal = 0;
};
