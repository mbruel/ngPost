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
#include "NgTools.h"
#include "NgLogger.h"
#include <QDateTime>
#include <QDebug> // MB_TODO to be removed"
#include <QDir>
#include <QFileInfo>

QString NgTools::substituteExistingFile(QString const &path, bool isNzbFile, bool useBasename)
{
    QString   inputPath = path;
    QFileInfo fi(inputPath);
    ushort    nbDuplicates = 0;
    while (fi.exists())
    {
        QString baseName = useBasename ? fi.completeBaseName() : fi.fileName();
        if (nbDuplicates != 0)
        {
            static QChar kUnderscore('_');
            baseName.chop(baseName.length() - baseName.lastIndexOf(kUnderscore));
        }
        inputPath = QFileInfo(fi.absoluteDir(),
                              QString("%1_%2%3").arg(baseName).arg(++nbDuplicates).arg(isNzbFile ? ".nzb" : ""))
                            .absoluteFilePath();
        fi = QFileInfo(inputPath);
    }
    return inputPath;
}

uint NgTools::getUShortVersion(QString const &version)
{
    if (version.isEmpty())
        return 0; // no version
    QStringList majorAndMinor = version.split(".");
    ushort      ushortVesion  = majorAndMinor.at(0).toInt() * 100;
    if (majorAndMinor.size() > 1) // as 5.0 = 5 has no minor contrary to 5.11
        ushortVesion += majorAndMinor.at(1).toUShort();
    return ushortVesion;
}

//! return 0 if up to date with built version
//! otherwise send back the configuration version as ushort
ushort NgTools::isConfigurationVesionObsolete()
{
    ushort builtVersion = getUShortVersion(kVersion);
    ushort confVersion  = getUShortVersion(kConfVersion);
    qDebug() << "[MB_TRACE] isConfigurationVesionObsolete, kVersion: " << kVersion
             << " kConfVersion: " << kConfVersion;
    if (confVersion < builtVersion)
    {
        NgLogger::error(tr("Configuration version needs update!"));
        return confVersion;
    }
    else
    {
        NgLogger::log(tr("Config Version up to date :)"), true);
        return 0; // configuration version up to date
    }
}

QString NgTools::currentDateTime() { return QDateTime::currentDateTime().toString(kDateTimeFormat); }

QString NgTools::timestamp() { return QTime::currentTime().toString("hh:mm:ss.zzz"); }

void NgTools::removeAccentsFromString(QString &str)
{
    static const QRegularExpression regEx_A("[ÀÁÂÃÄÅ]");
    static const QRegularExpression regEx_a("[àáâãäå]");
    static const QRegularExpression regEx_E("[ÈÉÊË]");
    static const QRegularExpression regEx_e("[èéêë]");
    static const QRegularExpression regEx_I("[ÌÍÎÏ]");
    static const QRegularExpression regEx_i("[ìíîï]");
    static const QRegularExpression regEx_O("[ÒÓÔÕÖØ]");
    static const QRegularExpression regEx_o("[òóôõöø]");
    static const QRegularExpression regEx_U("[ÙÚÛÜ]");
    static const QRegularExpression regEx_u("[ùúûü]");
    static const QRegularExpression regEx_Y("[ÿý]");
    static const QRegularExpression regEx_sp("[^A-Za-z0-9\\.,_\\-\\(\\)\\[\\]\\{\\}!#&'\\+ ]");

    str.replace(regEx_A, "A");
    str.replace(regEx_a, "a");
    str.replace("Ç", "C");
    str.replace("ç", "c");
    str.replace(regEx_E, "E");
    str.replace(regEx_e, "e");
    str.replace(regEx_I, "I");
    str.replace(regEx_i, "i");
    str.replace("Ñ", "N");
    str.replace("ñ", "n");
    str.replace(regEx_O, "O");
    str.replace(regEx_o, "o");
    str.replace(regEx_U, "U");
    str.replace(regEx_u, "u");
    str.replace(regEx_Y, "y");
    str.replace(regEx_sp, "");
}

qint64 NgTools::recursivePathSize(QFileInfo const &fi)
{
    if (fi.isFile())
        return fi.size();
    if (fi.isDir())
    {
        static QDir::Filters kAllFilesAndFolder =
                QDir::Files | QDir::Hidden | QDir::System | QDir::Dirs | QDir::NoDotAndDotDot;
        qint64 size = 4096; // size of a dir on Unix
        QDir   dir(fi.absoluteFilePath());
        for (QFileInfo const &subFi : dir.entryInfoList(kAllFilesAndFolder))
        {
            if (subFi.isDir())
                size += recursivePathSize(subFi);
            else
                size += subFi.size();
        }
        return size;
    }
    else
        return 0;
}
