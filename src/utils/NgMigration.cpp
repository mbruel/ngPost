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

#include "NgMigration.h"
#include <QStringList>

#include "NgPost.h"
#include "utils/NgTools.h"

#include "NgHistoryDatabase.h"
#include "PostingJob.h"

NgMigration::NgMigration(NgPost &ngPost) : _ngPost(ngPost) { }

bool NgMigration::migrate()
{
    if (!_ngPost.initHistoryDatabase())
        return false;

    const uint confVersion = NgTools::isConfigurationVesionObsolete();
    if (confVersion != 0)
        _doNgMigration(confVersion);
    return true;
}

void NgMigration::_doNgMigration(unsigned short const confBuild)
{
    bool migrationDone = false;
#ifdef _MSC_VER // MSVC compiler...
    // Fallback for other compilers
    if (confBuild < 500)
        migrationDone = _migrateTo500();
#else
    switch (confBuild)
    {
    case 1 ... 416:
        migrationDone = _migrateTo500();

        // no break so all new updates will be done!
    }
#endif // <-- Add this line
       //
    if (migrationDone)
        _ngPost.saveConfig(); // Update conf
}

bool NgMigration::_migrateTo500()
{
    NgLogger::log(tr("Migrate config to build v5.0"), true);
    QString const &postHistoryFile = _ngPost.postHistoryFile();
    if (postHistoryFile.isEmpty())
        return false;
    QFile hist(postHistoryFile);
    if (hist.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        NgHistoryDatabase *db            = _ngPost.historyDatabase();
        QString const     &histSeparator = _ngPost.historyFieldSeparator();
        QTextStream        stream(&hist);
        stream.readLine(); // skip header
        while (!stream.atEnd())
        {
            // date;nzb name;size;avg. speed;archive name;archive pass;groups;from
            QString     line = stream.readLine();
            QStringList v    = line.split(histSeparator);
            if (v.size() != 8)
            {
                qDebug() << "Error parsing history csv line: " << line;
            }
            else
            {
                QString dateStr = v.at(0);
                db->_insertPost(dateStr.replace("/", "-"),       // date
                                v.at(1),                         // nzbName
                                _sizeInMbFromStr(v.at(2)),       // sizeMB (from human readable string)
                                _avgSpeedInKbpsFromStr(v.at(3)), // avgSpeedKbps (from human readable)
                                v.at(4),                         // archiveName
                                v.at(5),                         // archivePass
                                v.at(6),                         // groups
                                v.at(7),                         // poster
                                "",                              // tmpPath not saved in csv
                                "",                              // nzbFilePath not saved in csv
                                0,                               // nbFiles (we don't know?)
                                PostingJob::JOB_STATE::POSTED);  // upload finished (done)
            }
        }
        NgLogger::log(tr("NgPost has been migrated to use an Sqlite Database!"), true);
        db->dumpStatistics();
    }
    return true;
}

double NgMigration::_sizeInMbFromStr(QString const &sizeStr) const
{
    static const QRegularExpression kByteSizeRegExp("(\\d+\\.\\d{2}) ([kMG]?B)");

    QRegularExpressionMatch match = kByteSizeRegExp.match(sizeStr);
    if (!match.hasMatch())
        return 0.;

    QString unit = match.captured(2);
    if (unit == "B")
        return 0.;
    double size = match.captured(1).toDouble();
    if (unit == "GB")
        size *= 1024;
    else if (unit == "kB")
        size /= 1024;

    return size;
}

double NgMigration::_avgSpeedInKbpsFromStr(QString const &avgSpeedStr)
{
    static const QRegularExpression kAvgSpeedRegExp("(\\d+\\.\\d{2}) ([ kM]B)/s");

    QRegularExpressionMatch match = kAvgSpeedRegExp.match(avgSpeedStr);
    if (!match.hasMatch())
        return 0.;

    double  avgSpeed = match.captured(1).toDouble();
    QString unit     = match.captured(2);
    if (unit == " B")
        avgSpeed /= 1024.;
    else if (unit == "MB")
        avgSpeed *= 1024;

    return avgSpeed;
}
