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
#ifndef NgMigration_H
#define NgMigration_H
#include <QCoreApplication>
#include <QString>
class NgPost;

class NgMigration
{
    Q_DECLARE_TR_FUNCTIONS(NgMigration); // tr() without QObject using QCoreApplication::translate

    NgPost &_ngPost;
    bool    _migrationDone;

public:
    NgMigration(NgPost &ngPost);
    ~NgMigration() = default;

    bool migrate();

private:
    void _doNgMigration(unsigned short const confBuild = 1);

    bool _migrateTo500();

    double _sizeInMbFromStr(QString const &sizeStr) const;
    double _avgSpeedInKbpsFromStr(QString const &avgSpeedStr);
};

#endif // NgMigration_H
