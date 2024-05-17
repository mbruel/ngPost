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

#include <QMap>
#include <QString>

namespace NgError
{

enum class ERR_CODE : char
{
    NONE = 0,
    COMPLETED_WITH_ERRORS,
    ERR_CONF_FILE,
    ERR_WRONG_ARG,
    ERR_NO_INPUT,
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

QMap<ERR_CODE, QString> const kErrorNames = {
    {ERR_CODE::NONE,                     "NONE"                   },
    { ERR_CODE::COMPLETED_WITH_ERRORS,   "COMPLETED_WITH_ERRORS"  },
    { ERR_CODE::ERR_CONF_FILE,           "ERR_CONF_FILE"          },
    { ERR_CODE::ERR_WRONG_ARG,           "ERR_WRONG_ARG"          },
    { ERR_CODE::ERR_NO_INPUT,            "ERR_NO_INPUT"           },
    { ERR_CODE::ERR_DEL_AUTO,            "ERR_DEL_AUTO"           },
    { ERR_CODE::ERR_AUTO_NO_COMPRESS,    "ERR_AUTO_NO_COMPRESS"   },
    { ERR_CODE::ERR_AUTO_INPUT,          "ERR_AUTO_INPUT"         },
    { ERR_CODE::ERR_MONITOR_NO_COMPRESS, "ERR_MONITOR_NO_COMPRESS"},
    { ERR_CODE::ERR_MONITOR_INPUT,       "ERR_MONITOR_INPUT"      },
    { ERR_CODE::ERR_NB_THREAD,           "ERR_NB_THREAD"          },
    { ERR_CODE::ERR_ARTICLE_SIZE,        "ERR_ARTICLE_SIZE"       },
    { ERR_CODE::ERR_NB_RETRY,            "ERR_NB_RETRY"           },
    { ERR_CODE::ERR_PAR2_PATH,           "ERR_PAR2_PATH"          },
    { ERR_CODE::ERR_PAR2_ARGS,           "ERR_PAR2_ARGS"          },
    { ERR_CODE::ERR_NO_SERVER,           "ERR_NO_SERVER"          },
    { ERR_CODE::ERR_SERVER_REGEX,        "ERR_SERVER_REGEX"       },
    { ERR_CODE::ERR_SERVER_PORT,         "ERR_SERVER_PORT"        },
    { ERR_CODE::ERR_SERVER_CONS,         "ERR_SERVER_CONS"        },
    { ERR_CODE::ERR_INPUT_READ,          "ERR_INPUT_READ"         },
};

} // namespace NgError

#endif // NGERROR_H
