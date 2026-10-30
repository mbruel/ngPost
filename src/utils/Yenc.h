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

#ifndef YENC_H
#define YENC_H
#include "PureStaticClass.h"
#include <string>
#include <QByteArray>
#include <QString>
#include <QtGlobal>

struct YencEncryptionContext
{
    const QByteArray &masterKey;
    const QByteArray &salt;
    quint32 segmentIndex;

    YencEncryptionContext(const QByteArray &master,
                          const QByteArray &s, quint32 segment) :
        masterKey(master), salt(s), segmentIndex(segment) {}
};

class Yenc : public PureStaticClass
{
public:
    static qint64 encode(const char data[], qint64 dataSize, uchar encbuffer[], quint32 &crc32);
    static bool encodeArticle(const QByteArray &plaintext, quint32 part, quint32 totalParts,
                              qint64 fileSize, qint64 filePosition, const QByteArray &fileName,
                              const YencEncryptionContext &encryption, QByteArray &article,
                              quint32 &wireCrc32, QString *error = nullptr);

private:
    static quint32 crc32_tab[];
};

#endif // YENC_H
