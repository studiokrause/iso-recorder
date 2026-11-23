#include "AppSettings.h"

#include <QSettings>

#include "ChannelManager.h"

namespace {
QSettings store() { return QSettings(ChannelManager::settingsOrg(), QStringLiteral("IsoRecorder")); }
}

OutputConfig AppSettings::defaultOutput() {
    QSettings s = store();
    OutputConfig o;
    const OutputConfig d;
    o.dir = s.value(QStringLiteral("defaults/dir")).toString();
    o.nameTemplate = s.value(QStringLiteral("defaults/nameTemplate")).toString();
    o.encoder = s.value(QStringLiteral("defaults/encoder"), d.encoder).toString();
    const QString codec = s.value(QStringLiteral("defaults/codec")).toString();
    o.codec = codec == QLatin1String("h265") || codec == QLatin1String("av1") || codec == QLatin1String("prores") ? codec : QStringLiteral("h264");
    o.videoKbps = qBound(500, s.value(QStringLiteral("defaults/videoKbps"), d.videoKbps).toInt(), 200000);
    o.audioKbps = qBound(64, s.value(QStringLiteral("defaults/audioKbps"), d.audioKbps).toInt(), 512);
    const QString cont = s.value(QStringLiteral("defaults/container")).toString();
    o.container = cont == QLatin1String("ts") || cont == QLatin1String("mp4") || cont == QLatin1String("mov") ? cont : QStringLiteral("mkv");
    o.growing = s.value(QStringLiteral("defaults/growing"), false).toBool();
    o.splitSeconds = qMax(0, s.value(QStringLiteral("defaults/splitSeconds"), 0).toInt());
    o.splitBytes = qMax<qint64>(0, s.value(QStringLiteral("defaults/splitBytes"), 0).toLongLong());
    return o;
}

AppSettings::DefaultFormat AppSettings::defaultFormat() {
    QSettings s = store();
    DefaultFormat f;
    f.width = qBound(160, s.value(QStringLiteral("defaults/width"), f.width).toInt(), 7680);
    f.height = qBound(120, s.value(QStringLiteral("defaults/height"), f.height).toInt(), 4320);
    f.fps = qBound(1, s.value(QStringLiteral("defaults/fps"), f.fps).toInt(), 120);
    return f;
}

void AppSettings::setDefaultFormat(const DefaultFormat& f) {
    QSettings s = store();
    s.setValue(QStringLiteral("defaults/width"), f.width);
    s.setValue(QStringLiteral("defaults/height"), f.height);
    s.setValue(QStringLiteral("defaults/fps"), f.fps);
}

void AppSettings::setDefaultOutput(const OutputConfig& o) {
    QSettings s = store();
    s.setValue(QStringLiteral("defaults/dir"), o.dir);
    s.setValue(QStringLiteral("defaults/nameTemplate"), o.nameTemplate);
    s.setValue(QStringLiteral("defaults/encoder"), o.encoder);
    s.setValue(QStringLiteral("defaults/codec"), o.codec);
    s.setValue(QStringLiteral("defaults/videoKbps"), o.videoKbps);
    s.setValue(QStringLiteral("defaults/audioKbps"), o.audioKbps);
    s.setValue(QStringLiteral("defaults/container"), o.container);
    s.setValue(QStringLiteral("defaults/growing"), o.growing);
    s.setValue(QStringLiteral("defaults/splitSeconds"), o.splitSeconds);
    s.setValue(QStringLiteral("defaults/splitBytes"), o.splitBytes);
}

QString AppSettings::monitorDevice() { return store().value(QStringLiteral("monitorDevice")).toString(); }
void AppSettings::setMonitorDevice(const QString& id) { store().setValue(QStringLiteral("monitorDevice"), id); }

QString AppSettings::splitFolderTemplate() {
    const QString t = store().value(QStringLiteral("splitFolderTemplate")).toString().trimmed();
    return t.isEmpty() ? QString::fromLatin1(kSplitFolderDefault) : t;
}
QString AppSettings::renameOnSource() {
    const QString m = store().value(QStringLiteral("renameOnSource")).toString();
    return m == QLatin1String("always") || m == QLatin1String("never") ? m : QStringLiteral("ask");
}
void AppSettings::setRenameOnSource(const QString& mode) { store().setValue(QStringLiteral("renameOnSource"), mode); }

AppSettings::RemoteConfig AppSettings::remoteConfig() {
    QSettings s = store();
    RemoteConfig c;
    c.osc = s.value(QStringLiteral("remote/osc"), c.osc).toBool();
    c.oscPort = qBound(1, s.value(QStringLiteral("remote/oscPort"), c.oscPort).toInt(), 65535);
    c.reply = s.value(QStringLiteral("remote/reply"), c.reply).toBool();
    c.fbHost = s.value(QStringLiteral("remote/fbHost")).toString();
    c.fbPort = qBound(1, s.value(QStringLiteral("remote/fbPort"), c.fbPort).toInt(), 65535);
    c.midi = s.value(QStringLiteral("remote/midi"), c.midi).toBool();
    c.midiIn = s.value(QStringLiteral("remote/midiIn")).toString();
    c.midiOut = s.value(QStringLiteral("remote/midiOut")).toString();
    c.midiChannel = qBound(0, s.value(QStringLiteral("remote/midiChannel"), c.midiChannel).toInt(), 16);
    c.midiMap = s.value(QStringLiteral("remote/midiMap")).toString();
    return c;
}

void AppSettings::setRemoteConfig(const RemoteConfig& c) {
    QSettings s = store();
    s.setValue(QStringLiteral("remote/osc"), c.osc);
    s.setValue(QStringLiteral("remote/oscPort"), c.oscPort);
    s.setValue(QStringLiteral("remote/reply"), c.reply);
    s.setValue(QStringLiteral("remote/fbHost"), c.fbHost.trimmed());
    s.setValue(QStringLiteral("remote/fbPort"), c.fbPort);
    s.setValue(QStringLiteral("remote/midi"), c.midi);
    s.setValue(QStringLiteral("remote/midiIn"), c.midiIn);
    s.setValue(QStringLiteral("remote/midiOut"), c.midiOut);
    s.setValue(QStringLiteral("remote/midiChannel"), c.midiChannel);
    s.setValue(QStringLiteral("remote/midiMap"), c.midiMap);
}

void AppSettings::setSplitFolderTemplate(const QString& tmpl) { store().setValue(QStringLiteral("splitFolderTemplate"), tmpl.trimmed()); }
