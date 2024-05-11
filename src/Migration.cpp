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

#include "Migration.h"
#include <QStringList>

#include "NgPost.h"
#include "utils/Database.h"


#include "ResumeJobQueue.h"

Migration::Migration(const NgPost &ngPost) : _ngPost(ngPost)
{}

bool Migration::migrate()
{
    if (!_ngPost.initHistoryDatabase())
        return false;

    QStringList v = NgPost::version().split(".");
    int buildVersion = v.at(0).toInt()*100 + v.at(1).toInt();

    QString confVersionStr = NgPost::confVersion();
    if (confVersionStr.isEmpty())
        _doMigration();
    else
    {
        v = confVersionStr.split(".");
        int confBuild = v.at(0).toInt()*100 + v.at(1).toInt();
        if (confBuild > buildVersion)
        {
            qDebug() << "Error conf version > ngPost version...";
            return false;
        }
        if (buildVersion == confBuild)
            qDebug() << "Config Version up to date :)";
        else
            _doMigration(confBuild);
    }
    return true;
}

void Migration::_doMigration(short confBuild)
{
    switch(confBuild)
    {
    case 1 ... 416:
        _migrateTo417();
    // no break so all new updates will be done!
    }

    ResumeJobQueue::postedFilesFromNzb(_ngPost,
                                       "/home/mb/Downloads/nzb/Stephane.Guillon.Sur.Scene.A.La.Cigale.2023.French.WEB.1080P.H264.nzb");
//    _ngPost.saveConfig(); // Update conf
}

void Migration::_migrateTo417()
{
    qDebug() << "Migrate config to build 417";
    const QString &postHistoryFile = _ngPost.postHistoryFile();
    if (postHistoryFile.isEmpty())
        return;
    QFile hist(postHistoryFile);
    if (hist.open(QIODevice::ReadOnly|QIODevice::Text))
    {
        Database *db = _ngPost.historyDatabase();
        QString const &histSeparator = _ngPost.historyFieldSeparator();
        QTextStream stream(&hist);
        stream.readLine(); // skip header
        while (!stream.atEnd())
        {
            // date;nzb name;size;avg. speed;archive name;archive pass;groups;from
            QString line = stream.readLine();
            QStringList v = line.split(histSeparator);
            if (v.size() != 8)
            {
                qDebug() << "Error parsing history csv line: " << line;
            }
            else
            {
                QString dateStr = v.at(0);// sizeStr = v.at(2), speedStr = v.at(3);
                db->insertPost(dateStr.replace("/", "-"), v.at(1), v.at(2), v.at(3),
//                               Database::byteSize(sizeStr), Database::byteSize(speedStr),
                               v.at(4), v.at(5), v.at(6), v.at(7), "", "", 1);
//                qDebug() << "[ " << dateStr << "] " << v.at(1)
//                         << " : " << v.at(2) << " at " << v.at(3);
            }
        }
        qDebug() << postHistoryFile << " has been migrated to Sqlite DB!\n"
                 << "Posted size: " << db->postedSize(Database::DATE_CONDITION::ALL)
                 << " (this year: " << db->postedSize(Database::DATE_CONDITION::YEAR)
                 << ", this month: " << db->postedSize(Database::DATE_CONDITION::MONTH)
                 << ")";
    }
}
