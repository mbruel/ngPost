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
#ifndef RESUMEJOBQUEUE_H
#define RESUMEJOBQUEUE_H

#include "utils/PureStaticClass.h"
#include <QStringList>
#include <QtGlobal>

class ResumeJobQueue : public PureStaticClass
{
public:
    static QStringList postedFilesFromNzb(QString const &nzbPath);
};

#endif // RESUMEJOBQUEUE_H
