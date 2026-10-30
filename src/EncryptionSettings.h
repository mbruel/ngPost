#ifndef ENCRYPTIONSETTINGS_H
#define ENCRYPTIONSETTINGS_H

#include <QString>

struct EncryptionSettings
{
    bool enabled = false;
    QString password;

    // XML 1.0 Char production: #x9 | #xA | #xD | [#x20-#xD7FF] | [#xE000-#xFFFD] | [#x10000-#x10FFFF].
    // Passwords that fail this cannot round-trip through <meta type="password"> (Body Std Section 4),
    // so they are refused before posting. Surrogate pairs are accepted; lone surrogates are not.
    static bool isXmlRepresentable(const QString &text)
    {
        for (int i = 0; i < text.size(); ++i)
        {
            const ushort unit = text.at(i).unicode();
            if (unit >= 0xD800 && unit <= 0xDBFF)
            {
                if (i + 1 < text.size() && text.at(i + 1).unicode() >= 0xDC00 && text.at(i + 1).unicode() <= 0xDFFF)
                {
                    ++i;
                    continue;
                }
                return false; // unpaired high surrogate
            }
            if (unit >= 0xDC00 && unit <= 0xDFFF)
                return false; // unpaired low surrogate
            if (unit == 0x9 || unit == 0xA || unit == 0xD)
                continue;
            if (unit >= 0x20 && unit <= 0xFFFD)
                continue;
            return false; // C0 controls, U+FFFE and U+FFFF
        }
        return true;
    }

    // Saving preserves the opt-in switch; credentials are supplied separately.
    QString configLine() const
    {
        return enabled ? QStringLiteral("ENCRYPT = true\n") : QString();
    }

    // Body Encryption Standard v1.2 §2.1: checked before any posting.
    static bool acceptsFileSize(qint64 size)
    {
        return size >= 0 && size < (qint64(1) << 32);
    }

    bool validate(QString *error = nullptr) const
    {
        if(enabled && password.isEmpty())
        {
            if(error)
                *error = QStringLiteral("Encryption requires a non-empty password");
            return false;
        }
        // The message deliberately does not echo the password.
        if(enabled && !isXmlRepresentable(password))
        {
            if(error)
                *error = QStringLiteral("Encryption password contains characters that cannot be represented in XML 1.0 (control characters or unpaired surrogates); choose another password");
            return false;
        }
        return true;
    }

    void clearPassword()
    {
        password.detach();
        password.fill(QChar(0));
        password.clear();
    }
};

#endif
