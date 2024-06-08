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
    qDebug() << "Migrate config to build 417";
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
                QString dateStr = v.at(0);                 // sizeStr = v.at(2), speedStr = v.at(3);
                db->_insertPost(dateStr.replace("/", "-"), // date
                                v.at(1),                   // nzbName
                                v.at(2),                   // size (human readable string)
                                v.at(3),                   // avgSpeed (human readable)
                                v.at(4),                   // archiveName
                                v.at(5),                   // archivePass
                                v.at(6),                   // groups
                                v.at(7),                   // poster
                                "",                        // tmpPath not saved in csv
                                "",                        // nzbFilePath not saved in csv
                                0,                         // nbFiles (we don't know?)
                                1);                        // upload finished (done)
            }
        }
        qDebug() << postHistoryFile << " has been migrated to Sqlite DB!\n"
                 << "Posted size: " << db->postedSize(Database::DATE_CONDITION::ALL)
                 << " (this year: " << db->postedSize(Database::DATE_CONDITION::YEAR)
                 << ", this month: " << db->postedSize(Database::DATE_CONDITION::MONTH) << ")";
    }
    return true;
}
