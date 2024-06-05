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
#ifndef MACROS_H
#define MACROS_H

#include <QtGlobal>

// Helper macros to convert __cplusplus to a string
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
#  define MB_FLUSH flush
#else
#  define MB_FLUSH Qt::flush
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
#  define MB_LoadAtomic(atom) atom.load()
#else
#  define MB_LoadAtomic(atom) atom.loadRelaxed()
#endif

// TO think about macro for _id, _logPrefix
// #define DEC_UINT_ID_PREFIX(className) , uint _id(id)
// ,
//         QString const        _prefixLog(tr("[className %2]") id) setObjctName(##className)
// #if defined(withGetSet) then uint##name() const { return name; }
//                 uint set##name(const uint value) const
// {
//     name = value;
// }
// #endif

#endif // MACROS_H
