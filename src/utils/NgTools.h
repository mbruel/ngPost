//========================================================================
//
// Copyright (C) 2024 Matthieu Bruel <Matthieu.Bruel@gmail.com>
// This file is a part of ngPost : https://github.com/mbruel/ngPost
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3..
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>
//
//========================================================================
#ifndef NGTOOLS_H
#define NGTOOLS_H

#include "../NgConf.h"
#include "./PureStaticClass.h"
class QFileInfo;
namespace NgConf
{
inline const QString kAmpXml   = QStringLiteral("&amp;");
inline const QString kLtXml    = QStringLiteral("&lt;");
inline const QString kGtXml    = QStringLiteral("&gt;");
inline const QString kQuoteXml = QStringLiteral("&quot;");
inline const QString kAPosXml  = QStringLiteral("&apos;");

inline const QString kAmpStr   = QStringLiteral("&");
inline const QString kLtStr    = QStringLiteral("<");
inline const QString kGtStr    = QStringLiteral(">");
inline const QString kQuoteStr = QStringLiteral("\"");
inline const QString kAPosStr  = QStringLiteral("'");

inline const QChar kAmpChar   = '&';
inline const QChar kLtChar    = '<';
inline const QChar kGtChar    = '>';
inline const QChar kQuoteChar = '"';
inline const QChar kAPosChar  = '\'';
} // namespace NgConf

using namespace NgConf;

class NgTools : public PureStaticClass
{
public:
    static void removeAccentsFromString(QString &str);

    static qint64 recursivePathSize(QFileInfo const &fi);

    inline static QString humanSize(double size);

    inline static std::string randomStdFrom(ushort length = 13);

    inline static QString randomFrom(ushort length);

    inline static QString randomFileName(uint length = 13);

    inline static QString randomPass(uint length = 13);

    inline static QString getConfigurationFilePath();

    inline static QString escapeXML(char const *str);
    inline static QString escapeXML(QString const &str);
    inline static QString xml2txt(char const *str);
    inline static QString xml2txt(QString const &str);
};

QString NgTools::humanSize(double size)
{
    QString unit = "B";
    if (size > 1024)
    {
        size /= 1024;
        unit = "kB";
    }
    if (size > 1024)
    {
        size /= 1024;
        unit = "MB";
    }
    if (size > 1024)
    {
        size /= 1024;
        unit = "GB";
    }
    return QString("%1 %2").arg(size, 0, 'f', 2).arg(unit);
}

std::string NgTools::randomStdFrom(ushort length)
{
    size_t      nbLetters = kRandomAlphabet.length();
    std::string randomFrom;
    randomFrom.reserve(length + kArticleIdSignature.length() + 5);
    for (size_t i = 0; i < length; ++i)
        randomFrom.push_back(kRandomAlphabet.at(std::rand() % nbLetters));
    randomFrom.push_back('@');
    randomFrom.append(kArticleIdSignature);
    randomFrom.append(".com");

    return randomFrom;
}

QString NgTools::randomFrom(ushort length) { return QString::fromStdString(randomStdFrom(length)); }

QString NgTools::randomFileName(uint length)
{
    QString    pass;
    static int nbLetters = kFileNameAlphabet.length();
    for (uint i = 0; i < length; ++i)
        pass.append(kFileNameAlphabet.at(std::rand() % nbLetters));
    return pass;
}

QString NgTools::randomPass(uint length)
{
    QString    pass;
    static int nbLetters = kPasswordAlphabet.length();
    for (uint i = 0; i < length; ++i)
        pass.append(kPasswordAlphabet.at(std::rand() % nbLetters));
    return pass;
}

QString NgTools::getConfigurationFilePath()
{
#if defined(WIN32) || defined(__MINGW64__)
    return kDefaultConfig;
#else
    return QString("%1/%2").arg(getenv("HOME")).arg(kDefaultConfig);
#endif
}

QString NgTools::escapeXML(char const *str)
{
    QString escaped(str);
    escaped.replace(kAmpChar, kAmpXml);
    escaped.replace(kLtChar, kLtXml);
    escaped.replace(kGtChar, kGtXml);
    escaped.replace(kQuoteChar, kQuoteXml);
    escaped.replace(kAPosChar, kAPosXml);
    return escaped;
}

QString NgTools::escapeXML(QString const &str)
{
    QString escaped(str);
    escaped.replace(kAmpChar, kAmpXml);
    escaped.replace(kLtChar, kLtXml);
    escaped.replace(kGtChar, kGtXml);
    escaped.replace(kQuoteChar, kQuoteXml);
    escaped.replace(kAPosChar, kAPosXml);
    return escaped;
}

QString NgTools::xml2txt(char const *str)
{

    QString escaped(str);
    escaped.replace(kAmpXml, kAmpStr);
    escaped.replace(kLtXml, kLtStr);
    escaped.replace(kGtXml, kGtStr);
    escaped.replace(kQuoteXml, kQuoteStr);
    escaped.replace(kAPosXml, kAPosStr);
    return escaped;
}

QString NgTools::xml2txt(QString const &str)
{
    QString escaped(str);
    escaped.replace(kAmpXml, kAmpStr);
    escaped.replace(kLtXml, kLtStr);
    escaped.replace(kGtXml, kGtStr);
    escaped.replace(kQuoteXml, kQuoteStr);
    escaped.replace(kAPosXml, kAPosStr);
    return escaped;
}

#endif // NGTOOLS_H
