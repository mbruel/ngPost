//========================================================================
//
// Copyright (C) 2020-2024 Matthieu Bruel <Matthieu.Bruel@gmail.com>
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

#ifndef NNTP_H
#define NNTP_H
#include <map>
#include <regex>
namespace NNTP
{

/*!
 * \brief Pure Static class (no instance) to hold Nntp Protocol actions/responses...
 */
namespace RFC
{

constexpr char const *QUIT{ "quit\r\n" };
constexpr char const *AUTHINFO_USER{ "authinfo user " };
constexpr char const *AUTHINFO_PASS{ "authinfo pass " };
constexpr char const *POST{ "post\r\n" };
constexpr char const *ENDLINE{ "\r\n" };
constexpr char const *STAT{ "stat" };

enum class RespCode : ushort
{
    UNKNOWN = 0,

    //  rfc977: 2.4.3  General Responses
    SERV_READY  = 200,
    SERV_READY1 = 201,

    SERV_DISCON      = 400,
    CMD_NOT_KNOWN    = 500,
    CMD_SYNTAX_ERROR = 501,
    ACCESS_DENIED    = 502,
    SRV_FAULT        = 503,

    //  rfc977: 3.2.2  The GROUP command
    GRP_OK   = 211,
    GRP_NONE = 411,

    //  rfc977: 3.10.2  The POST command
    POST_OK          = 240,
    SEND_ARTICLE     = 340,
    POST_NOT_ALLOWED = 440,
    POST_FAIL        = 441,

    //  rfc977: 6.2.4.  STAT
    ARTICLE_OK   = 223,
    ARTICLE_NONE = 430,

    //  rfc977: 3.11.2  The QUIT command
    CLOSED_CON = 205,

    //  rfc4643: 2.3.1
    AUTH_OK       = 281,
    AUTH_MORE     = 380,
    PASS_NEEDED   = 381,
    AUTH_REQUIRED = 480,
    AUTH_FAIL     = 481,
    AUTH_ISSUE    = 482
};

inline const std::map<RespCode, char const *> kResponses = {
    {RespCode::UNKNOWN,           "000 UNKNOWN NNTP RESPONSE..."                                       },

 //  rfc977: 2.4.3  General Responses
    { RespCode::SERV_READY,       "200 server ready - posting allowed"                                 },
    { RespCode::SERV_READY1,      "201 server ready - no posting allowed"                              },

    { RespCode::SERV_DISCON,      "400 service discontinued"                                           },
    { RespCode::CMD_NOT_KNOWN,    "500 command not recognized"                                         },
    { RespCode::CMD_SYNTAX_ERROR, "501 command syntax error"                                           },
    { RespCode::ACCESS_DENIED,    "502 access restriction or permission denied"                        },
    { RespCode::SRV_FAULT,        "503 program fault - command couldn't perform"                       },

 //  rfc977: 3.2.2  The GROUP command
    { RespCode::GRP_OK,           "211 <number of articles> <first one> <last one> <name of the group>"},
    { RespCode::GRP_NONE,         "411 no such news group"                                             },

 //  rfc977: 3.10.2  The POST command
    { RespCode::POST_OK,          "240 article posted ok"                                              },
    { RespCode::SEND_ARTICLE,     "340 send article to be posted. End with <CR-LF>.<CR-LF>"            },
    { RespCode::POST_NOT_ALLOWED, "440 posting not allowed"                                            },
    { RespCode::POST_FAIL,        "441 posting failed"                                                 },

 //  rfc977: 6.2.4.  STAT
    { RespCode::ARTICLE_OK,       "223 0|n message-id    Article exists"                               },
    { RespCode::ARTICLE_NONE,     "430 No article with that message-id"                                },

 //  rfc977: 3.11.2  The QUIT command
    { RespCode::CLOSED_CON,       "205 closing connection - goodbye!"                                  },

 //  rfc4643: 2.3.1
    { RespCode::AUTH_OK,          "281 Authentication accepted"                                        },
    { RespCode::AUTH_MORE,        "380 More Authentication Required"                                   },
    { RespCode::PASS_NEEDED,      "381 Password required"                                              },
    { RespCode::AUTH_REQUIRED,    "480 Authentication Required"                                        },
    { RespCode::AUTH_FAIL,        "481 Authentication failed/rejected"                                 },
    { RespCode::AUTH_ISSUE,       "482 Authentication commands issued out of sequence"                 },
};

//! return the response associated to a certain code
inline char const *getResponse(RespCode aCode)
{
    try
    {
        return kResponses.at(aCode);
    }
    catch (std::out_of_range const &)
    {
        return kResponses.at(RespCode::UNKNOWN);
    }
}

} // namespace RFC
} // namespace NNTP
#endif // NNTP_H
