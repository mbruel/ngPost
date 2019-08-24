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

#ifndef NNTP_H
#define NNTP_H

#include "utils/PureStaticClass.h"
#include <map>
#include <regex>

/*!
 * \brief Pure Static class (no instance) to hold Nntp Protocol actions/responses...
 */
class Nntp : public PureStaticClass
{
public:
    static constexpr const char* QUIT          {"quit\r\n"};
    static constexpr const char* AUTHINFO_USER {"authinfo user "};
    static constexpr const char* AUTHINFO_PASS {"authinfo pass "};
    static constexpr const char* POST          {"post\r\n"};
    static constexpr const char* ENDLINE       {"\r\n"};

    //! return the response associated to a certain code
    static const char* getResponse(unsigned short aCode);

private:
    static const std::map<unsigned short, const char *> sResponses; //!< Responses map
};

#endif // NNTP_H
