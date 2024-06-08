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

#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

#include <QCoreApplication>
#include <QDebug> // MB_TODO to be removed"
#include <QFileInfo>
#include <QRegularExpression>

#include "NgLogger.h"

Database::Database() : _type(TYPE::SQLITE), _db(), _isDbInitialized(false) { }

Database::~Database()
{
    if (_db.isOpen())
        _db.close();
    QSqlDatabase::removeDatabase(CONF::DB::kDriver);
}

bool Database::initSQLite(QString const &dbPath, QStringList const &pragmas)
{
    if (!_dbFilePath.isEmpty() && _dbFilePath != dbPath)
        closeDB();

    _dbFilePath = dbPath;
    if (_isDbInitialized)
    {
        qDebug() << "DB already initialized";
        return true;
    }

    // 1.: SQLite driver not available
    _db = QSqlDatabase::addDatabase(CONF::DB::kDriver);
    if (!_db.isValid())
    {
        qDebug() << "[MB_TRACE][Database::initSQLite] return false !_db.isValid;";
        NgLogger::error(tr("Error loading Sqlite driver... ").arg(_db.lastError().text()));
        return false;
    }

    // 2.: open the default pass one
    bool dbAlreadyExists = QFileInfo(dbPath).exists();
    NgLogger::log(
            QString("Database path: %1 , already exist: %2").arg(dbPath).arg(dbAlreadyExists ? "yes" : "no"),
            true);

    // When using the SQLite driver, open() will create the SQLite database if it doesn't exist.
    _db.setDatabaseName(dbPath);
    if (!_db.open())
    {
        NgLogger::error(tr("Cannot open database: ").arg(qPrintable(_db.lastError().text())));
        return false;
    }

    QSqlQuery query;
    for (auto const &pragma : pragmas)
        query.exec(pragma);

    if (!dbAlreadyExists)
    {
        NgLogger::log(tr("Creating database..."), true);
        if (_execSqlFile(CONF::DB::kCreateTables) != 0)
            return false;
    }

    _isDbInitialized = true;
    return true;
}

void Database::closeDB()
{
    _isDbInitialized = false;
    if (!_db.isOpen())
        return;
    _db.close();
}

bool Database::clearTable(QString const &tableName)
{
    if (!_isDbInitialized)
        return true;

    QSqlQuery query;
    query.prepare("delete * FROM :tableName");
    query.bindValue(":tableName", tableName);
    return query.exec();
}

int Database::_execSqlFile(QString const &fileName, QString const &separator)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "[MB_TRACE][Database::_execSqlFile] ERROR " << fileName;
        NgLogger::error(tr("execSqlFile: cannot open file %1").arg(fileName));
        return -1;
    }

    QTextStream textStream(&file);
    QString     fileString = textStream.readAll();

    // Remove commented out lines to prevent errors.
    fileString.replace(QRegularExpression("--.*?\\n"), " ");

    QStringList stringList = fileString.split(separator, Qt::SkipEmptyParts);
    QSqlQuery   query;
    int         nbFailed = 0;
    for (auto const &queryString : stringList)
    {
        QString end = separator;
        if (queryString.isNull() || queryString.trimmed().isEmpty() || queryString.trimmed().startsWith("END"))
        {
            continue;
        }

        if (queryString.trimmed().startsWith("CREATE TRIGGER"))
            end = "; END;";

        if (!query.exec(queryString + end))
        {
            NgLogger::error(tr("Error executing query : %1  (query: %2)")
                                    .arg(query.lastError().text())
                                    .arg(query.lastQuery()));
            ++nbFailed;
        }
    }

    return nbFailed;
}
