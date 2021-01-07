//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
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

#ifndef NNTPSERVERPARAMS_H
#define NNTPSERVERPARAMS_H

#include <QString>

struct NntpServerParams{
    QString     host;
    ushort      port;
    bool        auth;
    std::string user; //!< std::string to be coded 1 byte per char
    std::string pass;
    int         nbCons;
    bool        useSSL;
    bool        enabled;
    bool        nzbCheck;

    static const ushort sDefaultPort = 119;
    static const ushort sDefaultSslPort = 563;


    NntpServerParams():
        host(""), port(sDefaultPort), auth(false), user(""),
        pass(""), nbCons(1), useSSL(false), enabled(true), nzbCheck(false)
    {}

    NntpServerParams(const QString & aHost, ushort aPort = sDefaultPort, bool aAuth = false,
                         const std::string &aUser = "", const std::string &aPass = "",
                         int aNbCons = 1, bool aUseSSL = false, bool aNzbCheck = false):
       host(aHost), port(aPort), auth(aAuth), user(aUser),
       pass(aPass), nbCons(aNbCons), useSSL(aUseSSL), enabled(true), nzbCheck(aNzbCheck)
    {}

    ~NntpServerParams() = default;

    NntpServerParams(const NntpServerParams& o) = default;
    NntpServerParams(NntpServerParams&& aParams) = default;

    inline QString str() const;
};


QString NntpServerParams::str() const
{
    if (auth)
        return QString("[%5con%6 on %1:%2@%3:%4 enabled:%7, nzbCheck:%8]").arg(user.c_str()).arg(pass.c_str()).arg(
                    host).arg(port).arg(nbCons).arg(useSSL?" SSL":"").arg(enabled).arg(nzbCheck);
    else
        return QString("[%3con%4 on %1:%2 enabled:%5, nzbCheck:%6]").arg(host).arg(port).arg(nbCons).arg(
                    useSSL?" SSL":"").arg(enabled).arg(nzbCheck);
}

#endif // NNTPSERVERPARAMS_H
