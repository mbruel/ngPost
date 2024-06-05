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

#ifndef NntpArticle_H
#define NntpArticle_H
#include <QObject>
#include <QtGlobal>
#include <QUuid>
class NntpConnection;

namespace NNTP
{
class File;

/*!
 * \brief The NNTP::Article is an Active Object so it can emit the sigPosted signal when it has been uploaded
 * the NntpConnection will emit this signal from an upload thread
 * it will be received on the main thread by the File
 */
class Article : public QObject
{
    Q_OBJECT

    friend class File; //!< to access all members

signals:
    void sigPosted(quint64 size); //!< to warn the main thread (async upload)
    void sigFailed(quint64 size); //!< to warn the main thread (async upload)

private:
    QUuid      _id;       //!< to generate a unique Message-ID for the Header
    File      *_nntpFile; //!< original file
    const uint _part;     //!< part of the original file

    std::string const *_from;    //!< NNTP header From (owned by PostingJob)
    char              *_subject; //!< NNTP header Subject (if defined it won't be obfuscated)
    char              *_body;    //!< full body of the Article with the yEnc header

    const qint64 _filePos;   //!< position in the File (for yEnc header)
    const qint64 _fileBytes; //!< bytes of the original file that are encoded

    ushort _nbTrySending;

    mutable QString _msgId; //!< using the UUID but the server could rewrite it...

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    static const QUuid::StringFormat kMsgIdFormat = QUuid::StringFormat::Id128;
#endif
    static ushort sNbMaxTrySending;

public:
    Article(File *file, uint part, qint64 pos, qint64 bytes, std::string const *from, bool obfuscation);

    void yEncBody(char const data[]);

    //    Article(const std::string &from, const std::string &groups, const std::string &subject,
    //                const std::string &body);

    ~Article();

#ifdef __test_ngPost__
    static std::string testPostingStdString(char const *msg = "NgPost testing POST allowance");
#endif

    QString str() const;

    bool tryResend();
#ifdef __RELEASE_ARTICLES_WHEN_CON_FAILS__
    inline void resetNbTrySending();
#endif

    void        write(NntpConnection *con, std::string const &idSignature);
    inline void freeMemory();

    void dumpToFile(QString const &path, std::string const &articleIdSignature);

    std::string        header(std::string const &idSignature) const;
    inline std::string body() const;
    inline QString     id() const;
    inline uint        part() const;
    inline File       *nntpFile() const;

    inline bool isFirstArticle() const;

    inline quint64 size() const;

    inline void genNewId();

    inline void overwriteMsgId(QString const &serverMsgID);

    inline static ushort nbMaxTrySending();
    inline static void   setNbMaxRetry(ushort nbMax);
};

#ifdef __RELEASE_ARTICLES_WHEN_CON_FAILS__
void Article::resetNbTrySending() { _nbTrySending = 0; }
#endif

void Article::freeMemory()
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

std::string Article::body() const { return _body; }

QString Article::id() const { return _msgId; }
uint    Article::part() const { return _part; }
File   *Article::nntpFile() const { return _nntpFile; }

bool Article::isFirstArticle() const { return _part == 1; }

quint64 Article::size() const { return static_cast<quint64>(_fileBytes); }
void    Article::genNewId() { _id = QUuid::createUuid(); }

void Article::overwriteMsgId(QString const &serverMsgID) { _msgId = serverMsgID; }

ushort Article::nbMaxTrySending() { return sNbMaxTrySending; }
void   Article::setNbMaxRetry(ushort nbMax) { sNbMaxTrySending = nbMax; }

} // namespace NNTP
#endif // NntpArticle_H
