//========================================================================
//
// Copyright (C) 2019 Matthieu Bruel <Matthieu.Bruel@gmail.com>
//
// This file is a part of ngPost : https://github.com/mbruel/ngPost
//
// ngPost is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; version 3.0 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// You should have received a copy of the GNU Lesser General Public
// License along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301,
// USA.
//
//========================================================================

#include "Nntp.h"

const std::map<unsigned short, const char *>  Nntp::sResponses = {
    {0,   "000 UNKNOWN NNTP RESPONSE..."},

    //rfc977: 2.4.3  General Responses
    {200, "200 server ready - posting allowed"},
    {201, "201 server ready - no posting allowed"},

    {400, "400 service discontinued"},
    {500, "500 command not recognized"},
    {501, "501 command syntax error"},
    {502, "502 access restriction or permission denied"},
    {503, "503 program fault - command couldn't perform"},


    //rfc977: 3.2.2  The GROUP command
    {211, "211 <number of articles> <first one> <last one> <name of the group>"},
    {411, "411 no such news group"},


    //rfc977: 3.10.2  The POST command
    {240, "240 article posted ok"},
    {340, "340 send article to be posted. End with <CR-LF>.<CR-LF>"},
    {440, "440 posting not allowed"},
    {441, "441 posting failed"},

    //rfc977: 3.11.2  The QUIT command
    {205, "205 closing connection - goodbye!"},


    //rfcrfc4643: 2.3.1
    {281, "281 Authentication accepted"},
    {380, "380 More Authentication Required"},
    {381, "381 Password required"},
    {480, "480 Authentication Required"},
    {481, "481 Authentication failed/rejected"},
    {482, "482 Authentication commands issued out of sequence"},
};

const char * Nntp::getResponse(unsigned short aCode){
    try {
        return sResponses.at(aCode);
    } catch (const std::out_of_range &ex) {
        return sResponses.at(0);
    }
}

