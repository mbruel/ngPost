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

#ifndef NGPOST_H
#define NGPOST_H
#include <QSet>
#include <QVector>
#include <QQueue>
#include <QFileInfo>
#include <QTextStream>
#include <QSettings>
#include <QTime>
#include <QMutex>
#include <QQueue>
#include <QCommandLineOption>
class NntpConnection;
class NntpServerParams;
class NntpFile;
class NntpArticle;

#ifdef __DISP_PROGRESS_BAR__
#include <QTimer>
#endif

/*!
 * \brief The NgPost is responsible to manage the posting of all files, know when it is finished and write the nzb
 *
 * 1.: it parses the command line and /or the config file
 * 2.: it creates an NntpFile for each files to post
 * 3.: it creates the upload Threads (at least one)
 * 4.: it creates the NntpConnections and spreads them amongst the upload Threads
 * 5.: it prepares 2 Articles for each NntpConnections (yEnc encoding done)
 * 6.: it updates the progress bar when Articles are posted
 * 7.: it writes NntpFile to the nzb file when they are fully posted
 * 8.: it handles properly the shutdown
 *
 */
class NgPost : public QObject
{
    Q_OBJECT

    enum class Opt {HELP = 0, VERSION, CONF,
                    INPUT, OUTPUT, NZB_PATH, THREAD,
                    REAL_NAME, MSG_ID, META, ARTICLE_SIZE, FROM, GROUPS,
                    HOST, PORT, SSL, USER, PASS, CONNECTION
                   };

    static const QMap<Opt, QString> sOptionNames;

private:
    QString               _nzbName;
    QQueue<NntpFile*>     _filesToUpload;
    QSet<NntpFile*>       _filesInProgress;
    int                   _nbFiles;
    int                   _nbPosted;

    QVector<QThread*>        _threadPool;      //!< the connections are distributed among several threads
    QVector<NntpConnection*> _nntpConnections; //!< the connections

    QList<NntpServerParams*> _nntpServers;

    bool                 _obfuscation;
    std::string          _from;
    std::string          _groups;
    std::string          _articleIdSignature;

    QFile      *_nzb;
    QTextStream _nzbStream;

    QMutex       _mutex;
    NntpFile    *_nntpFile;
    QFile       *_file;
    char        *_buffer;
    int          _part;

    QQueue<NntpArticle*> _articles; //!< all articles that are yenc encoded

    QTime       _timeStart;
    qint64      _uploadSize;

    QMap<QString, QString> _meta;
    QVector<QString>       _grpList;

    int     _nbConnections;
    int     _nbThreads;
    int     _socketTimeOut;
    QString _nzbPath;

#ifdef __DISP_PROGRESS_BAR__
    int                  _nbArticlesUploaded;
    int                  _nbArticlesTotal;
    QTimer               _progressTimer;
    const int            _refreshRate;
#endif


    static qint64 sArticleSize;
    static const QString sSpace;


    static const QString sAppName;
    static const QString sVersion;
    static const QList<QCommandLineOption> sCmdOptions;

    static const int sDefaultNumberOfConnections = 15;
    static const int sDefaultSocketTimeOut       = 5000;
    static const int sDefaultArticleSize         = 716800;
    static constexpr const char *sDefaultGroups  = "alt.binaries.test,alt.binaries.misc";
    static constexpr const char *sDefaultSpace   = "  ";
    static constexpr const char *sDefaultMsgIdSignature = "ngPost";
#ifdef __MINGW64__
    static constexpr const char *sDefaultNzbPath = ""; //!< local folder
    static constexpr const char *sDefaultConfig = "ngPost.conf";
#else
    static constexpr const char *sDefaultNzbPath = "/tmp";
    static constexpr const char *sDefaultConfig = ".ngPost";
#endif

#ifdef __DISP_PROGRESS_BAR__
    static const int sProgressBarWidth = 50;
#endif

public:
    NgPost();
    ~NgPost();

    bool startPosting();

    NntpArticle *getNextArticle();

    bool parseCommandLine(int argc, char *argv[]);

    inline const std::string &aticleSignature() const;

    inline int nbThreads() const;
    inline int getSocketTimeout() const;
    inline QString nzbPath() const;

signals:
    void scheduleNextArticle();


private slots:
    void onNntpFilePosted(NntpFile *nntpFile);
    void onLog(QString msg);
    void onDisconnectedConnection(NntpConnection *con);
    void onPrepareNextArticle();
    void onErrorConnecting(QString err);

#ifndef __USE_MUTEX__
    void onRequestArticle(NntpConnection *con);
#endif

#ifdef __DISP_PROGRESS_BAR__
    void onArticlePosted(NntpArticle *article);
    void onRefreshProgressBar();
#endif


private:
    int  _createNntpConnections();
    void _closeNzb();
    void _printStats() const;
    void _log(const QString &aMsg) const; //!< log function for QString
    void _prepareArticles();


    inline NntpFile *_getNextFile();
    NntpArticle *_getNextArticle();

    NntpArticle *_prepareNextArticle();

    std::string _randomFrom() const;


    void _syntax(char *appName);
    QString _parseConfig(const QString &configPath);

#ifdef __DEBUG__
    void _dumpParams() const;
#endif


public:
    inline static qint64 articleSize();
    inline static const QString &space();
    inline static QString escapeXML(const char *str);
    inline static QString escapeXML(const QString &str);


};

NntpFile *NgPost::_getNextFile()
{
    if (_filesToUpload.size())
    {
        NntpFile *file = _filesToUpload.dequeue();
        _filesInProgress.insert(file);
        return file;
    }
    else
        return nullptr;
}

const std::string &NgPost::aticleSignature() const { return _articleIdSignature; }

int NgPost::nbThreads() const { return _nbThreads; }
int NgPost::getSocketTimeout() const { return _socketTimeOut; }
QString NgPost::nzbPath() const
{
#ifdef __MINGW64__
    if (_nzbPath.isEmpty())
        return QString("%1.nzb").arg(_nzbName);
    else
        return QString("%1\\%2.nzb").arg(_nzbPath).arg(_nzbName);
#else
    return QString("%1/%2.nzb").arg(_nzbPath).arg(_nzbName);
#endif
}


qint64 NgPost::articleSize()  { return sArticleSize; }
const QString &NgPost::space(){ return sSpace; }

QString NgPost::escapeXML(const char *str)
{
    QString escaped(str);
    escaped.replace('&',  "&amp;");
    escaped.replace('<',  "&lt;");
    escaped.replace('>',  "&gt;");
    escaped.replace('"',  "&quot;");
    escaped.replace('\'', "&apos;");
    return escaped;
}

QString NgPost::escapeXML(const QString &str)
{
    QString escaped(str);
    escaped.replace('&',  "&amp;");
    escaped.replace('<',  "&lt;");
    escaped.replace('>',  "&gt;");
    escaped.replace('"',  "&quot;");
    escaped.replace('\'', "&apos;");
    return escaped;
}

#endif // NGPOST_H
