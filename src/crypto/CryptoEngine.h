#ifndef CRYPTOENGINE_H
#define CRYPTOENGINE_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <sodium.h>

struct BodyEncryptionResult
{
    QByteArray ciphertext;
    QByteArray tag;
};

class CryptoEngine
{
public:
    static QByteArray generateSalt();
    static QByteArray generateControlSalt();
    static bool deriveKey(const QString &password, const QByteArray &salt, QByteArray &key, QString *error = nullptr);
    static bool deriveKeys(const QString &password, const QByteArray &salt, QByteArray &keys, QString *error = nullptr);
    static bool deriveBodyNonce(const QByteArray &key, quint32 segmentIndex, QByteArray &nonce, QString *error = nullptr);
    static bool deriveControlKey(const QByteArray &masterKey, QByteArray &key, QString *error = nullptr);
    static bool deriveControlTweak(const QByteArray &masterKey, quint32 segmentIndex, quint32 lineIndex,
                                   QByteArray &tweak, QString *error = nullptr);
    static QByteArray buildBodyAad(quint32 partNumber, quint32 partBegin, quint64 segmentBytes, const QByteArray &fileName);
    static bool encryptBody(const QByteArray &plaintext, const QByteArray &key, quint32 segmentIndex,
                            const QByteArray &aad, BodyEncryptionResult &result, QString *error = nullptr);

private:
    static QByteArray uint32Be(quint32 value);
    static QByteArray uint64Be(quint64 value);

    static bool initialize(QString *error);
    static bool validateKey(const QByteArray &key, const char *name, QString *error);
};

#endif
