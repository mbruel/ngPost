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
class QCoreApplication;
class MainWindow;

using QAtomicBool = QAtomicInteger<unsigned short>; // 16 bit only (faster than using 8 bit variable...)

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

    friend class MainWindow; //!< so it can access all parameters

    enum class Opt {HELP = 0, VERSION, CONF, DISP_PROGRESS,
                    INPUT, OUTPUT, NZB_PATH, THREAD,
                    MSG_ID, META, ARTICLE_SIZE, FROM, GROUPS, NB_RETRY,
                    OBFUSCATE,
                    HOST, PORT, SSL, USER, PASS, CONNECTION
                   };

    static const QMap<Opt, QString> sOptionNames;

    enum class AppMode {CMD = 0, HMI = 1}; //!< supposed to be CMD but a simple HMI has been added


private:
    QCoreApplication     *_app;  //!< Application instance (either a QCoreApplication or a QApplication in HMI mode)
    const AppMode         _mode; //!< CMD or HMI (for Windowser...)
    MainWindow           *_hmi;  //!< potential HMI

    mutable QTextStream   _cout; //!< stream for stdout
    mutable QTextStream   _cerr; //!< stream for stderr

    bool                  _debug;
    bool                  _dispProgressBar;
    bool                  _dispFilesPosting;

    QString               _nzbName; //!< name of nzb that we'll write (without the extension)
    QQueue<NntpFile*>     _filesToUpload;  //!< list of files to upload (that we didn't start)
    QSet<NntpFile*>       _filesInProgress;//!< files in queue to be uploaded (Articles have been produced)
    int                   _nbFiles;  //!< number of files to post in this iteration
    int                   _nbPosted; //!< number of files posted

    QVector<QThread*>        _threadPool;      //!< the connections are distributed among several threads
    QVector<NntpConnection*> _nntpConnections; //!< the NNTP connections (owning the TCP sockets)

    QList<NntpServerParams*> _nntpServers; //!< the servers parameters

    bool                 _obfuscateArticles;  //!< shall we obfuscate each Article (subject)

    std::string          _from;               //!< email of poster (if empty, random one will be used for each file)
    std::string          _groups;             //!< Newsgroup where to post
    std::string          _articleIdSignature; //!< signature for Article message id (must be as a email address)

    QFile      *_nzb;       //!< nzb file that will be filled on the fly when a file is fully posted
    QTextStream _nzbStream; //!< txt stream for the nzb file

    QMutex       _mutex;    //!< mutex to protect the Article stack (as the NntpConnection will pop from the ThreadPool)
    NntpFile    *_nntpFile; //!< current file that is getting processed
    QFile       *_file;     //!< file handler on the file getting processed
    char        *_buffer;   //!< buffer to read the current file
    int          _part;     //!< part number (Article) on the current file

    QQueue<NntpArticle*> _articles; //!< prepared articles that are yEnc encoded

    QTime       _timeStart; //!< to get some stats (upload time and avg speed)
    quint64     _totalSize; //!< total size (in Bytes) to be uploaded

    QMap<QString, QString> _meta;    //!< list of meta to add in the nzb header (typically a password)
    QVector<QString>       _grpList; //!< Newsgroup where we're posting in a list format to write in the nzb file

    int     _nbConnections; //!< available number of NntpConnection (we may loose some)
    int     _nbThreads;     //!< size of the ThreadPool
    int     _socketTimeOut; //!< socket timeout
    QString _nzbPath;       //!< default path where to write the nzb files

#ifdef __DISP_PROGRESS_BAR__
    int       _nbArticlesUploaded; //!< number of Articles that have been uploaded (+ failed ones)
    int       _nbArticlesFailed;   //!< number of Articles that failed to be uploaded
    quint64   _uploadedSize;       //!< bytes posted (to compute the avg speed)
    int       _nbArticlesTotal;    //!< number of Articles of all the files to post
    QTimer    _progressTimer;      //!< timer to refresh the upload information (progress bar, avg. speed)
    const int _refreshRate;        //!< refresh rate
#endif

    QAtomicBool _stopPosting;
    bool        _noMoreFiles;
    bool        _noMoreArticles;


    static qint64 sArticleSize;
    static const QString sSpace;


    static const QString sAppName;
    static const QString sVersion;
    static const QString sProFileURL;

    static const QList<QCommandLineOption> sCmdOptions;

    static const int sDefaultNumberOfConnections = 15;
    static const int sDefaultSocketTimeOut       = 5000;
    static const int sDefaultArticleSize         = 716800;
    static constexpr const char *sDefaultGroups  = "alt.binaries.test,alt.binaries.misc";
    static constexpr const char *sDefaultSpace   = "  ";
    static constexpr const char *sDefaultMsgIdSignature = "ngPost";
#if defined(WIN32) || defined(__MINGW64__)
    static constexpr const char *sDefaultNzbPath = ""; //!< local folder
    static constexpr const char *sDefaultConfig = "ngPost.conf";
#else
    static constexpr const char *sDefaultNzbPath = "/tmp";
    static constexpr const char *sDefaultConfig = ".ngPost";
#endif

#ifdef __DISP_PROGRESS_BAR__
    static const int sProgressBarWidth = 50;
    static const int sDefaultRefreshRate = 500; //!< how often shall we refresh the progress bar?
#endif

public:
    NgPost(int &argc, char *argv[]);
    ~NgPost();

    int startEventLoop();
    inline bool useHMI() const;

    int startHMI();
    QString parseDefaultConfig();


    bool startPosting();

    inline QString avgSpeed() const;

    NntpArticle *getNextArticle();

    bool parseCommandLine(int argc, char *argv[]);

    inline const std::string &aticleSignature() const;

    inline int nbThreads() const;
    inline int getSocketTimeout() const;
    inline QString nzbPath() const;

    inline bool removeNntpServer(NntpServerParams *server);

    QString randomFrom() const;
    QString randomPass(uint length = 13) const;

    inline static const QString & proFileUrl();

    inline bool debugMode() const;
    inline void setDebug(bool isDebug);

    inline bool dispPostingFile() const;



signals:
    void scheduleNextArticle();
    void log(QString msg); //!< in case we signal from another thread

public slots:
    void onCheckForNewVersion();

private slots:
    void onNntpFileStartPosting();
    void onNntpFilePosted();
    void onLog(QString msg);
    void onError(QString msg);
    void onDisconnectedConnection(NntpConnection *con);
    void onPrepareNextArticle();
    void onErrorConnecting(QString err);

#ifndef __USE_MUTEX__
    void onRequestArticle(NntpConnection *con);
#endif

#ifdef __DISP_PROGRESS_BAR__
    void onArticlePosted(quint64 size);
    void onArticleFailed(quint64 size);
    void onRefreshProgressBar();
#endif


private:
    void _initPosting(const QList<QFileInfo> &filesToUpload);
    void _cleanInit();
    void _finishPosting();

    void stopPosting(); //!< for HMI


    int  _createNntpConnections();
    void _closeNzb();
    void _printStats() const;
    void _log(const QString &aMsg) const; //!< log function for QString
    void _error(const QString &error) const;
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

bool NgPost::useHMI() const { return _mode == AppMode::HMI; }

QString NgPost::avgSpeed() const
{
    QString power = " ";
    double bandwidth = 0.;

    if (!_timeStart.isNull())
    {
        double sec = _timeStart.elapsed()/1000.;
        bandwidth = _uploadedSize / sec;

        if (bandwidth > 1024)
        {
            bandwidth /= 1024;
            power = "k";
        }
        if (bandwidth > 1024)
        {
            bandwidth /= 1024;
            power = "M";
        }
    }

    return QString("%1 %2B/s").arg(bandwidth, 6, 'f', 2).arg(power);
}

const std::string &NgPost::aticleSignature() const { return _articleIdSignature; }

int NgPost::nbThreads() const { return _nbThreads; }
int NgPost::getSocketTimeout() const { return _socketTimeOut; }
QString NgPost::nzbPath() const
{
#if defined(WIN32) || defined(__MINGW64__)
    if (_nzbPath.isEmpty())
        return QString("%1.nzb").arg(_nzbName);
    else
        return QString("%1\\%2.nzb").arg(_nzbPath).arg(_nzbName);
#else
    return QString("%1/%2.nzb").arg(_nzbPath).arg(_nzbName);
#endif
}

bool NgPost::removeNntpServer(NntpServerParams *server){ return _nntpServers.removeOne(server); }

const QString &NgPost::proFileUrl() { return sProFileURL; }

bool NgPost::debugMode() const { return _debug; }
void NgPost::setDebug(bool isDebug){ _debug = isDebug; }

bool NgPost::dispPostingFile() const { return _dispFilesPosting; }


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
