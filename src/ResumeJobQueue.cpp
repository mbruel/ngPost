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

#include "ResumeJobQueue.h"
#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include "utils/NgLogger.h"
#include "utils/NgTools.h"

QStringList ResumeJobQueue::postedFilesFromNzb(QString const &nzbPath)
{
    QFile xmlFile(nzbPath);
    if (!xmlFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        NgLogger::error(QString("Can't open nzb file: %1").arg(nzbPath));
        return QStringList();
    }
    QXmlStreamReader xmlReader(&xmlFile);

    static const QRegularExpression sSubjectRegExp(".*\"(.*)\".*");
    QStringList                     postedFileNames;
    while (!xmlReader.atEnd() && !xmlReader.hasError())
    {
        // Read next element
        QXmlStreamReader::TokenType token = xmlReader.readNext();

        // If token is StartElement - read it
        if (token == QXmlStreamReader::StartElement)
        {
            if (xmlReader.name() == QStringLiteral("file"))
            {
                QStringView subject = xmlReader.attributes().value(QStringLiteral("subject"));
                //                qDebug() << "[ResumeJobQueue::postedFilesFromNzb] has file with subject: " <<
                //                subject;
                QRegularExpressionMatch match = sSubjectRegExp.match(NgTools::xml2txt(subject.toString()));
                if (match.hasMatch())
                    postedFileNames << NgTools::xml2txt(match.captured(1));
            }
        }
    }

    if (xmlReader.hasError())
    {
        qDebug() << "[postedFilesFromNzb] Error parsing nzb " << nzbPath << " : " << xmlReader.errorString();
        return postedFileNames;
    }

    qDebug() << "[postedFilesFromNzb] postedFileNames: " << postedFileNames;

    // close reader and flush file
    xmlReader.clear();
    xmlFile.close();

    return postedFileNames;
}
