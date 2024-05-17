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

#include "Database.h"
#include <QDebug>

#include <QSqlQuery>
#include <QSqlError>

#include <QFileInfo>
#include <QRegularExpression>

Database::Database():
    _type(TYPE::SQLITE)
  , _db()
{}

Database::~Database()
{
    _db.close();
    QSqlDatabase::removeDatabase(kDriverSqlite);
}

bool Database::initSQLite(const QString &dbPath)
{
    // 1.: SQLite driver not available
    _db = QSqlDatabase::addDatabase(kDriverSqlite);
    if (!_db.isValid()){
        qCritical() << "Error loading Sqlite driver... " << _db.lastError().text();
        return false;
    }

    // 2.: open the default pass one
    bool dbAlreadyExists = QFileInfo(dbPath).exists();
    qDebug() << "Database path: " << dbPath << ", already exist? " << dbAlreadyExists;

    // When using the SQLite driver, open() will create the SQLite database if it doesn't exist.
    _db.setDatabaseName(dbPath);
    if (!_db.open()) {
        qCritical() << "Cannot open database: " << qPrintable(_db.lastError().text());
        return false;
    }

    QSqlQuery query;
    query.exec("PRAGMA journal_mode = WAL");
    query.exec("PRAGMA wal_autocheckpoint = 16");
    query.exec("PRAGMA journal_size_limit = 1536");
    if (!dbAlreadyExists)
    {
        qInfo() << "Creating database...";
        int nbErrors = _execSqlFile(":/db/tables.sql");
        if (nbErrors != 0) {
            qCritical() << "Error creating Database (" << nbErrors << ")"
                        << " => data not created...";
            return false;
        }
    }
    return true;
}


int Database::insertPost(const QString &date, const QString &nzbName,
                         const QString &size, const QString &avgSpeed,
                         const QString &archiveName, const QString &archivePass,
                         const QString &groups, const QString &from,
                         const QString &tmpPath, const QString &nzbFilePath,
                         int done)
{
    QSqlQuery query;
    query.prepare(kInsertStatement);
    query.bindValue(":date", date);
//    query.bindValue(":date", date.toString("yyyy-MM-dd"));
    query.bindValue(":nzbName", nzbName);
    query.bindValue(":size", size);
    query.bindValue(":avgSpeed", avgSpeed);
    query.bindValue(":archiveName", archiveName);
    query.bindValue(":archivePass", archivePass);
    query.bindValue(":groups", groups);
    query.bindValue(":from", from);
    query.bindValue(":tmpPath", tmpPath);
    query.bindValue(":nzbFilePath", nzbFilePath);
    query.bindValue(":done", done);

    if (!query.exec())
    {
        qCritical() << "Error inserting post to history DB: " << query.lastError().text()
                    << " (query: " << query.lastQuery() << ")";
        return 0;
    }

//    int postId = query.lastInsertId().toInt();
//    qDebug() << "Post ID for " << nzbName << " : " << postId;
    return query.lastInsertId().toInt();
}

qint64 Database::postedSizeInMB(DATE_CONDITION since) const
{
    qint64 megas = 0;
    QString req(kSizeSelect);
    if (since != DATE_CONDITION::ALL)
    {
        req.append(" WHERE ");
        switch (since)
        {
        case DATE_CONDITION::YEAR:
            req.append(kYearCondition);
            break;
        case DATE_CONDITION::MONTH:
            req.append(kMonthCondition);
            break;
        case DATE_CONDITION::WEEK:
            req.append(kWeekCondition);
            break;
        case DATE_CONDITION::DAY:
            req.append(kDayCondition);
            break;
        default:
            break;
        }
    }
    QSqlQuery query;
    if (!query.exec(req)) {
        qDebug() << "postedSizeInMB: cannot execute query: " << query.lastError().text()
                 << " - req: " << query.lastQuery();
        return 0;
    }
    while (query.next())
        megas += megaSize(query.value(0).toString());


    return megas;
}

QString Database::postedSize(DATE_CONDITION since) const
{
    double size = static_cast<double>(postedSizeInMB(since));
    QString unit = "MB";
    if (size > 1024)
    {
        size /= 1024;
        unit = "GB";
    }
    if (size > 1024)
    {
        size /= 1024;
        unit = "TB";
    }
    return QString("%1 %2").arg(size, 0, 'f', 2).arg(unit);
}

int Database::_execSqlFile(const QString &fileName, const QString &separator)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "execSqlFile: cannot open" << fileName;
        return -1;
    }

    QTextStream textStream(&file);
    QString fileString = textStream.readAll();

    // Remove commented out lines to prevent errors.
    fileString.replace(QRegularExpression("--.*?\\n"), " ");

    QStringList stringList = fileString.split(separator, Qt::SkipEmptyParts);
    QSqlQuery query;
    int nbFailed = 0;
    for (const auto &queryString : stringList) {
        QString end = separator;
        if (queryString.isNull() || queryString.trimmed().isEmpty()
                || queryString.trimmed().startsWith("END")) {
            continue;
        }

        if (queryString.trimmed().startsWith("CREATE TRIGGER"))
            end = "; END;";

        if (!query.exec(queryString + end)) {
            qDebug() << "execSqlFile: cannot execute query" << query.lastError().text()
                     << query.lastQuery();
            ++nbFailed;
        }
    }

    return nbFailed;
}

const QRegularExpression Database::kByteSizeRegExp = QRegularExpression("(\\d+\\.\\d{2}) ([kMG]?B)");
qint64 Database::byteSize(const QString &humanSize)
{
    qint64 bytes = 0;
    QRegularExpressionMatch match = kByteSizeRegExp.match(humanSize);
    if (match.hasMatch())
    {
        double size = match.captured(1).toDouble();
        QString unit = match.captured(2);
        if (unit == "GB")
            size *= 1024*1024*1024;
        else if (unit == "MB")
            size *= 1024*1024;
        else if (unit == "kB")
            size *= 1024;
        bytes = static_cast<long>(size);
    }
    return bytes;
}

qint64 Database::megaSize(const QString &humanSize)
{
    qint64 megas = 0;
    QRegularExpressionMatch match = kByteSizeRegExp.match(humanSize);
    if (match.hasMatch())
    {
        double size = match.captured(1).toDouble();
        QString unit = match.captured(2);
        if (unit == "GB")
            size *= 1024;
        else if (unit.isEmpty() || unit == "kB")
            size = 0; // ignore bytes and kB
        megas = static_cast<long>(size);
    }
    return megas;
}

const QString Database::kSizeSelect = "SELECT size FROM tHistory";

const QString Database::kYearCondition = "strftime('%Y',date) = strftime('%Y',date('now'))";
const QString Database::kMonthCondition = "strftime('%Y',date) = strftime('%Y',date('now')) \
AND strftime('%m',date) = strftime('%m',date('now'))";
const QString Database::kWeekCondition = "strftime('%Y',date) = strftime('%Y',date('now')) \
AND strftime('%W',date) = strftime('%W',date('now'))";
const QString Database::kDayCondition = "strftime('%Y',date) = strftime('%Y',date('now')) \
AND strftime('%m',date) = strftime('%m',date('now'))\
AND strftime('%d',date) = strftime('%d',date('now'))";

const QString Database::kInsertStatement = QString("\
insert into tHistory\
    (date, nzbName, size, avgSpeed, archiveName, archivePass, groups, poster, tmpPath, nzbFilePath, done)\
values\
    (:date, :nzbName, :size, :avgSpeed, :archiveName, :archivePass, :groups, :from, :tmpPath, :nzbFilePath, :done);\
");
