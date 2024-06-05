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
#ifndef NGCONFIGLOADER_H
#define NGCONFIGLOADER_H

#include "PostingParams.h"
#include "utils/PureStaticClass.h"
#include <QCoreApplication>
#include <QStringList>
class NgPost;

class NgConfigLoader : public PureStaticClass
{
    Q_DECLARE_TR_FUNCTIONS(NgConfigLoader); // tr() without QObject using QCoreApplication::translate

public:
    /*!
     * \brief load an NgPost's configuration file (default or not)
     * \param full path of the configuration file
     * \param SharedParams that will be overwritten (previous unset parameters are kept)
     * \return list of errors (empty if all ok)
     */
    static QStringList loadConfig(NgPost &ngPost, QString const &configPath, SharedParams &mainParams);

private:
    inline static bool isBooleanTrue(QString const &val)
    {
        QString valLower = val.toLower();
        return valLower == "true" || valLower == "on" || valLower == "1";
    }
    inline static void setBoolean(bool &param, QString const &val, bool invert = false)
    {
        QString valLower = val.toLower();
        if (valLower == "true" || valLower == "on" || valLower == "1")
            param = invert ? false : true;
        else
            param = invert ? true : false;
    }
    inline static bool
    setUShort(ushort &param, QString const &val, QStringList &errors, const uint lineNumber, QString const &opt)
    {
        bool   ok = false;
        ushort nb = val.toUShort(&ok);
        if (ok)
            param = nb;
        else
            errors << tr("[line #%1] Format error for %1 : %2").arg(lineNumber).arg(opt.toUpper()).arg(val);
        return ok;
    }
    inline static bool
    setUInt(uint &param, QString const &val, QStringList &errors, const uint lineNumber, QString const &opt)
    {
        bool ok = false;
        uint nb = val.toUInt(&ok);
        if (ok)
            param = nb;
        else
            errors << tr("[line #%1] Format error for %1 : %2").arg(lineNumber).arg(opt.toUpper()).arg(val);
        return ok;
    }
    inline static bool
    setUInt64(quint64 &param, QString const &val, QStringList &errors, const uint lineNumber, QString const &opt)
    {
        bool    ok = false;
        quint64 nb = val.toULongLong(&ok);
        if (ok)
            param = nb;
        else
            errors << tr("[line #%1] Format error for %1 : %2").arg(lineNumber).arg(opt.toUpper()).arg(val);
        return ok;
    }

    inline static void addError(QStringList &errList, const uint line, QString const &error)
    {
        errList << QString("[line #%1] %2").arg(line).arg(error);
    }
};

#endif // NGCONFIGLOADER_H
