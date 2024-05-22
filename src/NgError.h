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
#ifndef NGERROR_H
#define NGERROR_H
#include "utils/PureStaticClass.h"

#include <QMap>
#include <QString>

class NgError : public PureStaticClass
{
public:
    enum class ERR_CODE : char
    {
        NONE = 0,
        COMPLETED_WITH_ERRORS,
        ERR_CONF_FILE,
        ERR_WRONG_ARG,
        ERR_NO_INPUT,
        ERR_FILE_NOT_EXIST,
        ERR_XML_PARSING,
        ERR_DEL_AUTO,
        ERR_AUTO_NO_COMPRESS,
        ERR_AUTO_INPUT,
        ERR_MONITOR_NO_COMPRESS,
        ERR_MONITOR_INPUT,
        ERR_NB_THREAD,
        ERR_ARTICLE_SIZE,
        ERR_NB_RETRY,
        ERR_PAR2_PATH,
        ERR_PAR2_ARGS,
        ERR_NO_SERVER,
        ERR_SERVER_REGEX,
        ERR_SERVER_PORT,
        ERR_SERVER_CONS,
        ERR_INPUT_READ
    };

    static ERR_CODE errCode() { return sErrCode; }
    static void     setErrCode(ERR_CODE code) { sErrCode = code; }

    static QString const &errorName(ERR_CODE code)
    {
        auto it = kErrorNames.constFind(code);
        return it == kErrorNames.constEnd() ? sUnknown : it.value();
    }

private:
    inline static ERR_CODE sErrCode = ERR_CODE::NONE;
    inline static QString  sUnknown = QStringLiteral("UNKNOWN");

    inline static QMap<ERR_CODE, QString> const kErrorNames = {
        {ERR_CODE::NONE,                     QStringLiteral("NONE")                   },
        { ERR_CODE::COMPLETED_WITH_ERRORS,   QStringLiteral("COMPLETED_WITH_ERRORS")  },
        { ERR_CODE::ERR_CONF_FILE,           QStringLiteral("ERR_CONF_FILE")          },
        { ERR_CODE::ERR_WRONG_ARG,           QStringLiteral("ERR_WRONG_ARG")          },
        { ERR_CODE::ERR_NO_INPUT,            QStringLiteral("ERR_NO_INPUT")           },
        { ERR_CODE::ERR_FILE_NOT_EXIST,      QStringLiteral("ERR_FILE_NOT_EXIST")     },
        { ERR_CODE::ERR_XML_PARSING,         QStringLiteral("ERR_XML_PARSING")        },
        { ERR_CODE::ERR_DEL_AUTO,            QStringLiteral("ERR_DEL_AUTO")           },
        { ERR_CODE::ERR_AUTO_NO_COMPRESS,    QStringLiteral("ERR_AUTO_NO_COMPRESS")   },
        { ERR_CODE::ERR_AUTO_INPUT,          QStringLiteral("ERR_AUTO_INPUT")         },
        { ERR_CODE::ERR_MONITOR_NO_COMPRESS, QStringLiteral("ERR_MONITOR_NO_COMPRESS")},
        { ERR_CODE::ERR_MONITOR_INPUT,       QStringLiteral("ERR_MONITOR_INPUT")      },
        { ERR_CODE::ERR_NB_THREAD,           QStringLiteral("ERR_NB_THREAD")          },
        { ERR_CODE::ERR_ARTICLE_SIZE,        QStringLiteral("ERR_ARTICLE_SIZE")       },
        { ERR_CODE::ERR_NB_RETRY,            QStringLiteral("ERR_NB_RETRY")           },
        { ERR_CODE::ERR_PAR2_PATH,           QStringLiteral("ERR_PAR2_PATH")          },
        { ERR_CODE::ERR_PAR2_ARGS,           QStringLiteral("ERR_PAR2_ARGS")          },
        { ERR_CODE::ERR_NO_SERVER,           QStringLiteral("ERR_NO_SERVER")          },
        { ERR_CODE::ERR_SERVER_REGEX,        QStringLiteral("ERR_SERVER_REGEX")       },
        { ERR_CODE::ERR_SERVER_PORT,         QStringLiteral("ERR_SERVER_PORT")        },
        { ERR_CODE::ERR_SERVER_CONS,         QStringLiteral("ERR_SERVER_CONS")        },
        { ERR_CODE::ERR_INPUT_READ,          QStringLiteral("ERR_INPUT_READ")         },
    };
};

#endif // NGERROR_H
