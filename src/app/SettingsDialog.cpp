#include "SettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableWidget>
#include <QTimer>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include "app/RemoteControl.h"
#include "core/AppSettings.h"
#include "core/Lang.h"
#include "media/Devices.h"
#include "media/EncoderFactory.h"
#include "util/FileNameTemplate.h"

QString nameTemplateHelp() {
    return T("Variables for the name:\n"
             "{session} — session name\n"
             "{channel} — channel name\n"
             "{index} — channel number; {index:02} pads with zeros\n"
             "{source} — source description\n"
             "{date} — recording start date (YYYY-MM-DD)\n"
             "{time} — recording start time (HH-MM-SS)\n"
             "{part} — file part number; {part:03} pads with zeros");
}

void fillContainerCombo(QComboBox* combo) {
    combo->clear();
    combo->addItem(QStringLiteral("MKV (Matroska)"), QStringLiteral("mkv"));
    combo->addItem(QStringLiteral("MPEG-TS"), QStringLiteral("ts"));
    combo->addItem(QStringLiteral("MP4"), QStringLiteral("mp4"));
    combo->addItem(QStringLiteral("MOV (QuickTime)"), QStringLiteral("mov"));
}

void fillCodecCombo(QComboBox* combo) {
    combo->clear();
    for (const QString& c : EncoderFactory::codecs()) combo->addItem(EncoderFactory::codecLabel(c), c);
}

void fillEncoderCombo(QComboBox* combo, const QString& codec) {
    combo->clear();
    combo->addItem(T("Auto (hardware first)"), QStringLiteral("auto"));
    for (const QString& e : EncoderFactory::installed(codec)) combo->addItem(EncoderFactory::displayName(e), e);
    // zapis bez ponownego kodowania: dotyczy kanalow SRT / RTSP, pozostale biora enkoder automatyczny
    combo->addItem(T("No re-encoding (stream copy)"), QStringLiteral("copy"));
    for (int i = 0; i < combo->count(); ++i) combo->setItemData(i, combo->itemText(i), Qt::ToolTipRole);
    combo->setProperty("forCodec", codec);
}

void enableContainersFor(QComboBox* combo, const QString& codec) {
    auto* model = qobject_cast<QStandardItemModel*>(combo->model());
    if (!model) return;
    for (int i = 0; i < model->rowCount(); ++i)
        model->item(i)->setEnabled(codec.isEmpty() || EncoderFactory::containerAccepts(combo->itemData(i).toString(), codec));
}

QString growingHelp() {
    return T("Growing file: the MP4 / MOV file can be opened in the editing program while it is still being recorded, and it survives a program or power failure. The index is written first and the file is extended in 1-second fragments. Works for MP4 and MOV only; MKV and MPEG-TS can always be read while recording.");
}

void updateGrowing(QCheckBox* box, const QString& container) {
    const bool ok = container == QLatin1String("mp4") || container == QLatin1String("mov");
    box->setEnabled(ok);
}

QString containerHelp() {
    return T("MKV and MPEG-TS files stay readable when the program or the power fails during recording. MP4 and MOV files do "
             "not — choose them only when the editing program needs them. MPEG-TS holds H.264 and H.265 only; ProRes needs MOV or MKV.");
}

namespace {
QLabel* heading(const QString& text) {
    auto* l = new QLabel(text);
    l->setStyleSheet(QStringLiteral("font-weight:700; color:#f0f0f0; padding-top:10px;"));
    return l;
}

QLabel* note(const QString& text) {
    auto* l = new QLabel(text);
    l->setWordWrap(true);
    l->setStyleSheet(QStringLiteral("color:#9d9d9d; font-size:11px;"));
    return l;
}
}  // namespace

SettingsDialog::SettingsDialog(QWidget* parent, RemoteControl* remote) : QDialog(parent), m_remote(remote) {
    setWindowTitle(T("Settings"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setMinimumWidth(620);
    setStyleSheet(QStringLiteral(
        "QLineEdit, QComboBox { background:#1a1a1a; border:1px solid #3a3a3a; border-radius:2px; padding:2px 8px; min-height:22px; }"
        "QComboBox QAbstractItemView { background:#1a1a1a; border:1px solid #3a3a3a; selection-background-color:#4a4a4a; }"));
    // Okno ma duzo pozycji: tresc przewija sie, przyciski OK / Cancel zostaja na dole.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 10);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget;
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);
    auto* v = new QVBoxLayout(content);
    v->setContentsMargins(18, 10, 18, 14);
    v->setSpacing(6);
    const QRect avail = screen() ? screen()->availableGeometry() : QRect(0, 0, 1280, 800);
    resize(660, qMin(960, avail.height() - 90));

    // ---- domyslne parametry nagrywania
    v->addWidget(heading(T("Default recording settings")));
    v->addWidget(note(T("Every newly added channel starts with these settings. Existing channels are not changed.")));
    auto* g = new QGridLayout;
    g->setColumnStretch(1, 1);
    g->setHorizontalSpacing(12);
    g->setVerticalSpacing(7);
    int r = 0;
    auto row = [&](const QString& label, QWidget* field, const QString& tip = {}) {
        auto* l = new QLabel(label);
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        l->setStyleSheet(QStringLiteral("color:#9d9d9d;"));
        if (!tip.isEmpty()) {
            l->setToolTip(tip);
            field->setToolTip(tip);
        }
        g->addWidget(l, r, 0);
        g->addWidget(field, r++, 1);
    };
    const OutputConfig def = AppSettings::defaultOutput();

    auto* dirRow = new QWidget;
    auto* dh = new QHBoxLayout(dirRow);
    dh->setContentsMargins(0, 0, 0, 0);
    dh->setSpacing(4);
    m_dir = new QLineEdit(QDir::toNativeSeparators(def.dir));
    m_dir->setPlaceholderText(QDir::toNativeSeparators(Channel::defaultOutputDir()));
    auto* browse = new QPushButton(QStringLiteral("..."));
    browse->setFixedWidth(40);
    dh->addWidget(m_dir, 1);
    dh->addWidget(browse);
    row(T("Folder"), dirRow);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString start = m_dir->text().trimmed().isEmpty() ? Channel::defaultOutputDir() : m_dir->text().trimmed();
        const QString dir = QFileDialog::getExistingDirectory(this, T("Choose recording folder"), start);
        if (!dir.isEmpty()) m_dir->setText(QDir::toNativeSeparators(dir));
    });

    m_template = new QLineEdit(def.nameTemplate);
    m_template->setPlaceholderText(QLatin1String(FileNameTemplate::kDefault));
    row(T("Name template"), m_template, nameTemplateHelp());

    m_container = new QComboBox;
    fillContainerCombo(m_container);
    m_container->setCurrentIndex(qMax(0, m_container->findData(def.container)));
    row(T("Container"), m_container, containerHelp());
    m_growing = new QCheckBox(T("Readable while recording (MP4 / MOV)"));
    m_growing->setChecked(def.growing);
    row(T("Growing file"), m_growing, growingHelp());
    updateGrowing(m_growing, m_container->currentData().toString());
    connect(m_container, &QComboBox::activated, this, [this] { updateGrowing(m_growing, m_container->currentData().toString()); });

    m_split = new QComboBox;
    m_split->addItem(T("Off"), qlonglong(0));
    for (int min : {1, 5, 10, 15, 30, 60}) m_split->addItem(T("every %1 min").arg(min), qlonglong(min * 60));
    for (int gbs : {1, 2, 4}) m_split->addItem(T("every %1 GB").arg(gbs), qlonglong(-(qlonglong(gbs) << 30)));
    m_split->setCurrentIndex(qMax(0, m_split->findData(def.splitSeconds > 0 ? qlonglong(def.splitSeconds) : -qlonglong(def.splitBytes))));
    row(T("Auto-split"), m_split);

    // domyslny rozmiar obrazu i klatkaz nowych kanalow (te same pozycje co w zakladce Input)
    const AppSettings::DefaultFormat defFmt = AppSettings::defaultFormat();
    m_format = new QComboBox;
    const struct { int w, h; const char* ratio; } formats[] = {
        {1280, 720, "16:9"}, {1920, 1080, "16:9"}, {1024, 768, "4:3"}, {720, 1280, "9:16"}, {1920, 800, "2.40:1"}, {1080, 1080, "1:1"}};
    for (const auto& f : formats)
        m_format->addItem(QStringLiteral("%1 × %2  ·  %3").arg(f.w).arg(f.h).arg(QLatin1String(f.ratio)), QSize(f.w, f.h));
    if (m_format->findData(QSize(defFmt.width, defFmt.height)) < 0)
        m_format->addItem(QStringLiteral("%1 × %2").arg(defFmt.width).arg(defFmt.height), QSize(defFmt.width, defFmt.height));
    m_format->setCurrentIndex(m_format->findData(QSize(defFmt.width, defFmt.height)));
    row(T("Video format"), m_format);
    m_fps = new QComboBox;
    for (int f : {24, 25, 30, 50, 60}) m_fps->addItem(T("%1 fps").arg(f), f);
    if (m_fps->findData(defFmt.fps) < 0) m_fps->addItem(T("%1 fps").arg(defFmt.fps), defFmt.fps);
    m_fps->setCurrentIndex(m_fps->findData(defFmt.fps));
    row(T("Frame rate"), m_fps);

    m_codec = new QComboBox;
    fillCodecCombo(m_codec);
    m_codec->setCurrentIndex(qMax(0, m_codec->findData(def.codec)));
    row(T("Codec"), m_codec);
    m_encoder = new QComboBox;
    fillEncoderCombo(m_encoder, m_codec->currentData().toString());
    m_encoder->setCurrentIndex(qMax(0, m_encoder->findData(def.encoder)));
    row(T("Encoder"), m_encoder);
    enableContainersFor(m_container, m_codec->currentData().toString());
    // zmiana kodeka: lista jego enkoderow i kontenery, ktore go przyjma
    connect(m_codec, &QComboBox::activated, this, [this] {
        const QString codec = m_codec->currentData().toString();
        const QString keep = m_encoder->currentData().toString();
        fillEncoderCombo(m_encoder, codec);
        m_encoder->setCurrentIndex(qMax(0, m_encoder->findData(keep)));
        enableContainersFor(m_container, codec);
        m_container->setCurrentIndex(qMax(0, m_container->findData(EncoderFactory::containerFor(m_container->currentData().toString(), codec))));
        updateGrowing(m_growing, m_container->currentData().toString());
        m_videoRate->setEnabled(!EncoderFactory::fixedBitrate(codec));
    });

    m_videoRate = new QComboBox;
    for (int mb : {2, 4, 6, 8, 12, 20, 35, 50}) m_videoRate->addItem(QStringLiteral("%1 Mb/s").arg(mb), mb * 1000);
    m_videoRate->setCurrentIndex(qMax(0, m_videoRate->findData(def.videoKbps)));
    row(T("Video bitrate"), m_videoRate);
    m_videoRate->setEnabled(!EncoderFactory::fixedBitrate(m_codec->currentData().toString()));

    m_audioRate = new QComboBox;
    for (int kb : {128, 192, 256, 320}) m_audioRate->addItem(QStringLiteral("AAC  %1 kb/s").arg(kb), kb);
    m_audioRate->setCurrentIndex(qMax(0, m_audioRate->findData(def.audioKbps)));
    row(T("Audio bitrate"), m_audioRate);
    v->addLayout(g);
    v->addWidget(note(T("No re-encoding (stream copy): the program writes the SRT / RTSP stream exactly as it arrives — the same "
                        "codec, picture size, bitrate and quality as at the sender, with no encoder load. What you get: a file "
                        "identical in quality to the stream. What you lose: the bitrate, picture size and codec cannot be changed; "
                        "recording, a split and a resume after a pause begin at the sender's next key frame (up to a few seconds "
                        "late); a signal loss closes the file and recording continues in a new file when the signal returns (there is "
                        "no black picture to fill the gap). It works only for SRT and RTSP sources; other channels use the automatic "
                        "encoder.")));

    // ---- odsluch
    v->addWidget(heading(T("Monitoring")));
    v->addWidget(note(T("The sound card on which a channel is played when you click its headphones icon. Only one channel plays at a time.")));
    g = new QGridLayout;
    g->setColumnStretch(1, 1);
    g->setHorizontalSpacing(12);
    r = 0;
    m_monitor = new QComboBox;
    m_monitor->addItem(T("Default system output"), QString());
    for (const Devices::Info& d : Devices::list(true).audioOutputs) m_monitor->addItem(d.name, d.id);
    m_monitor->setCurrentIndex(qMax(0, m_monitor->findData(AppSettings::monitorDevice())));
    row(T("Sound card"), m_monitor);
    v->addLayout(g);

    // ---- split do nowego folderu
    v->addWidget(heading(T("Split to new folder")));
    v->addWidget(note(T("The folder is created inside the channel's recording folder. The same variables as in the file name, without {part}.")));
    g = new QGridLayout;
    g->setColumnStretch(1, 1);
    g->setHorizontalSpacing(12);
    r = 0;
    m_folderTemplate = new QLineEdit(AppSettings::splitFolderTemplate());
    m_folderTemplate->setPlaceholderText(QLatin1String(AppSettings::kSplitFolderDefault));
    row(T("Folder name"), m_folderTemplate, nameTemplateHelp());
    v->addLayout(g);

    // ---- nazwa kanalu wg zrodla
    v->addWidget(heading(T("Channel names")));
    v->addWidget(note(T("A channel that still has its default name (TEST 1, TEST 2 …) can take the name of its source, for example "
                        "\"NDI - STUDIO (Cam 1)\". \"Ask\" also brings the question back after \"Don't ask again\".")));
    g = new QGridLayout;
    g->setColumnStretch(1, 1);
    g->setHorizontalSpacing(12);
    r = 0;
    m_rename = new QComboBox;
    m_rename->addItem(T("Ask"), QStringLiteral("ask"));
    m_rename->addItem(T("Always rename"), QStringLiteral("always"));
    m_rename->addItem(T("Never rename"), QStringLiteral("never"));
    m_rename->setCurrentIndex(qMax(0, m_rename->findData(AppSettings::renameOnSource())));
    row(T("After a source change"), m_rename);
    v->addLayout(g);

    // ---- zdalne sterowanie
    v->addWidget(heading(T("Remote control")));
    v->addWidget(note(T("Control recording from OSC software, a MIDI controller or Bitfocus Companion (Stream Deck). "
                        "The paths, the MIDI note map and the Companion module are described in the documentation. Applied when you press OK.")));
    g = new QGridLayout;
    g->setColumnStretch(1, 1);
    g->setHorizontalSpacing(12);
    r = 0;
    const AppSettings::RemoteConfig rc = AppSettings::remoteConfig();
    m_osc = new QCheckBox(T("Listen for OSC commands (UDP)"));
    m_osc->setChecked(rc.osc);
    row(T("OSC"), m_osc);
    m_oscPort = new QSpinBox;
    m_oscPort->setRange(1, 65535);
    m_oscPort->setValue(rc.oscPort);
    row(T("OSC port"), m_oscPort, T("The port on which the program listens for commands."));
    m_reply = new QCheckBox(T("Send status back to the sender of a command"));
    m_reply->setChecked(rc.reply);
    row(T("Feedback"), m_reply, T("Bitfocus Companion and most OSC controllers expect the status on the address the command came from."));
    m_fbHost = new QLineEdit(rc.fbHost);
    m_fbHost->setPlaceholderText(T("empty = none"));
    row(T("Also send status to"), m_fbHost, T("Optional fixed address (IP or name) that always receives the status, for example a TouchOSC tablet."));
    m_fbPort = new QSpinBox;
    m_fbPort->setRange(1, 65535);
    m_fbPort->setValue(rc.fbPort);
    row(T("Status port"), m_fbPort);
    m_midi = new QCheckBox(T("Use MIDI"));
    m_midi->setChecked(rc.midi);
    row(T("MIDI"), m_midi);
    m_midiIn = new QComboBox;
    m_midiIn->addItem(T("None"), QString());
    for (const QString& n : RemoteControl::midiInputs()) m_midiIn->addItem(n, n);
    m_midiIn->setCurrentIndex(qMax(0, m_midiIn->findData(rc.midiIn)));
    row(T("MIDI input"), m_midiIn);
    m_midiOut = new QComboBox;
    m_midiOut->addItem(T("None"), QString());
    for (const QString& n : RemoteControl::midiOutputs()) m_midiOut->addItem(n, n);
    m_midiOut->setCurrentIndex(qMax(0, m_midiOut->findData(rc.midiOut)));
    row(T("MIDI output"), m_midiOut, T("Receives notes that light the buttons of the controller (recording, selected, view)."));
    m_midiChannel = new QSpinBox;
    m_midiChannel->setRange(0, 16);
    m_midiChannel->setSpecialValueText(T("Any"));
    m_midiChannel->setValue(rc.midiChannel);
    row(T("MIDI channel"), m_midiChannel);
    v->addLayout(g);

    // ---- MIDI learn: lista polecen z przypisaniem; dwuklik albo "Learn" i nacisniecie przycisku na kontrolerze
    v->addWidget(note(T("MIDI mapping: select a command, press Learn and then press a button (or turn a knob) on the controller. "
                        "Notes and controller numbers (CC) both work. By default the commands use notes 0–90.")));
    m_bindings = RemoteControl::parseBindings(rc.midiMap);
    m_midiTable = new QTableWidget(RemoteControl::kMidiCommands, 2);
    m_midiTable->setHorizontalHeaderLabels({T("Command"), T("MIDI message")});
    m_midiTable->verticalHeader()->setVisible(false);
    m_midiTable->horizontalHeader()->setStretchLastSection(true);
    m_midiTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_midiTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_midiTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_midiTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_midiTable->setFixedHeight(230);
    m_midiTable->setStyleSheet(QStringLiteral("QTableWidget { background:#1a1a1a; border:1px solid #3a3a3a; gridline-color:#2a2a2a; }"
                                              "QHeaderView::section { background:#262626; color:#9d9d9d; border:0; padding:3px 6px; }"));
    fillMidiTable();
    v->addWidget(m_midiTable);
    auto* learnRow = new QHBoxLayout;
    m_learnButton = new QPushButton(T("Learn"));
    auto* clearBtn = new QPushButton(T("Clear"));
    auto* resetBtn = new QPushButton(T("Reset all to default"));
    learnRow->addWidget(m_learnButton);
    learnRow->addWidget(clearBtn);
    learnRow->addStretch(1);
    learnRow->addWidget(resetBtn);
    v->addLayout(learnRow);
    m_learnTimer = new QTimer(this);
    m_learnTimer->setSingleShot(true);
    connect(m_learnTimer, &QTimer::timeout, this, [this] { stopLearn(); });
    connect(m_learnButton, &QPushButton::clicked, this, [this] { if (m_learnTimer->isActive()) stopLearn(); else startLearn(); });
    connect(m_midiTable, &QTableWidget::cellDoubleClicked, this, [this] { startLearn(); });
    connect(clearBtn, &QPushButton::clicked, this, [this] {
        const int row = m_midiTable->currentRow();
        if (row < 0) return;
        m_bindings[row] = {};
        fillMidiTable();
        m_midiTable->selectRow(row);
    });
    connect(resetBtn, &QPushButton::clicked, this, [this] {
        m_bindings = RemoteControl::defaultBindings();
        fillMidiTable();
    });
    if (m_remote) {
        connect(m_remote, &RemoteControl::midiLearned, this, [this](int type, int number, int channel) {
            const int row = m_midiTable->currentRow();
            if (row < 0) return;
            for (RemoteControl::MidiBinding& b : m_bindings)        // ta sama wiadomosc nie moze sterowac dwoma poleceniami
                if (b.type == type && b.number == number && b.channel == channel) b = {};
            m_bindings[row] = {type, number, channel};
            fillMidiTable();
            m_midiTable->selectRow(row);
        });
        connect(m_remote, &RemoteControl::learnEnded, this, [this] {
            m_learnTimer->stop();
            m_learnButton->setText(T("Learn"));
        });
    } else {
        m_learnButton->setEnabled(false);
    }

    connect(this, &QDialog::finished, this, [this] { if (m_remote) m_remote->cancelLearn(); });
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    box->setContentsMargins(18, 0, 18, 0);
    connect(box, &QDialogButtonBox::accepted, this, [this] { save(); accept(); });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(box);
}

void SettingsDialog::save() {
    OutputConfig o;
    o.dir = QDir::fromNativeSeparators(m_dir->text().trimmed());
    o.nameTemplate = m_template->text().trimmed();
    o.container = m_container->currentData().toString();
    const qlonglong split = m_split->currentData().toLongLong();
    o.splitSeconds = split > 0 ? int(split) : 0;
    o.splitBytes = split < 0 ? -split : 0;
    o.encoder = m_encoder->currentData().toString();
    o.codec = m_codec->currentData().toString();
    if (o.codec.isEmpty()) o.codec = QStringLiteral("h264");
    o.container = EncoderFactory::containerFor(o.container, o.codec);
    o.growing = m_growing->isChecked();
    o.videoKbps = m_videoRate->currentData().toInt();
    o.audioKbps = m_audioRate->currentData().toInt();
    AppSettings::setDefaultOutput(o);
    AppSettings::DefaultFormat fmt;
    fmt.width = m_format->currentData().toSize().width();
    fmt.height = m_format->currentData().toSize().height();
    fmt.fps = m_fps->currentData().toInt();
    AppSettings::setDefaultFormat(fmt);
    AppSettings::setMonitorDevice(m_monitor->currentData().toString());
    AppSettings::setSplitFolderTemplate(m_folderTemplate->text());
    AppSettings::setRenameOnSource(m_rename->currentData().toString());
    AppSettings::RemoteConfig rc;
    rc.osc = m_osc->isChecked();
    rc.oscPort = m_oscPort->value();
    rc.reply = m_reply->isChecked();
    rc.fbHost = m_fbHost->text().trimmed();
    rc.fbPort = m_fbPort->value();
    rc.midi = m_midi->isChecked();
    rc.midiIn = m_midiIn->currentData().toString();
    rc.midiOut = m_midiOut->currentData().toString();
    rc.midiChannel = m_midiChannel->value();
    rc.midiMap = RemoteControl::serializeBindings(m_bindings);
    AppSettings::setRemoteConfig(rc);
}

void SettingsDialog::fillMidiTable() {
    for (int i = 0; i < RemoteControl::kMidiCommands; ++i) {
        m_midiTable->setItem(i, 0, new QTableWidgetItem(RemoteControl::midiCommandName(i)));
        auto* item = new QTableWidgetItem(RemoteControl::describeBinding(m_bindings.value(i)));
        if (m_bindings.value(i).type == 0) item->setForeground(QColor(0x77, 0x77, 0x77));
        m_midiTable->setItem(i, 1, item);
    }
}

// Nasluch trwa do pierwszej wiadomosci z wybranego wejscia MIDI, najwyzej 15 s
void SettingsDialog::startLearn() {
    if (!m_remote || m_midiTable->currentRow() < 0) return;
    if (m_midiIn->currentData().toString().isEmpty() && m_remote->config().midiIn.isEmpty()) {
        m_learnButton->setText(T("Choose the MIDI input first"));
        QTimer::singleShot(2500, this, [this] { if (!m_learnTimer->isActive()) m_learnButton->setText(T("Learn")); });
        return;
    }
    m_remote->beginLearn(m_midiIn->currentData().toString());
    m_learnButton->setText(T("Press a button on the controller… (click to cancel)"));
    m_learnTimer->start(15000);
}

void SettingsDialog::stopLearn() {
    if (m_remote) m_remote->cancelLearn();
    m_learnTimer->stop();
    m_learnButton->setText(T("Learn"));
}
