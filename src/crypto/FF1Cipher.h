#ifndef FF1CIPHER_H
#define FF1CIPHER_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

class FF1Cipher
{
public:
    static bool encryptLine(const QByteArray &plaintext, const QByteArray &masterKey, quint32 segmentIndex,
                            quint32 lineIndex, const QByteArray &salt, QByteArray &wire, QString *error = nullptr);
    static bool encryptControlLines(const QByteArray &block, const QByteArray &masterKey, quint32 segmentIndex,
                                    const QByteArray &salt, QByteArray &wire, QString *error = nullptr);
    static bool isAlphabetByte(uchar value);
};

#endif
