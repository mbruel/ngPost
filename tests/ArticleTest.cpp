#include <QtTest>

#include "SegmentIndexAllocator.h"
#include "crypto/CryptoEngine.h"
#include "utils/Yenc.h"

class ArticleTest : public QObject
{
    Q_OBJECT

private slots:
    void encryptedSinglePartUsesStrictFraming();
    void encryptedMultipartUsesStrictFraming();
    // Q-1 / NGPOST-TESTS-06: final part (part=2,totalParts=2, non-zero filePosition)
    void encryptedMultipartFinalPartUsesStrictFraming();
    void segmentIndicesProgressReleaseWide();
    void segmentIndexAllocatorRejectsExhaustion();
    // Index framing rule: allocator must skip indices whose uint32_be contains 0x0A/0x0D
    void segmentIndexAllocatorSkipsForbiddenBytes();
    // Worst-case escape payloads must not overflow the encode buffer
    void worstCaseEscapePayloadDoesNotOverflow();
    // Exact-128-column wrap must not inject blank lines
    void exactWrapPreservesSingleCrlf();
};

void ArticleTest::encryptedSinglePartUsesStrictFraming()
{
    const QByteArray plaintext = QByteArray::fromHex("48656c6c6f20576f726c642e747874ff");
    const QByteArray salt("K7mX9pL2qR8vN4wZ", 16);
    QByteArray masterKey;
    QString error;
    QVERIFY2(CryptoEngine::deriveKey(QStringLiteral("conformance-fixture-password"), salt, masterKey, &error),
             qPrintable(error));
    YencEncryptionContext encryption(masterKey, salt, 1);

    QByteArray wire;
    quint32 crc32 = 0;
    QVERIFY2(Yenc::encodeArticle(plaintext, 1, 1, plaintext.size(), 0, QByteArrayLiteral("file.bin"),
                                 encryption, wire, crc32, &error), qPrintable(error));
    QVERIFY(crc32 != 0);
    QCOMPARE(wire.left(16), salt);
    QCOMPARE(wire.mid(16, 4), QByteArray::fromHex("00000001"));
    QVERIFY(!wire.contains("=ybegin"));
    QVERIFY(!wire.contains("=ypart"));
    QVERIFY(!wire.contains("=yencryption"));
    QVERIFY(!wire.contains("=yend"));
    const QList<QByteArray> lines = wire.split('\n');
    // 4 lines + trailing empty element after the final CRLF.
    QCOMPARE(lines.size(), 5);
    QVERIFY(lines.at(4).isEmpty());
}

void ArticleTest::encryptedMultipartUsesStrictFraming()
{
    const QByteArray plaintext = QByteArray::fromHex("48656c6c6f20576f726c642e747874ff");
    const QByteArray salt("K7mX9pL2qR8vN4wZ", 16);
    QByteArray masterKey;
    QString error;
    QVERIFY2(CryptoEngine::deriveKey(QStringLiteral("conformance-fixture-password"), salt, masterKey, &error),
             qPrintable(error));
    YencEncryptionContext encryption(masterKey, salt, 1);

    QByteArray wire;
    quint32 crc32 = 0;
    QVERIFY2(Yenc::encodeArticle(plaintext, 1, 2, plaintext.size() * 2, 0, QByteArrayLiteral("file.bin"),
                                 encryption, wire, crc32, &error), qPrintable(error));
    QVERIFY(crc32 != 0);
    QCOMPARE(wire.left(16), salt);
    QCOMPARE(wire.mid(16, 4), QByteArray::fromHex("00000001"));
    QVERIFY(!wire.contains("=ybegin"));
    QVERIFY(!wire.contains("=ypart"));
    QVERIFY(!wire.contains("=yend"));
    const QList<QByteArray> lines = wire.split('\n');
    // 5 lines + trailing empty element after the final CRLF.
    QCOMPARE(lines.size(), 6);
    QVERIFY(lines.at(5).isEmpty());
}

void ArticleTest::encryptedMultipartFinalPartUsesStrictFraming()
{
    // Final part of a 2-part file: part=2, totalParts=2, non-zero filePosition.
    // Exact wire line structure (split by '\n', each line CR-terminated except
    // the trailing empty element): Line 1 bootstrap (encrypted =ybegin),
    // encrypted =ypart, encrypted =yencryption, >=1 yEnc data line(s),
    // encrypted =yend. Control lines are FF1-encrypted, so the plaintext
    // markers must not appear; the no-whole-file-crc32= rule (Q-1) is asserted
    // structurally on the produced wire (see note below).
    const QByteArray plaintext = QByteArray::fromHex("48656c6c6f20576f726c642e747874ff");
    const QByteArray salt("K7mX9pL2qR8vN4wZ", 16);
    QByteArray masterKey;
    QString error;
    QVERIFY2(CryptoEngine::deriveKey(QStringLiteral("conformance-fixture-password"), salt, masterKey, &error),
             qPrintable(error));
    YencEncryptionContext encryption(masterKey, salt, 1);

    QByteArray wire;
    quint32 crc32 = 0;
    QVERIFY2(Yenc::encodeArticle(plaintext, 2, 2, plaintext.size() * 2, plaintext.size(),
                                  QByteArrayLiteral("file.bin"),
                                  encryption, wire, crc32, &error), qPrintable(error));
    QVERIFY(crc32 != 0);
    QCOMPARE(wire.left(16), salt);
    QCOMPARE(wire.mid(16, 4), QByteArray::fromHex("00000001"));
    QVERIFY(!wire.contains("=ybegin"));
    QVERIFY(!wire.contains("=ypart"));
    QVERIFY(!wire.contains("=yencryption"));
    QVERIFY(!wire.contains("=yend"));
    const QList<QByteArray> lines = wire.split('\n');
    // 5 lines + trailing empty element after the final CRLF.
    QCOMPARE(lines.size(), 6);
    QVERIFY(lines.at(5).isEmpty());
    for(int i = 0; i < 5; ++i)
        QVERIFY2(lines.at(i).endsWith('\r'), qPrintable(QStringLiteral("line %1 not CR-terminated").arg(i)));
    // Whole-file crc32= must never appear on an encrypted multipart =yend;
    // only per-part pcrc32= is allowed (Q-1). The tokens cannot appear
    // literally here because the =yend line is FF1-encrypted, so pin Q-1 by
    // the encrypted =yend line length instead: FF1 is length-preserving
    // (Line 1's +20 bootstrap aside), so "=yend size=16 part=2
    // pcrc32=<8hex>" (36 chars) encrypts to 36 chars, plus the CR kept by
    // the '\n' split = 37. A whole-file "=yend size=16 crc32=<8hex>" would
    // be only 28 chars (+CR). A leading '.' would add one dot-stuffing char
    // on top of the 36; this vector's ciphertext is not '.'-initial, so the
    // exact 37 holds (and would fail loudly if the ciphertext changed).
    QCOMPARE(lines.at(4).size(),
             QByteArrayLiteral("=yend size=16 part=2 pcrc32=00000000\r").size());
}

void ArticleTest::segmentIndicesProgressReleaseWide()
{
    SegmentIndexAllocator indices;
    quint32 segmentIndex = 0;
    QVERIFY(indices.next(segmentIndex));
    QCOMPARE(segmentIndex, quint32(1));
    QVERIFY(indices.next(segmentIndex));
    QCOMPARE(segmentIndex, quint32(2));
    QVERIFY(indices.next(segmentIndex));
    QCOMPARE(segmentIndex, quint32(3));
}

void ArticleTest::segmentIndexAllocatorRejectsExhaustion()
{
    SegmentIndexAllocator indices(0xffffffffU);
    quint32 segmentIndex = 0;
    QVERIFY(indices.next(segmentIndex));
    QCOMPARE(segmentIndex, quint32(0xffffffffU));
    QVERIFY(!indices.next(segmentIndex));
}

void ArticleTest::segmentIndexAllocatorSkipsForbiddenBytes()
{
    // Index framing rule: indices whose uint32_be encoding contains 0x0A
    // or 0x0D would split the Line 1 bootstrap on the wire and must be skipped.
    // The canonical candidate->assigned cases live in the JSON-driven
    // CryptoTest::vendoredIndexAllocationVectors; this slot keeps only the
    // predicate-position and consecutive-crossing checks that suite lacks.
    quint32 segmentIndex = 0;

    // forbidden byte in any of the four positions
    QVERIFY(SegmentIndexAllocator::hasForbiddenByte(0x0A000000U));
    QVERIFY(SegmentIndexAllocator::hasForbiddenByte(0x000D0000U));
    QVERIFY(!SegmentIndexAllocator::hasForbiddenByte(0xFFFFFFFFU));

    // consecutive allocation crossing forbidden values: 8, 9 -> 11 (10 skipped)
    SegmentIndexAllocator cross(8);
    QVERIFY(cross.next(segmentIndex));
    QCOMPARE(segmentIndex, quint32(8));
    QVERIFY(cross.next(segmentIndex));
    QCOMPARE(segmentIndex, quint32(9));
    QVERIFY(cross.next(segmentIndex));
    QCOMPARE(segmentIndex, quint32(11));
}

void ArticleTest::worstCaseEscapePayloadDoesNotOverflow()
{
    // A payload of bytes that all require escaping (0xD6 -> (0xD6+42)&0xFF == 0x00,
    // 0xED -> '=', 0x00 -> '\n') used to overflow the encode destination buffer
    // (size*2+4) because CRLF line terminators were not budgeted. The fixed
    // allocation covers 2N + N/32 + 64 bytes; this test fails under the old
    // allocation (heap-buffer-overflow detected by sanitizers) and must pass now.
    QByteArray payload;
    payload.resize(1000);
    for(int i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<char>(i % 3 == 0 ? char(0xD6) : (i % 3 == 1 ? char(0xED) : char(0x00)));

    const QByteArray salt("K7mX9pL2qR8vN4wZ", 16);
    QByteArray masterKey;
    QString error;
    QVERIFY2(CryptoEngine::deriveKey(QStringLiteral("conformance-fixture-password"), salt, masterKey, &error),
             qPrintable(error));
    YencEncryptionContext encryption(masterKey, salt, 1);
    QByteArray wire;
    quint32 crc32 = 0;
    QVERIFY2(Yenc::encodeArticle(payload, 1, 1, payload.size(), 0, QByteArrayLiteral("file.bin"),
                                 encryption, wire, crc32, &error), qPrintable(error));

    // Wire bytes are FF1-encrypted; the salt/index bootstrap stays verifiable.
    QCOMPARE(wire.left(16), salt);
    QCOMPARE(wire.mid(16, 4), QByteArray::fromHex("00000001"));
    QVERIFY(!wire.contains("=ybegin"));
}

void ArticleTest::exactWrapPreservesSingleCrlf()
{
    // When the encoded stream wraps exactly on a 128-column boundary the
    // old code emitted a double CRLF (blank line) before =yend. Downstream
    // FF1 tweak derivation counts physical lines, so a blank line
    // desynchronized the =yend tweak.
    // Encrypted articles must preserve single CRLF.
    const QByteArray salt("K7mX9pL2qR8vN4wZ", 16);
    QByteArray masterKey;
    QString error;
    QVERIFY2(CryptoEngine::deriveKey(QStringLiteral("conformance-fixture-password"), salt, masterKey, &error),
             qPrintable(error));
    YencEncryptionContext encryption(masterKey, salt, 1);

    // 128 ASCII 'A's encode to 128 column-exact chars and trigger the wrap.
    const QByteArray payload(128, 'A');
    QByteArray wire;
    quint32 crc32 = 0;
    QVERIFY2(Yenc::encodeArticle(payload, 1, 1, payload.size(), 0, QByteArrayLiteral("file.bin"),
                                 encryption, wire, crc32, &error), qPrintable(error));
    QVERIFY(!wire.contains("\r\n\r\n"));
    QVERIFY(!wire.contains("=yend"));
}

QTEST_APPLESS_MAIN(ArticleTest)

#include "ArticleTest.moc"
