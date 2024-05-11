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
#include <QSqlDatabase>

class Database
{
public:
    enum class TYPE : char {SQLITE};
    enum class DATE_CONDITION : char {ALL, YEAR, MONTH, WEEK, DAY};
private:
    TYPE         _type;
    QSqlDatabase _db;

    static constexpr const char *sDriverSqlite = "QSQLITE";

    static const QString sInsertStatement;

    static const QString sSizeSelect;

    static const QString sYearCondition;
    static const QString sMonthCondition;
    static const QString sWeekCondition;
    static const QString sDayCondition;

    static const QRegularExpression sByteSizeRegExp;

public:
    Database(); //!< for SQLITE
    ~Database();

    bool initSQLite(const QString &dbPath);
    int insertPost(QString const& date,
                   QString const& nzbName,
                   QString const& size,
                   QString const& avgSpeed,
                   QString const& archiveName,
                   QString const& archivePass,
                   QString const& groups,
                   QString const& from,
                   QString const& tmpPath,
                   QString const& nzbFilePath,
                   int done);

    qint64 postedSizeInMB(DATE_CONDITION since) const;
    QString postedSize(DATE_CONDITION since) const;

    static qint64 byteSize(const QString &humanSize);
    static qint64 megaSize(const QString &humanSize);


private:
    int _execSqlFile(const QString &fileName, const QString &separator = ";");



};

#endif // DATABASE_H
