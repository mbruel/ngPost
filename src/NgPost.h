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
class PostingJob;
class FoldersMonitorForNewFiles;

#define NB_ARTICLES_TO_PREPARE_PER_CONNECTION 3

//using QAtomicBool = QAtomicInteger<unsigned short>; // 16 bit only (faster than using 8 bit variable...)

#include <QTimer>

/*!
 * \brief The NgPost is responsible to manage the posting of all files, know when it is finished and write the nzb
 *
 * 1.: it parses the command line and /or the config file
 * 2.: it creates an NntpFile for each files to post
 * 3.: it creates the upload Threads (at least one)
 * 4.: it creates the NntpConnections and spreads them amongst the upload Threads
 * 5.: it prepares 2 Articles for each NntpConnections (yEnc encoding done)
 * 6.: it updates the progressbar bar when Articles are posted
 * 7.: it writes NntpFile to the nzb file when they are fully posted
 * 8.: it handles properly the shutdown
 *
 */
class NgPost : public QObject
{
    Q_OBJECT

    friend class MainWindow; //!< so it can access all parameters
    friend class PostingWidget;
    friend class AutoPostWidget;
    friend class PostingJob;

    enum class Opt {HELP = 0, VERSION, CONF, DISP_PROGRESS, DEBUG, POST_HISTORY,
                    INPUT, OUTPUT, NZB_PATH, THREAD,
                    MONITOR_FOLDERS, MONITOR_EXT, MONITOR_IGNORE_DIR,
                    MSG_ID, META, ARTICLE_SIZE, FROM, GROUPS, NB_RETRY,
                    OBFUSCATE, INPUT_DIR, AUTO_DIR, MONITOR_DIR, DEL_AUTO,
                    TMP_DIR, RAR_PATH, RAR_EXTRA, RAR_SIZE, RAR_MAX,
                    PAR2_PCT, PAR2_PATH, PAR2_ARGS,
                    COMPRESS, GEN_PAR2, GEN_NAME, GEN_PASS, LENGTH_NAME, LENGTH_PASS,
                    RAR_NAME, RAR_PASS,
                    HOST, PORT, SSL, USER, PASS, CONNECTION,
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
    bool                  _dispprogressbarBar;
    bool                  _dispFilesPosting;

    QString               _nzbName; //!< name of nzb that we'll write (without the extension)
    int                   _nbFiles;  //!< number of files to post in this iteration

    QList<NntpServerParams*> _nntpServers; //!< the servers parameters

    bool                 _obfuscateArticles;  //!< shall we obfuscate each Article (subject)

    std::string          _from;               //!< email of poster (if empty, random one will be used for each file)
    std::string          _groups;             //!< Newsgroup where to post
    std::string          _articleIdSignature; //!< signature for Article message id (must be as a email address)

    QMap<QString, QString> _meta;    //!< list of meta to add in the nzb header (typically a password)
    QList<QString>         _grpList; //!< Newsgroup where we're posting in a list format to write in the nzb file

    int     _nbThreads;     //!< size of the ThreadPool
    int     _socketTimeOut; //!< socket timeout
    QString _nzbPath;       //!< default path where to write the nzb files
    QString _nzbPathConf;       //!< default path where to write the nzb files

    QTimer    _progressbarTimer;      //!< timer to refresh the upload information (progressbar bar, avg. speed)
    const int _refreshRate;        //!< refresh rate

    QString     _tmpPath;
    QString     _rarPath;
    QString     _rarArgs;
    uint        _rarSize;
    uint        _rarMax;
    bool        _useRarMax;
    uint        _par2Pct;
    QString     _par2Path;
    QString     _par2Args;

    bool        _doCompress;
    bool        _doPar2;
    bool        _genName;
    bool        _genPass;

    uint        _lengthName;
    uint        _lengthPass;
    QString     _rarName;
    QString     _rarPass;

    QString     _inputDir;

    // Thread safe, only main thread is using this (NgPost or HMI)
    PostingJob         *_activeJob;
    QQueue<PostingJob*> _pendingJobs;

    QString     _postHistoryFile;
    QList<QDir> _autoDirs;

    FoldersMonitorForNewFiles *_folderMonitor;
    QThread                   *_monitorThread;
    bool                       _delAuto;

    bool                       _monitor_nzb_folders;
    QStringList                _monitorExtensions;
    bool                       _monitorIgnoreDir;


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

    static const int sprogressbarBarWidth = 50;
    static const int sDefaultRefreshRate = 500; //!< how often shall we refresh the progressbar bar?

    static const QString sDonationURL;
    static const QString sNgPostASCII;
    static const QString sNgPostDesc;

    static const QString sMainThreadName;

    static const uint sDefaultLengthName = 17;
    static const uint sDefaultLengthPass = 13;
    static const uint sDefaultRarMax     = 99;
    static constexpr const char *sDefaultRarExtraOptions = "-ep1 -m0";
    static constexpr const char *sDefault7zOptions = "-mx0 -mhe=on";

    static constexpr const char *sFolderMonitoringName = "Auto Posting";
    static constexpr const char *sQuickJobName = "Quick Post";

    static const int sNbPreparedArticlePerConnection = NB_ARTICLES_TO_PREPARE_PER_CONNECTION;

    static constexpr const char *sDonationTooltip = "Donations are welcome, I spent quite some time to develop this app and make a sexy GUI although I'm not using it ;)";

    static const char sHistoryLogFieldSeparator = ';';

public:
    NgPost(int &argc, char *argv[]);
    ~NgPost();

    int startEventLoop();
    inline bool useHMI() const;

    int startHMI();
    QString parseDefaultConfig();

    bool startPostingJob(PostingJob *job);

    void updateGroups(const QString &groups);

    bool parseCommandLine(int argc, char *argv[]);

    inline const std::string &aticleSignature() const;

    inline int nbThreads() const;
    inline int getSocketTimeout() const;
    inline QString nzbPath() const;
    void setNzbName(const QFileInfo &fileInfo);
    QString nzbPath(const QString &monitorFolder);


    inline bool removeNntpServer(NntpServerParams *server);

    inline QString randomFrom(ushort length = 13) const;
    QString randomPass(uint length = 13) const;

    inline static const QString & proFileUrl();
    inline static const QString & donationURL();
    inline static const QString & asciiArt();
    inline static QString asciiArtWithVersion();
    inline static const QString & desc();

    inline bool hasPostingJobs() const;
    void closeAllPostingJobs();

    bool hasMonitoringPostingJobs() const;
    void closeAllMonitoringJobs();


    inline bool debugMode() const;
    inline void setDebug(bool isDebug);

    inline bool dispPostingFile() const;

    void saveConfig();

    void setDelFilesAfterPosted(bool delFiles);
    void addMonitoringFolder(const QString &dirPath);

signals:
    void log(QString msg, bool newline); //!< in case we signal from another thread
    void error(QString msg); //!< in case we signal from another thread

public slots:
    void onCheckForNewVersion();
    void onDonation();
    void onAboutClicked();


    void onPostingJobStarted();
    void onPostingJobFinished();

private slots:
    void onLog(QString msg, bool newline);
    void onError(QString msg);
    void onErrorConnecting(QString err);
    void onRefreshprogressbarBar();

    void onNewFileToProcess(const QFileInfo &fileInfo);



private:
    void _post(const QFileInfo &fileInfo, const QString &monitorFolder = "");
    void _finishPosting();

    void _startMonitoring(const QString &folderPath);
    void _stopMonitoring();

    void _log(const QString &aMsg, bool newline = true) const; //!< log function for QString
    void _error(const QString &error) const;

    inline std::string _randomFrom(ushort length = 13) const;


    void _syntax(char *appName);
    QString _parseConfig(const QString &configPath);

#ifdef __DEBUG__
    void _dumpParams() const;
#endif

    void _showVersionASCII() const;


public:
    inline static qint64 articleSize();
    inline static const QString &space();
    inline static QString escapeXML(const char *str);
    inline static QString escapeXML(const QString &str);
    inline static QString xml2txt(const char *str);
    inline static QString xml2txt(const QString &str);


};

bool NgPost::useHMI() const { return _mode == AppMode::HMI; }

const std::string &NgPost::aticleSignature() const { return _articleIdSignature; }

int NgPost::nbThreads() const { return _nbThreads; }
int NgPost::getSocketTimeout() const { return _socketTimeOut; }
QString NgPost::nzbPath() const
{
#if defined(WIN32) || defined(__MINGW64__)
    if (_nzbPath.isEmpty())
        return _nzbName;
    else
        return QString("%1\\%2").arg(_nzbPath).arg(_nzbName);
#else
    return QString("%1/%2").arg(_nzbPath).arg(_nzbName);
#endif
}

bool NgPost::removeNntpServer(NntpServerParams *server){ return _nntpServers.removeOne(server); }

const QString &NgPost::proFileUrl() { return sProFileURL; }
const QString &NgPost::donationURL(){ return sDonationURL; }
const QString &NgPost::asciiArt()   { return sNgPostASCII; }

QString NgPost::asciiArtWithVersion()
{
    return QString("%1                          v%2\n").arg(sNgPostASCII).arg(sVersion);
}

const QString &NgPost::desc() { return sNgPostDesc; }

bool NgPost::hasPostingJobs() const { return (_activeJob || _pendingJobs.size()) ? true : false;}

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

QString NgPost::xml2txt(const char *str)
{
    QString escaped(str);
    escaped.replace("&amp;",  "&");
    escaped.replace("&lt;",   "<");
    escaped.replace("&gt;",   ">");
    escaped.replace("&quot;", "\"");
    escaped.replace("&apos;", "'");
    return escaped;
}

QString NgPost::xml2txt(const QString &str)
{
    QString escaped(str);
    escaped.replace("&amp;",  "&");
    escaped.replace("&lt;",   "<");
    escaped.replace("&gt;",   ">");
    escaped.replace("&quot;", "\"");
    escaped.replace("&apos;", "'");
    return escaped;
}

QString NgPost::randomFrom(ushort length) const { return QString::fromStdString(_randomFrom(length));}

std::string NgPost::_randomFrom(ushort length) const {
    QString randomFrom, alphabet("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    int nbLetters = alphabet.length();
    for (ushort i = 0 ; i < length ; ++i)
        randomFrom.append(alphabet.at(std::rand()%nbLetters));

    randomFrom += QString("@%1.com").arg(_articleIdSignature.c_str());
    return randomFrom.toStdString();
}

#endif // NGPOST_H
