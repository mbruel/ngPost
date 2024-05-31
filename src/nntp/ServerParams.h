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
#ifndef SERVERPARAMS_H
#define SERVERPARAMS_H

#include <QString>
namespace NNTP
{

struct ServerParams
{
    QString     host;
    ushort      port;
    bool        auth;
    std::string user; //!< std::string to be coded 1 byte per char
    std::string pass;
    ushort      nbCons;
    bool        useSSL;
    bool        enabled;
    bool        nzbCheck;

    static constexpr ushort kDefaultPort    = 119;
    static constexpr ushort kDefaultSslPort = 563;

    ServerParams()
        : host("")
        , port(kDefaultPort)
        , auth(false)
        , user("")
        , pass("")
        , nbCons(1)
        , useSSL(false)
        , enabled(true)
        , nzbCheck(false)
    {
    }

    ServerParams(QString const     &aHost,
                 ushort             aPort     = kDefaultPort,
                 bool               aAuth     = false,
                 std::string const &aUser     = "",
                 std::string const &aPass     = "",
                 ushort             aNbCons   = 1,
                 bool               aUseSSL   = false,
                 bool               aNzbCheck = false)
        : host(aHost)
        , port(aPort)
        , auth(aAuth)
        , user(aUser)
        , pass(aPass)
        , nbCons(aNbCons)
        , useSSL(aUseSSL)
        , enabled(true)
        , nzbCheck(aNzbCheck)
    {
    }

    ~ServerParams() = default;

    ServerParams(ServerParams const &o)  = default;
    ServerParams(ServerParams &&aParams) = default;

    inline QString str() const;
};

QString ServerParams::str() const
{
    if (auth)
        return QString("[%5con%6 on %1:%2@%3:%4 enabled:%7, nzbCheck:%8]")
                .arg(user.c_str())
                .arg(pass.c_str())
                .arg(host)
                .arg(port)
                .arg(nbCons)
                .arg(useSSL ? " SSL" : "")
                .arg(enabled)
                .arg(nzbCheck);
    else
        return QString("[%3con%4 on %1:%2 enabled:%5, nzbCheck:%6]")
                .arg(host)
                .arg(port)
                .arg(nbCons)
                .arg(useSSL ? " SSL" : "")
                .arg(enabled)
                .arg(nzbCheck);
}

} // namespace NNTP
#endif // SERVERPARAMS_H
