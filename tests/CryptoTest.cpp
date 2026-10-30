#include <QtTest>

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "SegmentIndexAllocator.h"
#include "crypto/CryptoEngine.h"
#include "crypto/FF1Cipher.h"
#include "utils/Yenc.h"

namespace
{
QByteArray readVectorFile(const char *name)
{
    QFile file(QStringLiteral("%1/test-vectors/%2").arg(QCoreApplication::applicationDirPath()).arg(name));
    // qmake may run tests from the build dir; also try the source-relative path
    if(!file.exists())
        file.setFileName(QStringLiteral("test-vectors/%1").arg(name));
    if(!file.open(QIODevice::ReadOnly))
        qFatal("cannot open vector file: %s", qPrintable(file.fileName()));
    return file.readAll();
}

QJsonObject loadVectorJson(const char *name)
{
    const QByteArray raw = readVectorFile(name);
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
    if(parseError.error != QJsonParseError::NoError)
        qFatal("cannot parse vector file %s: %s", name, qPrintable(parseError.errorString()));
    return doc.object();
}

// Binary fixture fields are JSON arrays of ints under a *_bytes name.
QByteArray jsonBytes(const QJsonObject &object, const char *fieldName)
{
    QByteArray bytes;
    for(const QJsonValue &value : object.value(QString::fromLatin1(fieldName)).toArray())
        bytes.append(static_cast<char>(value.toInt()));
    return bytes;
}

QByteArray fixtureBytes(const QJsonObject &object, const char *baseName)
{
    const QByteArray fieldName = QByteArray(baseName) + QByteArrayLiteral("_bytes");
    return jsonBytes(object, fieldName.constData());
}

bool testDecryptBody(const QByteArray &ciphertext, const QByteArray &tag, const QByteArray &key,
                     quint32 segmentIndex, const QByteArray &aad, QByteArray &plaintext, QString *error = nullptr)
{
    plaintext.clear();
    if(key.size() != 32 || tag.size() != 16 || aad.isEmpty())
    {
        if(error) *error = QStringLiteral("Invalid parameters for body decryption");
        return false;
    }
    QByteArray nonce;
    if(!CryptoEngine::deriveBodyNonce(key, segmentIndex, nonce, error))
        return false;
    QByteArray combined = ciphertext + tag;
    plaintext.resize(ciphertext.size());
    unsigned long long decryptedLen = 0;
    const int status = crypto_aead_xchacha20poly1305_ietf_decrypt(
        reinterpret_cast<unsigned char *>(plaintext.data()), &decryptedLen,
        nullptr,
        reinterpret_cast<const unsigned char *>(combined.constData()),
        static_cast<unsigned long long>(combined.size()),
        reinterpret_cast<const unsigned char *>(aad.constData()),
        static_cast<unsigned long long>(aad.size()),
        reinterpret_cast<const unsigned char *>(nonce.constData()),
        reinterpret_cast<const unsigned char *>(key.constData()));
    if(status != 0 || decryptedLen != static_cast<unsigned long long>(ciphertext.size()))
    {
        plaintext.clear();
        if(error) *error = QStringLiteral("Authentication failed");
        return false;
    }
    return true;
}
} // namespace

class CryptoTest : public QObject
{
    Q_OBJECT

private slots:
    void controlLineVectors_data();
    void controlLineVectors();
    void fullArticlePreservesDataAndFraming();
    void generatedSaltsAreUsable();
    // Missing =yencryption header must be rejected in both directions
    void rejectsInvalidSaltOnNonLine1();
    void rejectsMissingEncryptionHeader();
    // Vendored canonical conformance vectors
    void vendoredArgon2idVectors();
    void vendoredNonceTweakVectors();
    void vendoredBodyEncryptionVectors();
    void vendoredIndexAllocationVectors();
    // Full-article FF1 vectors (deterministic combined-mode restoration)
    void fullArticleVectors_data();
    void fullArticleVectors();
    // NGPOST-TESTS-03 (1): a control line whose FF1 ciphertext starts with '.'
    // must be dot-stuffed ('..') on the wire.
    void encryptControlLinesDotStuffsLeadingDot();
};

void CryptoTest::controlLineVectors_data()
{
    QTest::addColumn<QByteArray>("plaintext");
    QTest::addColumn<quint32>("lineIndex");
    QTest::addColumn<QByteArray>("expectedWire");

    // Single-line FF1 vectors read straight from the vendored fixture
    const QJsonObject data = loadVectorJson("control_line_encryption.json");
    QHash<QString, QJsonObject> byId;
    for(const QJsonValue &value : data.value(QStringLiteral("vectors")).toArray())
    {
        const QJsonObject vector = value.toObject();
        if(!vector.contains(QStringLiteral("expected_wire_bytes")))
            continue;
        byId.insert(vector.value(QStringLiteral("id")).toString(), vector);
    }

    struct LineCase { const char *fixtureId; const char *row; };
    const LineCase lineCases[] = {
        {"control-vec-01-line-1-ybegin-single",   "ybegin_single"},
        {"control-vec-02-line-1-ybegin-multipart","ybegin_multipart"},
        {"control-vec-03-line-2-ypart",           "ypart"},
        {"control-vec-04-line-3-yencryption",     "yencryption"},
        {"control-vec-05-line-4-yend-single",     "yend_single"},
        {"control-vec-06-yend-multipart", "yend_multipart"},
    };
    for(const LineCase &lineCase : lineCases)
    {
        const QJsonObject vector = byId.value(QLatin1String(lineCase.fixtureId));
        QVERIFY2(!vector.isEmpty(), lineCase.fixtureId);
        QTest::newRow(lineCase.row)
                << QByteArray(vector.value(QStringLiteral("plaintext_line")).toString().toLatin1())
                << static_cast<quint32>(vector.value(QStringLiteral("line_index")).toInt())
                << fixtureBytes(vector, "expected_wire");
    }
}

void CryptoTest::controlLineVectors()
{
    QFETCH(QByteArray, plaintext);
    QFETCH(quint32, lineIndex);
    QFETCH(QByteArray, expectedWire);

    const QByteArray salt = fixtureBytes(
                loadVectorJson("control_line_encryption.json")
                    .value(QStringLiteral("vectors")).toArray().first().toObject(), "salt");
    QByteArray masterKey;
    QString error;
    QVERIFY2(CryptoEngine::deriveKey(QStringLiteral("public-conformance-vector-key-2026"), salt, masterKey, &error), qPrintable(error));
    QByteArray wire;
    QVERIFY2(FF1Cipher::encryptLine(plaintext, masterKey, 1, lineIndex, salt, wire, &error), qPrintable(error));
    QCOMPARE(wire, expectedWire);
    if(lineIndex == 1)
        QCOMPARE(wire.left(20), salt + QByteArray::fromHex("00000001"));
}

void CryptoTest::fullArticlePreservesDataAndFraming()
{
    const QByteArray salt("K7mX9pL2qR8vN4wZ", 16);
    QByteArray masterKey;
    QString error;
    QVERIFY2(CryptoEngine::deriveKey(QStringLiteral("public-conformance-vector-key-2026"), salt, masterKey, &error), qPrintable(error));
    const QByteArray line1("=ybegin line=128 size=18 name=file.bin");
    const QByteArray line2("DataLine1TestDataMustRemainUntouched1234567890");
    const QByteArray line3("DataLine2TestDataMustRemainUntouched1234567890");
    const QByteArray line4("=yend size=18");
    const QByteArray lineEnc = QByteArrayLiteral("=yencryption cipher=XChaCha20-Poly1305 salt=")
            + salt.toHex() + QByteArrayLiteral(" index=00000001 tag=24d9229edfc04aeaeedcf105bc91e695");
    const QByteArray block = line1 + "\r\n" + lineEnc + "\r\n" + line2 + "\n" + line3 + "\r\n" + line4;

    QByteArray wire;
    QVERIFY2(FF1Cipher::encryptControlLines(block, masterKey, 1, salt, wire, &error), qPrintable(error));
    const QList<QByteArray> wireLines = wire.split('\n');
    QCOMPARE(wireLines.size(), 5);
    QCOMPARE(wireLines[2], line2);
    QCOMPARE(wireLines[3], line3 + "\r");
    QCOMPARE(wireLines[0].left(20), salt + QByteArray::fromHex("00000001"));
}

void CryptoTest::generatedSaltsAreUsable()
{
    const QByteArray first = CryptoEngine::generateControlSalt();
    const QByteArray second = CryptoEngine::generateControlSalt();
    QCOMPARE(first.size(), 16);
    QCOMPARE(second.size(), 16);
    QVERIFY(first != second);
    for(char value : first)
        QVERIFY(FF1Cipher::isAlphabetByte(static_cast<uchar>(value)));
    QCOMPARE(CryptoEngine::generateSalt().size(), 16);
}

void CryptoTest::rejectsInvalidSaltOnNonLine1()
{
    // NP12-05: salt validation must not be skipped for lineIndex != 1. The
    // salt is only *prepended* on Line 1, but a malformed salt on any line
    // signals a broken upload session and must be rejected, not ignored.
    const QByteArray goodSalt("K7mX9pL2qR8vN4wZ", 16);
    QByteArray masterKey;
    QString error;
    QVERIFY2(CryptoEngine::deriveKey(QStringLiteral("public-conformance-vector-key-2026"), goodSalt, masterKey, &error),
             qPrintable(error));
    const QByteArray plaintext = QByteArrayLiteral("=yend size=4");

    // Wrong size on a non-Line-1 line.
    const QByteArray shortSalt("K7mX9pL2qR8vN4w", 15);
    QByteArray wire;
    QString saltError;
    QVERIFY(!FF1Cipher::encryptLine(plaintext, masterKey, 1, 4, shortSalt, wire, &saltError));
    QVERIFY(wire.isEmpty());
    QVERIFY(!saltError.isEmpty());

    // Right size but a byte outside the Radix 253 alphabet (0x00) on line 2.
    QByteArray badAlphabetSalt = goodSalt;
    badAlphabetSalt[3] = '\0';
    QVERIFY(!FF1Cipher::encryptLine(plaintext, masterKey, 1, 2, badAlphabetSalt, wire, &saltError));
    QVERIFY(wire.isEmpty());
    QVERIFY(!saltError.isEmpty());

    // Control: the same plaintext/line with the valid salt still encrypts,
    // and carries no salt prefix on a non-Line-1 line.
    QVERIFY2(FF1Cipher::encryptLine(plaintext, masterKey, 1, 4, goodSalt, wire, &error),
             qPrintable(error));
    QCOMPARE(wire.size(), plaintext.size());
    QVERIFY(!wire.startsWith(goodSalt));
}

void CryptoTest::rejectsMissingEncryptionHeader()
{
    const QByteArray salt("K7mX9pL2qR8vN4wZ", 16);
    QByteArray masterKey;
    QString error;
    QVERIFY2(CryptoEngine::deriveKey(QStringLiteral("public-conformance-vector-key-2026"), salt, masterKey, &error),
             qPrintable(error));

    const QByteArray blockNoHeader = QByteArrayLiteral("=ybegin line=128 size=4 name=file.bin\r\n")
            + QByteArrayLiteral("data\r\n=yend size=4 pcrc32=12345678\r\n");
    QByteArray wire;
    QVERIFY(!FF1Cipher::encryptControlLines(blockNoHeader, masterKey, 1, salt, wire, &error));
    QVERIFY(wire.isEmpty());

    const QByteArray validBlock = QByteArrayLiteral("=ybegin line=128 size=4 name=file.bin\r\n")
            + QByteArrayLiteral("=yencryption cipher=XChaCha20-Poly1305 salt=") + salt.toHex()
            + QByteArrayLiteral(" index=00000001 tag=24d9229edfc04aeaeedcf105bc91e695\r\n")
            + QByteArrayLiteral("data\r\n=yend size=4 pcrc32=12345678\r\n");
    QVERIFY2(FF1Cipher::encryptControlLines(validBlock, masterKey, 1, salt, wire, &error),
             qPrintable(error));
    QCOMPARE(wire.left(20), salt + QByteArray::fromHex("00000001"));
    const QList<QByteArray> validLines = wire.split('\n');
    QCOMPARE(validLines.size(), 5);
    QCOMPARE(validLines[2], QByteArrayLiteral("data\r"));
}

void CryptoTest::fullArticleVectors_data()
{
    QTest::addColumn<QString>("fixtureId");
    QTest::newRow("full_article_5_lines") << QStringLiteral("control-vec-07-full-article-5-lines");
    QTest::newRow("combined_one_byte") << QStringLiteral("control-vec-11-combined-one-byte-data");
    QTest::newRow("full_article_54_lines") << QStringLiteral("control-vec-08-full-article-54-lines");
    QTest::newRow("one_byte_data") << QStringLiteral("control-vec-09-one-byte-data");
    QTest::newRow("exact_wrap_article_6_lines") << QStringLiteral("control-vec-10-exact-wrap-article-6-lines");
    // single-part article whose name= contains " part=" must keep single-part layout (Control Std Section 9)
    QTest::newRow("single_part_name_contains_part") << QStringLiteral("control-vec-12-single-part-name-contains-part");
}

void CryptoTest::fullArticleVectors()
{
    QFETCH(QString, fixtureId);
    const QJsonObject data = loadVectorJson("control_line_encryption.json");
    QJsonObject vector;
    for(const QJsonValue &value : data.value(QStringLiteral("vectors")).toArray())
    {
        if(value.toObject().value(QStringLiteral("id")).toString() == fixtureId)
        {
            vector = value.toObject();
            break;
        }
    }
    QVERIFY2(!vector.isEmpty(), qPrintable(fixtureId));

    const QByteArray salt = fixtureBytes(vector, "salt");
    const quint32 segmentIndex = static_cast<quint32>(vector.value(QStringLiteral("segment_index")).toInt());
    const QString password = vector.value(QStringLiteral("password")).toString();

    QByteArray masterKey;
    QString error;
    QVERIFY2(CryptoEngine::deriveKey(password, salt, masterKey, &error), qPrintable(error));

    QByteArray block;
    for(const QJsonValue &lineVal : vector.value(QStringLiteral("input_lines")).toArray())
    {
        block += lineVal.toString().toLatin1();
        block += "\r\n";
    }

    QByteArray wire;
    QVERIFY2(FF1Cipher::encryptControlLines(block, masterKey, segmentIndex, salt, wire, &error), qPrintable(error));

    const QJsonArray expectedWireLines = vector.value(QStringLiteral("expected_wire_lines_bytes")).toArray();
    // Every line is CRLF-terminated, so strip the final terminator before splitting to get the exact count.
    QByteArray wireBody = wire;
    QVERIFY(wireBody.endsWith("\r\n"));
    wireBody.chop(2);
    const QList<QByteArray> wireLines = wireBody.split('\n');
    QCOMPARE(wireLines.size(), static_cast<int>(expectedWireLines.size()));

    for(int i = 0; i < expectedWireLines.size(); ++i)
    {
        QByteArray actual = wireLines.at(i);
        if(actual.endsWith('\r'))
            actual.chop(1);
        QByteArray expected;
        for(const QJsonValue &byteValue : expectedWireLines.at(i).toArray())
            expected.append(static_cast<char>(byteValue.toInt()));
        QCOMPARE(actual, expected);
    }
}

void CryptoTest::vendoredArgon2idVectors()
{
    const QJsonArray vectors = loadVectorJson("argon2id.json").value(QStringLiteral("vectors")).toArray();
    QVERIFY(!vectors.isEmpty());
    for(const QJsonValue &value : vectors)
    {
        const QJsonObject vector = value.toObject();
        QByteArray key;
        QString error;
        QVERIFY2(CryptoEngine::deriveKey(vector.value(QStringLiteral("password")).toString(),
                                         fixtureBytes(vector, "salt"),
                                         key, &error), qPrintable(error));
        QCOMPARE(key, fixtureBytes(vector, "expected_key"));
    }
}

void CryptoTest::vendoredNonceTweakVectors()
{
    const QJsonObject data = loadVectorJson("nonce_tweak.json");
    const QByteArray bodyKey = fixtureBytes(
                data.value(QStringLiteral("body_nonce_vectors")).toArray().first().toObject(), "key");
    for(const QJsonValue &value : data.value(QStringLiteral("body_nonce_vectors")).toArray())
    {
        const QJsonObject vector = value.toObject();
        QByteArray nonce;
        QString error;
        QVERIFY2(CryptoEngine::deriveBodyNonce(bodyKey,
                    static_cast<quint32>(vector.value(QStringLiteral("segment_index")).toVariant().toUInt()),
                    nonce, &error), qPrintable(error));
        QCOMPARE(nonce, fixtureBytes(vector, "expected_nonce"));
    }
    for(const QJsonValue &value : data.value(QStringLiteral("control_tweak_vectors")).toArray())
    {
        const QJsonObject vector = value.toObject();
        const QByteArray masterKey = fixtureBytes(vector, "master_key");
        QByteArray controlKey;
        QByteArray tweak;
        QString error;
        QVERIFY2(CryptoEngine::deriveControlKey(masterKey, controlKey, &error), qPrintable(error));
        QCOMPARE(controlKey, fixtureBytes(vector, "enc_key"));
        QVERIFY2(CryptoEngine::deriveControlTweak(masterKey,
                    static_cast<quint32>(vector.value(QStringLiteral("segment_index")).toVariant().toUInt()),
                    static_cast<quint32>(vector.value(QStringLiteral("line_index")).toVariant().toUInt()),
                    tweak, &error), qPrintable(error));
        QCOMPARE(tweak, fixtureBytes(vector, "expected_tweak"));
    }
}

void CryptoTest::vendoredBodyEncryptionVectors()
{
    const QJsonArray vectors = loadVectorJson("body_encryption.json").value(QStringLiteral("vectors")).toArray();
    QVERIFY(!vectors.isEmpty());
    for(const QJsonValue &value : vectors)
    {
        const QJsonObject vector = value.toObject();
        const QByteArray key = fixtureBytes(vector, "derived_key");
        const quint32 segmentIndex = static_cast<quint32>(vector.value(QStringLiteral("segment_index")).toVariant().toUInt());
        const QByteArray plaintext = fixtureBytes(vector, "plaintext");
        const QJsonObject aadObj = vector.value(QStringLiteral("aad")).toObject();
        const QByteArray expectedAad = fixtureBytes(aadObj, "expected_aad");
        const QByteArray nameBytes = fixtureBytes(aadObj, "name_bytes");
        // The production AAD builder must reproduce the canonical serialization.
        QCOMPARE(CryptoEngine::buildBodyAad(
                     static_cast<quint32>(aadObj.value(QStringLiteral("part_number")).toInt()),
                     static_cast<quint32>(aadObj.value(QStringLiteral("part_begin")).toInt()),
                     aadObj.value(QStringLiteral("segment_bytes")).toVariant().toULongLong(),
                     nameBytes),
                 expectedAad);
        BodyEncryptionResult encrypted;
        QString error;
        QVERIFY2(CryptoEngine::encryptBody(plaintext, key, segmentIndex, expectedAad, encrypted, &error), qPrintable(error));
        QCOMPARE(encrypted.ciphertext, fixtureBytes(vector, "expected_ciphertext"));
        QCOMPARE(encrypted.tag, fixtureBytes(vector, "expected_tag"));
        // Produced-article check (NGPOST-TESTS-01): run Yenc::encodeArticle
        // for this vector's geometry. producedLine below is built in test code
        // from the fixture salt/index and the actually-computed encrypted.tag
        // (already QCOMPAREd against the fixture above), so comparing it to
        // expected_yencryption_line would be fixture-vs-fixture; it is kept
        // only as the line the plain block is rebuilt around. The
        // produced-article assertion is the QCOMPARE(reEncrypted, article)
        // further down: FF1 is deterministic, so re-encrypting that rebuilt
        // block must reproduce encodeArticle's wire byte-for-byte, pinning the
        // =yencryption line encodeArticle actually emitted.
        {
            const quint32 partNumber = static_cast<quint32>(aadObj.value(QStringLiteral("part_number")).toInt());
            const quint64 segmentBytes = aadObj.value(QStringLiteral("segment_bytes")).toVariant().toULongLong();
            const quint32 partBegin = static_cast<quint32>(aadObj.value(QStringLiteral("part_begin")).toInt());
            const bool singlePart = (segmentBytes == static_cast<quint64>(plaintext.size()))
                    && (plaintext.size() == 0 || partBegin == 1);
            const quint32 totalParts = singlePart ? 1 : partNumber;
            const qint64 filePosition = singlePart ? 0 : static_cast<qint64>(partBegin - 1);
            const qint64 fileSize = singlePart ? static_cast<qint64>(plaintext.size())
                                              : filePosition + static_cast<qint64>(plaintext.size());
            // Line 1 carries the salt raw, so a wire salt must be in the FF1 alphabet;
            // body-vec-04's salt (0102...0f10, contains 0x0A/0x0D) is primitive-level
            // only and has no valid wire article, so it is skipped here.
            const QByteArray salt = fixtureBytes(vector, "salt");
            bool wireSalt = salt.size() == 16;
            for(char byte : salt)
                wireSalt = wireSalt && FF1Cipher::isAlphabetByte(static_cast<uchar>(byte));
            if(wireSalt)
            {
                YencEncryptionContext context(key, salt, segmentIndex);
                QByteArray article;
                quint32 wireCrc = 0;
                QString articleError;
                QVERIFY2(Yenc::encodeArticle(plaintext, partNumber, totalParts, fileSize, filePosition,
                                              nameBytes, context, article, wireCrc, &articleError),
                         qPrintable(articleError));
                const QString producedLine = QStringLiteral("=yencryption cipher=XChaCha20-Poly1305 salt=")
                        + QString::fromLatin1(salt.toHex())
                        + QStringLiteral(" index=")
                        + QStringLiteral("%1").arg(segmentIndex, 8, 16, QLatin1Char('0'))
                        + QStringLiteral(" tag=") + QString::fromLatin1(encrypted.tag.toHex());

                // Rebuild the plain block encodeArticle must have produced: same
                // =ybegin/=ypart framing and yEnc body over the computed ciphertext.
                QByteArray plainBlock;
                if(singlePart)
                    plainBlock = QByteArrayLiteral("=ybegin line=128 size=") + QByteArray::number(fileSize)
                            + QByteArrayLiteral(" name=") + nameBytes + QByteArrayLiteral("\r\n");
                else
                    plainBlock = QByteArrayLiteral("=ybegin part=") + QByteArray::number(partNumber)
                            + QByteArrayLiteral(" total=") + QByteArray::number(totalParts)
                            + QByteArrayLiteral(" line=128 size=") + QByteArray::number(fileSize)
                            + QByteArrayLiteral(" name=") + nameBytes + QByteArrayLiteral("\r\n")
                            + QByteArrayLiteral("=ypart begin=") + QByteArray::number(filePosition + 1)
                            + QByteArrayLiteral(" end=") + QByteArray::number(filePosition + plaintext.size())
                            + QByteArrayLiteral("\r\n");
                plainBlock += producedLine.toLatin1() + QByteArrayLiteral("\r\n");
                const qint64 maxEncoded = static_cast<qint64>(encrypted.ciphertext.size()) * 2
                        + (static_cast<qint64>(encrypted.ciphertext.size()) / 32) + 64;
                QByteArray encoded(static_cast<int>(maxEncoded), Qt::Uninitialized);
                quint32 rebuiltCrc = 0;
                const qint64 encodedSize = Yenc::encode(encrypted.ciphertext.constData(), encrypted.ciphertext.size(),
                                                        reinterpret_cast<uchar *>(encoded.data()), rebuiltCrc);
                QVERIFY(encodedSize > 0);
                QCOMPARE(rebuiltCrc, wireCrc);
                encoded.resize(static_cast<int>(encodedSize - 1));
                plainBlock += encoded;
                if(!encoded.isEmpty() && !encoded.endsWith("\r\n"))
                    plainBlock += QByteArrayLiteral("\r\n");
                plainBlock += QByteArrayLiteral("=yend size=") + QByteArray::number(encrypted.ciphertext.size());
                if(singlePart)
                    plainBlock += QByteArrayLiteral(" crc32=");
                else
                    plainBlock += QByteArrayLiteral(" part=") + QByteArray::number(partNumber)
                            + QByteArrayLiteral(" pcrc32=");
                plainBlock += QByteArray::number(wireCrc, 16).rightJustified(8, '0') + QByteArrayLiteral("\r\n");

                QByteArray reEncrypted;
                QVERIFY2(FF1Cipher::encryptControlLines(plainBlock, key, segmentIndex, salt,
                                                        reEncrypted, &articleError),
                         qPrintable(articleError));
                QCOMPARE(reEncrypted, article);
                QCOMPARE(article.left(16), salt);
            }
        }

        QByteArray restored;
        QVERIFY2(testDecryptBody(encrypted.ciphertext, encrypted.tag, key, segmentIndex, expectedAad, restored, &error),
                 qPrintable(error));
        QCOMPARE(restored, plaintext);

        QByteArray tamperedTag = encrypted.tag;
        tamperedTag[0] = static_cast<char>(tamperedTag.at(0) ^ 1);
        QVERIFY(!testDecryptBody(encrypted.ciphertext, tamperedTag, key, segmentIndex, expectedAad, restored, &error));
        QVERIFY(restored.isEmpty());
    }
}

void CryptoTest::vendoredIndexAllocationVectors()
{
    const QJsonArray vectors = loadVectorJson("index_allocation.json").value(QStringLiteral("vectors")).toArray();
    QVERIFY(!vectors.isEmpty());
    for(const QJsonValue &value : vectors)
    {
        const QJsonObject vector = value.toObject();
        const quint32 candidate = static_cast<quint32>(vector.value(QStringLiteral("candidate_index")).toVariant().toUInt());
        QVERIFY(SegmentIndexAllocator::hasForbiddenByte(candidate));
        SegmentIndexAllocator allocator(candidate);
        quint32 assigned = 0;
        QVERIFY(allocator.next(assigned));
        QCOMPARE(assigned, static_cast<quint32>(vector.value(QStringLiteral("expected_assigned_index")).toInt()));
    }
}

void CryptoTest::encryptControlLinesDotStuffsLeadingDot()
{
    // Control Std Section 9 / RFC 3977 3.1.1: FF1Cipher::encryptControlLines
    // prepends '.' when an encrypted control line starts with '.'. Search
    // segment indices deterministically until some control line's ciphertext
    // starts with '.', then assert the wire carries the stuffed '..'.
    const QByteArray salt("K7mX9pL2qR8vN4wZ", 16);
    QByteArray masterKey;
    QString error;
    QVERIFY2(CryptoEngine::deriveKey(QStringLiteral("public-conformance-vector-key-2026"), salt, masterKey, &error), qPrintable(error));

    const QByteArray plainBlock = QByteArrayLiteral("=ybegin line=128 size=4 name=file.bin\r\n")
            + QByteArrayLiteral("=yencryption cipher=XChaCha20-Poly1305 salt=") + salt.toHex()
            + QByteArrayLiteral(" index=00000001 tag=24d9229edfc04aeaeedcf105bc91e695\r\n")
            + QByteArrayLiteral("data\r\n=yend size=4\r\n");

    // Pinned deterministic case (found by the same search, then frozen):
    // segmentIndex 511 at lineIndex 4 makes the =yend control line's FF1
    // ciphertext start with '.' (0x2E; ciphertext begins 2e6c3c6f...). Pin
    // it explicitly so the leading-dot case is exercised by construction,
    // not only if the search below happens to find one; the search remains
    // as a cross-check over other indices. (An earlier pin used segmentIndex
    // 22, which no longer yields a '.'-initial ciphertext with the current
    // FF1 tweak derivation; the search below only covers lineIndex 1..3 for
    // the first three lines of the block, so the =yend/lineIndex-4 case is
    // pinned here instead of searched.)
    {
        QByteArray raw;
        QVERIFY2(FF1Cipher::encryptLine(QByteArrayLiteral("=yend size=4"), masterKey, 511,
                                        4, salt, raw, &error),
                 qPrintable(error));
        QVERIFY2(raw.startsWith('.'),
                 qPrintable(QStringLiteral("pinned segmentIndex 511 =yend ciphertext no longer "
                                           "starts with '.': %1").arg(QString::fromLatin1(raw.left(8).toHex()))));
        QByteArray wire;
        QVERIFY2(FF1Cipher::encryptControlLines(plainBlock, masterKey, 511, salt, wire, &error),
                 qPrintable(error));
        QVERIFY2(wire.contains(QByteArrayLiteral("..") + raw.mid(1)),
                 "wire does not contain the dot-stuffed '..' form of the '.'-initial =yend ciphertext");
    }

    bool found = false;
    for(quint32 segmentIndex = 1; segmentIndex < 512 && !found; ++segmentIndex)
    {
        // encryptLine gives the unstuffed ciphertext for each control line.
        // Note: an FF1 ciphertext may itself contain 0x0A, so wire lines are
        // recovered by expected lengths rather than by splitting on '\n'.
        const QList<QByteArray> plainLines = plainBlock.split('\n');
        QByteArray wire;
        QVERIFY2(FF1Cipher::encryptControlLines(plainBlock, masterKey, segmentIndex, salt, wire, &error),
                 qPrintable(error));
        int offset = 0;
        for(int i = 0; i < plainLines.size() - 1; ++i)
        {
            QByteArray plainContent = plainLines.at(i);
            if(plainContent.endsWith('\r'))
                plainContent.chop(1);
            const bool isControl = plainContent.startsWith("=y");
            if(!isControl)
            {
                const int len = plainLines.at(i).size() + 1; // content + CR + LF, minus split '\n'
                QCOMPARE(wire.mid(offset, plainLines.at(i).size()), plainLines.at(i));
                offset += len;
                continue;
            }
            QByteArray raw;
            QVERIFY2(FF1Cipher::encryptLine(plainContent, masterKey, segmentIndex,
                                            static_cast<quint32>(i + 1), salt, raw, &error),
                     qPrintable(error));
            const QByteArray expectedWire = raw.startsWith('.') ? QByteArrayLiteral(".") + raw : raw;
            QCOMPARE(wire.mid(offset, expectedWire.size()), expectedWire);
            if(raw.startsWith('.'))
            {
                QVERIFY(expectedWire.startsWith(".."));
                found = true;
            }
            offset += expectedWire.size() + 2; // plus the preserved CRLF ending
        }
        QCOMPARE(offset, wire.size());
    }
    QVERIFY2(found, "no leading-dot control ciphertext found in searched segment indices");
}

QTEST_GUILESS_MAIN(CryptoTest)

#include "CryptoTest.moc"
