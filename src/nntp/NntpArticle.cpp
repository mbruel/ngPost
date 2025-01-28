//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
// This file is a part of ngPost : https://github.com/mbruel/ngPost
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3.
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

#include "NntpArticle.h"
#include "NntpConnection.h"
#include "NgPost.h"
#include "nntp/NntpFile.h"
#include "nntp/Nntp.h"
#include "utils/Yenc.h"
#include <cstring>
#include <sstream>

ushort NntpArticle::sNbMaxTrySending = 5;

NntpArticle::NntpArticle(NntpFile *file, uint part, qint64 pos, qint64 bytes,
                         const std::string *from, bool obfuscation):
    _nntpFile(file), _part(part),
    _id(QUuid::createUuid()),
    _from(from),
    _subject(nullptr),
    _body(nullptr),
    _filePos(pos), _fileBytes(bytes),
    _nbTrySending(0),
    _msgId()
{
    file->addArticle(this);
    connect(this, &NntpArticle::posted, _nntpFile, &NntpFile::onArticlePosted, Qt::QueuedConnection);
    connect(this, &NntpArticle::failed, _nntpFile, &NntpFile::onArticleFailed, Qt::QueuedConnection);

    if (!obfuscation)
    {
        std::stringstream ss;
        ss << _nntpFile->nameWithQuotes().toStdString() << " (" << part << "/" << _nntpFile->nbArticles() << ")";

        std::string subject = ss.str();
        _subject = new char[subject.length()+1];
        strcpy(_subject, subject.c_str());
    }
}

void NntpArticle::yEncBody(const char data[])
{
    // do the yEnc encoding
    quint32 crc32    = 0xFFFFFFFF;
    uchar  *yencBody = new uchar[_fileBytes*2];
    Yenc::encode(data, _fileBytes, yencBody, crc32);

    // format the body
    std::stringstream ss;
    ss << "=ybegin part=" << _part << " total=" << _nntpFile->nbArticles() << " line=128"
       << " size=" << _nntpFile->fileSize() << " name=" << _nntpFile->fileName() << Nntp::ENDLINE
       << "=ypart begin=" << _filePos + 1 << " end=" << _filePos + _fileBytes << Nntp::ENDLINE
       << yencBody << Nntp::ENDLINE
       << "=yend size=" << _fileBytes << " pcrc32=" << std::hex << crc32 << Nntp::ENDLINE
       << "." << Nntp::ENDLINE;

    delete[] yencBody;

    std::string body = ss.str();
    _body = new char[body.length()+1];
    strcpy(_body, body.c_str());
}

NntpArticle::~NntpArticle()
{
    freeMemory();
}


//NntpArticle::NntpArticle(const std::string &from, const std::string &groups, const std::string &subject, const std::string &body):
//    _nntpFile(nullptr), _part(1),
//    _id(QUuid::createUuid()),
//    _from(from), _groups(groups),
//    _subject(subject),
//    _body(nullptr),
//    _filePos(0), _fileBytes(0),
//    _yencBody(nullptr),
//    _yencSize(0),
//    _crc32(0xFFFFFFFF),
//    _nbTrySending(0)
//{

//    _body->operator+=(Nntp::ENDLINE);
//    _body->operator+=(".");
//    _body->operator+=(Nntp::ENDLINE);
//}

QString NntpArticle::str() const
{
    if (_msgId.isEmpty())
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
        _msgId = _id.toString(sMsgIdFormat);
#else
        _msgId = _id.toString();
#endif
    return QString("%5 - Article #%1/%2 <id: %3, nbTrySend: %4>").arg(
                _part).arg(_nntpFile->nbArticles()).arg(_msgId).arg(
                _nbTrySending).arg(_nntpFile->name());
}

bool NntpArticle::tryResend()
{
    if (_nbTrySending < sNbMaxTrySending)
    {
        // get a new UUID
        _id = QUuid::createUuid();
        return true;
    }
    else
        return false;
}

void NntpArticle::write(NntpConnection *con, const std::string &idSignature)
{
    ++_nbTrySending;
    con->write(header(idSignature).c_str());
    con->write(_body);
}

std::string NntpArticle::header(const std::string &idSignature) const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    QByteArray msgId = _id.toByteArray(sMsgIdFormat);
#else
    QByteArray msgId = _id.toByteArray();
#endif
    std::stringstream ss;
    ss << "From: "        << (_from == nullptr ? NgPost::randomStdFrom() : *_from)    << Nntp::ENDLINE
       << "Newsgroups: "  << _nntpFile->groups()  << Nntp::ENDLINE
       << "Subject: "     << (_subject == nullptr ? msgId.constData() : _subject) << Nntp::ENDLINE
       << "Message-ID: <" << msgId.constData() << "@" << idSignature << ">" << Nntp::ENDLINE
       << Nntp::ENDLINE;
    _msgId = QString("%1@%2").arg(msgId.constData()).arg(idSignature.c_str());
    return ss.str();
}

void NntpArticle::dumpToFile(const QString &path, const std::string &articleIdSignature)
{
    QString fileName = QString("%1/%2_%3.yenc").arg(path).arg(_nntpFile->fileName().c_str()).arg(_part);
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
    {
        qDebug() << "[NntpArticle::dumpToFile] error creating file " << fileName;
        return;
    }

    file.write(header(articleIdSignature).c_str());
    file.write(_body);
    file.close();
}
