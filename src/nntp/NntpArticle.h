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

#ifndef NntpArticle_H
#define NntpArticle_H
#include <QtGlobal>
#include <QUuid>
#include <QObject>
class NntpFile;
class NntpConnection;


/*!
 * \brief The NntpArticle is an Active Object so it can emit the posted signal when it has been uploaded
 * the NntpConnection will emit this signal from an upload thread
 * it will be received on the main thread by the NntpFile
 */
class NntpArticle : public QObject
{
    Q_OBJECT

    friend class NntpFile; //!< to access all members

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    static const QUuid::StringFormat sMsgIdFormat = QUuid::StringFormat::Id128;
#endif
    static ushort sNbMaxTrySending;

private:
    NntpFile  *_nntpFile; //!< original file
    const uint _part;     //!< part of the original file
    QUuid      _id;       //!< to generate a unique Message-ID for the Header

    const std::string *_from;    //!< NNTP header From (owned by PostingJob)
    char *_subject;              //!< NNTP header Subject (if defined it won't be obfuscated)
    char *_body;                 //!< full body of the Article with the yEnc header

    const qint64 _filePos;   //!< position in the File (for yEnc header)
    const qint64 _fileBytes; //!< bytes of the original file that are encoded
    const bool _obfuscateArticles;

    ushort _nbTrySending;

    mutable QString _msgId; //!< using the UUID but the server could rewrite it...

signals:
    void posted(quint64 size); //!< to warn the main thread (async upload)
    void failed(quint64 size); //!< to warn the main thread (async upload)

public:
    NntpArticle(NntpFile *file, uint part, qint64 pos, qint64 bytes,
                const std::string *from, bool obfuscation);

    void yEncBody(const char data[]);

//    NntpArticle(const std::string &from, const std::string &groups, const std::string &subject,
//                const std::string &body);

    ~NntpArticle();

    QString str() const;

    bool tryResend();
#ifdef __RELEASE_ARTICLES_WHEN_CON_FAILS__
    inline void resetNbTrySending();
#endif

    void write(NntpConnection *con, const std::string &idSignature);
    inline void freeMemory();

    void dumpToFile(const QString &path, const std::string &articleIdSignature);

    std::string header(const std::string &idSignature) const;
    inline std::string body() const;
    inline QString id() const;
    inline uint part() const;
    inline NntpFile *nntpFile() const;

    inline bool isFirstArticle() const;

    inline quint64 size() const;

    inline void genNewId();

    inline void overwriteMsgId(const QString &serverMsgID);

    inline static ushort nbMaxTrySending();
    inline static void setNbMaxRetry(ushort nbMax);

};

#ifdef __RELEASE_ARTICLES_WHEN_CON_FAILS__
void NntpArticle::resetNbTrySending() { _nbTrySending = 0; }
#endif

void NntpArticle::freeMemory()
{
    if (_subject)
    {
        delete _subject;
        _subject = nullptr;
    }
    if (_body)
    {
        delete[] _body;
        _body = nullptr;
    }
}

std::string NntpArticle::body() const { return _body; }

QString NntpArticle::id() const { return _msgId; }
uint NntpArticle::part() const{ return _part; }
NntpFile *NntpArticle::nntpFile() const { return _nntpFile; }

bool NntpArticle::isFirstArticle() const { return _part == 1; }

quint64 NntpArticle::size() const { return static_cast<quint64>(_fileBytes); }
void NntpArticle::genNewId() { _id = QUuid::createUuid(); }

void NntpArticle::overwriteMsgId(const QString &serverMsgID){ _msgId = serverMsgID; }

ushort NntpArticle::nbMaxTrySending() { return sNbMaxTrySending; }
void NntpArticle::setNbMaxRetry(ushort nbMax) { sNbMaxTrySending = nbMax; }

#endif // NntpArticle_H
