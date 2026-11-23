#pragma once
#include <QByteArray>
#include <QList>
#include <QString>
#include <QVariantList>

// Minimalna obsluga OSC 1.0 (UDP): komunikaty i pakiety "#bundle". Typy: i, h, f, d, s, T, F, N, I; reszta jest pomijana.
namespace Osc {
struct Message {
    QString path;
    QVariantList args;
};
// int / bool -> 'i', liczba rzeczywista -> 'f', tekst -> 's'
QByteArray encode(const QString& path, const QVariantList& args = {});
// Dekoduje komunikat albo pakiet (rekurencyjnie); false = dane uszkodzone
bool decode(const QByteArray& data, QList<Message>& out);
}
