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

    static constexpr char const *kDriverSqlite = "QSQLITE";

    static const QString kInsertStatement;

    static const QString kSizeSelect;

    static const QString kYearCondition;
    static const QString kMonthCondition;
    static const QString kWeekCondition;
    static const QString kDayCondition;

    static const QRegularExpression kByteSizeRegExp;

public:
    Database(); //!< for SQLITE
    ~Database();

    bool initSQLite(QString const &dbPath);
    int  insertPost(QString const &date,
                    QString const &nzbName,
                    QString const &size,
                    QString const &avgSpeed,
                    QString const &archiveName,
                    QString const &archivePass,
                    QString const &groups,
                    QString const &from,
                    QString const &tmpPath,
                    QString const &nzbFilePath,
                    int            done);

    qint64  postedSizeInMB(DATE_CONDITION since) const;
    QString postedSize(DATE_CONDITION since) const;

    static qint64 byteSize(QString const &humanSize);
    static qint64 megaSize(QString const &humanSize);

private:
    int _execSqlFile(QString const &fileName, QString const &separator = ";");
};

#endif // DATABASE_H
