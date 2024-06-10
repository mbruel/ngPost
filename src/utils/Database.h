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
#ifndef DATABASE_H
#define DATABASE_H

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QSqlDatabase>

namespace CONF
{
namespace DB
{
inline const QString  kCreateTables = ":/db/tables.sql";
constexpr char const *kDriver       = "QSQLITE";

} // namespace DB
} // namespace CONF

class Database
{
    Q_DECLARE_TR_FUNCTIONS(Database); // tr() without QObject using QCoreApplication::translate

public:
    enum class TYPE : char
    {
        SQLITE
    };
    enum class DATE_CONDITION : char
    {
        ALL,
        YEAR,
        MONTH,
        WEEK,
        DAY
    };

private:
    TYPE         _type;
    QSqlDatabase _db;
    QString      _dbFilePath;
    bool         _isDbInitialized;

public:
    Database(); //!< for SQLITE
    virtual ~Database();

    bool isDbInitialized() const
    {
        return _isDbInitialized; // MB_TODO: could be _db.isOpen() ?
    }

    bool initSQLite(QString const &dbPath, QStringList const &pragmas);
    bool isOpen() const { return _db.isOpen(); }
    void closeDB();
    uint dumpTablesNamesAndRows(); //!< return the number of tables
    bool commit() { return _db.commit(); }
    bool rollback() { return _db.rollback(); }
    bool transaction() { return _db.transaction(); }

    QSqlDatabase const &qSqlDatabase() { return _db; }

protected:
    int  _execSqlFile(QString const &fileName, QString const &separator = ";");
    bool clearTable(QString const &tableName);
};

#endif // DATABASE_H
