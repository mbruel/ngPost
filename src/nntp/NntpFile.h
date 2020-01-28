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

#ifndef NntpFile_H
#define NntpFile_H
#include <QObject>
#include <QVector>
#include <QSet>
#include <QFileInfo>
class NntpArticle;
class QTextStream;
class PostingJob;

/*!
 * \brief The NntpFile class represent a File uploaded on Usenet thus it will holds all its Articles
 * All NntpFiles will live (active objects) in the Main Thread so they can write into the nzb without concurrency
 * when all its articles are posted (slot onArticlePosted) it emits allArticlesArePosted
 */
class NntpFile : public QObject
{
    Q_OBJECT

public:
    NntpFile(PostingJob *postingJob, const QFileInfo &file, uint num, uint nbFiles, const QList<QString> &grpList);
    ~NntpFile();

    inline void addArticle(NntpArticle *article);

    void writeToNZB(QTextStream &stream, const char *articleIdSignature);

    inline QString path() const;
    inline QString name() const;
    inline std::string fileName() const;
    inline qint64 fileSize() const;
    inline uint nbArticles() const;
    inline uint nbFailedArticles() const;
    inline bool hasFailedArticles() const;

signals:
    void allArticlesArePosted();
    void startPosting();
    void scheduleDeletion();
    void errorReadingFile();

public slots:
    void onArticlePosted(quint64 size);
    void onArticleFailed(quint64 size);


private:
    PostingJob       *const _postingJob;
    const QFileInfo         _file;      //!< original file
    const uint              _num;       //!< file number
    const uint              _nbFiles;   //!< total number of file
    const QList<QString>   &_grpList;   //!< groups where the file is posted
    const uint              _nbAticles; //!< total number of articles
    QVector<NntpArticle*>   _articles;  //!< all articles (that are yEnc encoded)

    QSet<uint> _posted; //!< part number of the Articles that have been posted (uploaded on the socket)
    QSet<uint> _failed; //!< part number of the Articles that FAILED to be posted (uploaded on the socket)
};

void NntpFile::addArticle(NntpArticle *article) { _articles.push_back(article); }
QString NntpFile::path() const { return _file.absoluteFilePath(); }
QString NntpFile::name() const { return QString("[%1/%2] %3").arg(_num).arg(_nbFiles).arg(_file.fileName()); }
std::string NntpFile::fileName() const { return _file.fileName().toStdString(); }
qint64 NntpFile::fileSize() const { return _file.size(); }
uint NntpFile::nbArticles() const { return _nbAticles; }
uint NntpFile::nbFailedArticles() const { return static_cast<uint>(_failed.size()); }
bool NntpFile::hasFailedArticles() const { return _failed.size() != 0; }

#endif // NntpFile_H
