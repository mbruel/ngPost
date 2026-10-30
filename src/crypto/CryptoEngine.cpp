#include "CryptoEngine.h"

#include <argon2.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <sodium.h>

#include <cstring>

namespace
{
const int SaltSize = 16;
const int KeySize = 32;
const int NonceSize = 24;
const int TagSize = 16;

void setError(QString *error, const QString &message)
{
    if(error)
        *error = message;
}

bool validIndex(quint32 value, const char *name, QString *error)
{
    if(value != 0)
        return true;
    setError(error, QString::fromLatin1(name) + QStringLiteral(" must be in 1..4294967295"));
    return false;
}

QByteArray hmacSha256(const QByteArray &key, const QByteArray &message)
{
    unsigned int length = 0;
    unsigned char digest[EVP_MAX_MD_SIZE];
    HMAC(EVP_sha256(), key.constData(), key.size(),
         reinterpret_cast<const unsigned char *>(message.constData()), message.size(), digest, &length);
    QByteArray result(reinterpret_cast<const char *>(digest), static_cast<int>(length));
    sodium_memzero(digest, sizeof(digest));
    return result;
}
}

bool CryptoEngine::initialize(QString *error)
{
    if(sodium_init() >= 0)
        return true;
    setError(error, QStringLiteral("Unable to initialize cryptographic provider"));
    return false;
}

bool CryptoEngine::validateKey(const QByteArray &key, const char *name, QString *error)
{
    if(key.size() == KeySize)
        return true;
    setError(error, QString::fromLatin1(name) + QStringLiteral(" must be exactly 32 bytes"));
    return false;
}

QByteArray CryptoEngine::uint32Be(quint32 value)
{
    QByteArray bytes(4, Qt::Uninitialized);
    bytes[0] = static_cast<char>((value >> 24) & 0xff);
    bytes[1] = static_cast<char>((value >> 16) & 0xff);
    bytes[2] = static_cast<char>((value >> 8) & 0xff);
    bytes[3] = static_cast<char>(value & 0xff);
    return bytes;
}

QByteArray CryptoEngine::uint64Be(quint64 value)
{
    QByteArray bytes(8, Qt::Uninitialized);
    for(int index = 7; index >= 0; --index)
    {
        bytes[index] = static_cast<char>(value & 0xff);
        value >>= 8;
    }
    return bytes;
}

// Associated data per yEnc Body Encryption Standard v1.2 Section 2.1 (as amended 2026-10-11):
// "yenc-body aad" || uint32_be(partNumber) || uint32_be(partBegin)
// || uint64_be(segmentBytes) || uint32_be(nameLength) || nameBytes.
QByteArray CryptoEngine::buildBodyAad(quint32 partNumber, quint32 partBegin, quint64 segmentBytes,
                                      const QByteArray &fileName)
{
    QByteArray aad = QByteArrayLiteral("yenc-body aad");
    aad += uint32Be(partNumber);
    aad += uint32Be(partBegin);
    aad += uint64Be(segmentBytes);
    aad += uint32Be(static_cast<quint32>(fileName.size()));
    aad += fileName;
    return aad;
}

QByteArray CryptoEngine::generateSalt()
{
    if(sodium_init() < 0)
        return QByteArray();
    QByteArray salt(SaltSize, Qt::Uninitialized);
    randombytes_buf(salt.data(), static_cast<size_t>(salt.size()));
    return salt;
}

QByteArray CryptoEngine::generateControlSalt()
{
    QByteArray salt;
    do
    {
        salt = generateSalt();
    }
    while(!salt.isEmpty() && (salt.contains('\0') || salt.contains('\n') || salt.contains('\r')));
    return salt;
}

bool CryptoEngine::deriveKey(const QString &password, const QByteArray &salt, QByteArray &key, QString *error)
{
    key.clear();
    if(!initialize(error))
        return false;
    if(password.isEmpty())
    {
        setError(error, QStringLiteral("Password must not be empty"));
        return false;
    }
    if(salt.size() != SaltSize)
    {
        setError(error, QStringLiteral("Salt must be exactly 16 bytes"));
        return false;
    }

    QByteArray passwordBytes = password.toUtf8();
    unsigned char rawKey[KeySize];
    const int status = argon2id_hash_raw(1, 65536, 4,
                                         passwordBytes.constData(), static_cast<size_t>(passwordBytes.size()),
                                         salt.constData(), static_cast<size_t>(salt.size()), rawKey, sizeof(rawKey));
    sodium_memzero(passwordBytes.data(), static_cast<size_t>(passwordBytes.size()));
    if(status != ARGON2_OK)
    {
        sodium_memzero(rawKey, sizeof(rawKey));
        setError(error, QStringLiteral("Argon2id key derivation failed"));
        return false;
    }
    key = QByteArray(reinterpret_cast<const char *>(rawKey), KeySize);
    sodium_memzero(rawKey, sizeof(rawKey));
    return true;
}

bool CryptoEngine::deriveKeys(const QString &password, const QByteArray &salt, QByteArray &keys, QString *error)
{
    return deriveKey(password, salt, keys, error);
}

bool CryptoEngine::deriveBodyNonce(const QByteArray &key, quint32 segmentIndex, QByteArray &nonce, QString *error)
{
    nonce.clear();
    if(!initialize(error) || !validateKey(key, "Key", error) || !validIndex(segmentIndex, "segmentIndex", error))
        return false;
    const QByteArray message = QByteArrayLiteral("yenc-body nonce") + uint32Be(segmentIndex);
    nonce = hmacSha256(key, message).left(NonceSize);
    return nonce.size() == NonceSize;
}

bool CryptoEngine::deriveControlKey(const QByteArray &masterKey, QByteArray &key, QString *error)
{
    key.clear();
    if(!initialize(error) || !validateKey(masterKey, "Master key", error))
        return false;
    key = hmacSha256(masterKey, QByteArrayLiteral("yenc-control key"));
    return key.size() == KeySize;
}

bool CryptoEngine::deriveControlTweak(const QByteArray &masterKey, quint32 segmentIndex, quint32 lineIndex,
                                      QByteArray &tweak, QString *error)
{
    tweak.clear();
    if(!initialize(error) || !validateKey(masterKey, "Master key", error)
            || !validIndex(segmentIndex, "segmentIndex", error) || !validIndex(lineIndex, "lineIndex", error))
        return false;
    const QByteArray message = QByteArrayLiteral("yenc-control tweak")
            + uint32Be(segmentIndex) + uint32Be(lineIndex);
    tweak = hmacSha256(masterKey, message).left(8);
    return tweak.size() == 8;
}

bool CryptoEngine::encryptBody(const QByteArray &plaintext, const QByteArray &key, quint32 segmentIndex,
                               const QByteArray &aad, BodyEncryptionResult &result, QString *error)
{
    result = BodyEncryptionResult();
    if(aad.isEmpty())
    {
        setError(error, QStringLiteral("AAD must not be empty"));
        return false;
    }
    QByteArray nonce;
    if(!initialize(error) || !validateKey(key, "Key", error)
            || !deriveBodyNonce(key, segmentIndex, nonce, error))
        return false;

    QByteArray sealed(plaintext.size() + TagSize, Qt::Uninitialized);
    unsigned long long sealedLength = 0;
    const int status = crypto_aead_xchacha20poly1305_ietf_encrypt(
                reinterpret_cast<unsigned char *>(sealed.data()), &sealedLength,
                reinterpret_cast<const unsigned char *>(plaintext.constData()),
                static_cast<unsigned long long>(plaintext.size()),
                reinterpret_cast<const unsigned char *>(aad.constData()),
                static_cast<unsigned long long>(aad.size()),
                nullptr,
                reinterpret_cast<const unsigned char *>(nonce.constData()),
                reinterpret_cast<const unsigned char *>(key.constData()));
    if(status != 0 || sealedLength != static_cast<unsigned long long>(sealed.size()))
    {
        sodium_memzero(sealed.data(), static_cast<size_t>(sealed.size()));
        sodium_memzero(nonce.data(), static_cast<size_t>(nonce.size()));
        result = BodyEncryptionResult();
        setError(error, QStringLiteral("Body encryption failed"));
        return false;
    }
    result.ciphertext = sealed.left(plaintext.size());
    result.tag = sealed.right(TagSize);
    sodium_memzero(sealed.data(), static_cast<size_t>(sealed.size()));
    sodium_memzero(nonce.data(), static_cast<size_t>(nonce.size()));
    return true;
}
