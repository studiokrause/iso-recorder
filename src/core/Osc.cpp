#include "Osc.h"

#include <QMetaType>
#include <QtEndian>

#include <cstring>

namespace {
void padTo4(QByteArray& b) {
    while (b.size() % 4) b.append('\0');
}

void appendString(QByteArray& b, const QByteArray& s) {
    b.append(s);
    b.append('\0');
    padTo4(b);
}

void appendInt(QByteArray& b, qint32 v) {
    char raw[4];
    qToBigEndian(v, raw);
    b.append(raw, 4);
}

void appendFloat(QByteArray& b, float f) {
    quint32 bits;
    std::memcpy(&bits, &f, 4);
    char raw[4];
    qToBigEndian(bits, raw);
    b.append(raw, 4);
}

// Czyta tekst zakonczony zerem i dopelniony do 4 bajtow; zwraca false przy przekroczeniu zakresu
bool readString(const QByteArray& d, int& pos, QByteArray& out) {
    const int end = d.indexOf('\0', pos);
    if (end < 0) return false;
    out = d.mid(pos, end - pos);
    pos = (end + 4) & ~3;       // koniec tekstu + zero, w gore do wielokrotnosci 4
    return pos <= d.size();
}

bool decodeMessage(const QByteArray& d, Osc::Message& m) {
    int pos = 0;
    QByteArray path, tags;
    if (!readString(d, pos, path) || path.isEmpty() || path.at(0) != '/') return false;
    m.path = QString::fromUtf8(path);
    if (pos >= d.size()) return true;           // starsze nadajniki: komunikat bez listy typow
    if (!readString(d, pos, tags) || tags.isEmpty() || tags.at(0) != ',') return false;
    for (int i = 1; i < tags.size(); ++i) {
        switch (tags.at(i)) {
        case 'i':
            if (pos + 4 > d.size()) return false;
            m.args << int(qFromBigEndian<qint32>(d.constData() + pos));
            pos += 4;
            break;
        case 'h':
            if (pos + 8 > d.size()) return false;
            m.args << qlonglong(qFromBigEndian<qint64>(d.constData() + pos));
            pos += 8;
            break;
        case 'f': {
            if (pos + 4 > d.size()) return false;
            const quint32 bits = qFromBigEndian<quint32>(d.constData() + pos);
            float f;
            std::memcpy(&f, &bits, 4);
            m.args << double(f);
            pos += 4;
            break;
        }
        case 'd': {
            if (pos + 8 > d.size()) return false;
            const quint64 bits = qFromBigEndian<quint64>(d.constData() + pos);
            double v;
            std::memcpy(&v, &bits, 8);
            m.args << v;
            pos += 8;
            break;
        }
        case 's': {
            QByteArray s;
            if (!readString(d, pos, s)) return false;
            m.args << QString::fromUtf8(s);
            break;
        }
        case 'T': m.args << 1; break;
        case 'F': m.args << 0; break;
        case 'N': case 'I': m.args << QVariant(); break;
        default: return false;                  // nieznany typ: dalszej czesci nie da sie bezpiecznie odczytac
        }
    }
    return true;
}

bool decodeInto(const QByteArray& d, QList<Osc::Message>& out, int depth) {
    if (d.startsWith("#bundle")) {
        if (depth > 4 || d.size() < 16) return false;
        int pos = 16;                           // "#bundle\0" + znacznik czasu
        while (pos + 4 <= d.size()) {
            const int len = qFromBigEndian<qint32>(d.constData() + pos);
            pos += 4;
            if (len <= 0 || pos + len > d.size()) return false;
            if (!decodeInto(d.mid(pos, len), out, depth + 1)) return false;
            pos += len;
        }
        return true;
    }
    Osc::Message m;
    if (!decodeMessage(d, m)) return false;
    out << m;
    return true;
}
}  // namespace

QByteArray Osc::encode(const QString& path, const QVariantList& args) {
    QByteArray b;
    appendString(b, path.toUtf8());
    QByteArray tags(",");
    QByteArray data;
    for (const QVariant& v : args) {
        const int t = v.typeId();
        if (t == QMetaType::Bool || t == QMetaType::Int || t == QMetaType::UInt || t == QMetaType::LongLong || t == QMetaType::ULongLong) {
            tags.append('i');
            appendInt(data, qint32(v.toLongLong()));
        } else if (t == QMetaType::Double || t == QMetaType::Float) {
            tags.append('f');
            appendFloat(data, v.toFloat());
        } else {
            tags.append('s');
            appendString(data, v.toString().toUtf8());
        }
    }
    appendString(b, tags);
    b.append(data);
    return b;
}

bool Osc::decode(const QByteArray& data, QList<Message>& out) { return decodeInto(data, out, 0); }
