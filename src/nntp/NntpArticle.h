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

    friend class NntpFile; //!< to access all members and be able to clear the _body

    static const QUuid::StringFormat sMsgIdFormat = QUuid::StringFormat::Id128;
    static ushort sNbMaxTrySending;

private:
    NntpFile *_nntpFile; //!< original file
    const int _part;     //!< part of the original file
    QUuid     _id;       //!< to generate a unique Message-ID for the Header

    const std::string _from;    //!< NNTP header From
    const std::string _groups;  //!< NNTP header Newsgroups
    const std::string _subject; //!< NNTP header Subject (if defined it won't be obfuscated)

    const qint64 _filePos;   //!< position in the File (for yEnc header)
    const qint64 _fileBytes; //!< bytes of the original file that are encoded

    uchar    *_yencBody; //!< buffer for the yEnc encoding
    qint64    _yencSize; //!< size of the yEnc body (unused)
    quint32   _crc32;    //!< crc of the yEnc body

    std::string _body; //!< full body of the Article with the yEnc header

    ushort _nbTrySending;

signals:
    void posted(quint64 size); //!< to warn the main thread (async upload)
    void failed(quint64 size); //!< to warn the main thread (async upload)

public:
    NntpArticle(NntpFile *file, int part, const char data[], qint64 pos, qint64 bytes,
                const std::string &from, const std::string &groups, bool obfuscation);

    NntpArticle(const std::string &from, const std::string &groups, const std::string &subject,
                const std::string &body);

    ~NntpArticle() = default;

    QString str() const;

    bool tryResend();

    void write(NntpConnection *con, const std::string &idSignature);

    void dumpToFile(const QString &path, const std::string &articleIdSignature);

    std::string header(const std::string &idSignature) const;
    inline std::string body() const;
    inline QString id() const;
    inline NntpFile *nntpFile() const;

    inline bool isFirstArticle() const;

    inline quint64 size() const;

    inline void genNewId();

    inline static ushort nbMaxTrySending();
    inline static void setNbMaxRetry(ushort nbMax);
};

std::string NntpArticle::body() const { return _body; }
QString NntpArticle::id() const { return _id.toString(sMsgIdFormat); }
NntpFile *NntpArticle::nntpFile() const { return _nntpFile; }

bool NntpArticle::isFirstArticle() const { return _part == 1; }

quint64 NntpArticle::size() const { return static_cast<quint64>(_fileBytes); }
void NntpArticle::genNewId() { _id = QUuid::createUuid(); }

ushort NntpArticle::nbMaxTrySending() { return sNbMaxTrySending; }
void NntpArticle::setNbMaxRetry(ushort nbMax) { sNbMaxTrySending = nbMax; }

#endif // NntpArticle_H
