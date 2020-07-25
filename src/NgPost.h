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
#include "utils/CmdOrGuiApp.h"
#include "utils/Macros.h"

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
#include <QProcess>
#include <QNetworkAccessManager>
class QTranslator;
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
class NgPost : public QObject, public CmdOrGuiApp
{
    Q_OBJECT

    friend class MainWindow; //!< so it can access all parameters
    friend class PostingWidget;
    friend class AutoPostWidget;
    friend class PostingJob;
    friend class AboutNgPost;

    enum class Opt {HELP = 0, LANG, VERSION, CONF, SHUTDOWN_CMD,
                    DISP_PROGRESS, DEBUG, DEBUG_FULL, POST_HISTORY, FIELD_SEPARATOR, NZB_RM_ACCENTS,
                    RESUME_WAIT, NO_RESUME_AUTO, SOCK_TIMEOUT, PREPARE_PACKING,
                    INPUT, OUTPUT, NZB_PATH, THREAD, NZB_UPLOAD_URL, NZB_POST_CMD,
                    MONITOR_FOLDERS, MONITOR_EXT, MONITOR_IGNORE_DIR,
                    MSG_ID, META, ARTICLE_SIZE, FROM, GROUPS, NB_RETRY, GEN_FROM,
                    OBFUSCATE, INPUT_DIR, AUTO_DIR, MONITOR_DIR, DEL_AUTO,
                    TMP_DIR, RAR_PATH, RAR_EXTRA, RAR_SIZE, RAR_MAX, KEEP_RAR,
                    PAR2_PCT, PAR2_PATH, PAR2_ARGS,
                    COMPRESS, GEN_PAR2, GEN_NAME, GEN_PASS, LENGTH_NAME, LENGTH_PASS,
                    RAR_NAME, RAR_PASS, RAR_NO_ROOT_FOLDER,
                    AUTO_CLOSE_TABS, AUTO_COMPRESS,
                    SERVER, HOST, PORT, SSL, USER, PASS, CONNECTION, ENABLED
                   };

    static const QMap<Opt, QString> sOptionNames;

    enum class AppMode {CMD = 0, HMI = 1}; //!< supposed to be CMD but a simple HMI has been added

    enum class ERROR_CODE : ushort {NONE=0, COMPLETED_WITH_ERRORS,
                                    ERR_CONF_FILE, ERR_WRONG_ARG, ERR_NO_INPUT,
                                    ERR_DEL_AUTO, ERR_AUTO_NO_COMPRESS, ERR_AUTO_INPUT,
                                    ERR_MONITOR_NO_COMPRESS, ERR_MONITOR_INPUT,
                                    ERR_NB_THREAD, ERR_ARTICLE_SIZE, ERR_NB_RETRY, ERR_PAR2_ARGS,
                                    ERR_SERVER_REGEX, ERR_SERVER_PORT, ERR_SERVER_CONS,
                                    ERR_INPUT_READ
                                   };

private:
    mutable QTextStream   _cout; //!< stream for stdout
    mutable QTextStream   _cerr; //!< stream for stderr

    ERROR_CODE            _err;
    ushort                _debug;
    bool                  _dispProgressBar;
    bool                  _dispFilesPosting;

    QString               _nzbName; //!< name of nzb that we'll write (without the extension)
    int                   _nbFiles;  //!< number of files to post in this iteration

    QList<NntpServerParams*> _nntpServers; //!< the servers parameters

    bool                 _obfuscateArticles;  //!< shall we obfuscate each Article (subject + from)
    bool                 _obfuscateFileName;  //!< for single file or folder, rename it with a random name prior to compress it

    bool                 _genFrom;
    bool                 _saveFrom;
    std::string          _from;               //!< email of poster (if empty, random one will be used for each file)
    std::string          _groups;             //!< Newsgroup where to post

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
    QString     _par2PathConfig;

    bool        _doCompress;
    bool        _doPar2;
    bool        _genName;
    bool        _genPass;

    uint        _lengthName;
    uint        _lengthPass;
    QString     _rarName;
    QString     _rarPass;
    QString     _rarPassFixed;

    QString     _inputDir;

    // Thread safe, only main thread is using this (NgPost or HMI)
    PostingJob         *_activeJob;
    QQueue<PostingJob*> _pendingJobs;
    PostingJob         *_packingJob;

    QString     _historyFieldSeparator;
    QString     _postHistoryFile;
    QList<QDir> _autoDirs;

    FoldersMonitorForNewFiles *_folderMonitor;
    QThread                   *_monitorThread;
    bool                       _delAuto;

    bool          _monitor_nzb_folders;
    QStringList   _monitorExtensions;
    bool          _monitorIgnoreDir;

    bool          _keepRar;
    bool          _autoCompress;

    QString       _lang;
    QMap<QString, QTranslator*> _translators;

    QNetworkAccessManager _netMgr;
    QUrl *_urlNzbUpload;
    QString _urlNzbUploadStr;

    bool       _doShutdownWhenDone;
    QProcess  *_shutdownProc;
    QString    _shutdownCmd;

    bool       _removeAccentsOnNzbFileName;
    bool       _autoCloseTabs;
    bool       _rarNoRootFolder;

    bool       _tryResumePostWhenConnectionLost;
    ushort     _waitDurationBeforeAutoResume;

    QString    _nzbPostCmd;
    bool       _preparePacking;

    static constexpr const char *sDefaultShutdownCmdLinux   = "sudo -n /sbin/poweroff";
    static constexpr const char *sDefaultShutdownCmdWindows = "shutdown /s /f /t 0";
    static constexpr const char *sDefaultShutdownCmdMacOS   = "sudo -n shutdown -h now";


    static qint64 sArticleSize;
    static const QString sSpace;


    static const char *sAppName;
    static const QString sVersion;
    static const QString sProFileURL;

    static const QList<QCommandLineOption> sCmdOptions;

    static const int sDefaultResumeWaitInSec     = 30;
    static const int sDefaultNumberOfConnections = 15;
    static const int sDefaultSocketTimeOut       = 30000;
    static const int sMinSocketTimeOut           = 5000;
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

    static const QString sMainThreadName;

    static const uint sDefaultLengthName = 17;
    static const uint sDefaultLengthPass = 13;
    static const uint sDefaultRarMax     = 99;
    static constexpr const char *sDefaultRarExtraOptions = "-ep1 -m0";
    static constexpr const char *sDefault7zOptions = "-mx0 -mhe=on";

    static const char *sFolderMonitoringName;
    static const char *sQuickJobName;

    static const int sNbPreparedArticlePerConnection = NB_ARTICLES_TO_PREPARE_PER_CONNECTION;

    static const char *sDonationTooltip;

    static const char sDefaultFieldSeparator = ';';
    static constexpr const char *sTranslationPath = ":/lang";

    static constexpr const char *sNntpServerStrRegExp = "^(([^:]+):([^@]+)@@@)?([\\w\\.\\-_]+):(\\d+):(\\d+):(no)?ssl$";


    static std::string sArticleIdSignature; //!< signature for Article message id (must be as a email address)
    static const std::string sRandomAlphabet;


public:
    explicit NgPost(int &argc, char *argv[]);
    ~NgPost() override;

    // pure virtual from CmdOrGuiApp
    bool parseCommandLine(int argc, char *argv[]) override;
    inline const char * appName() override;

    void checkForNewVersion() override;
    int startHMI() override;

    inline int errCode() const;


    QString parseDefaultConfig();

    bool startPostingJob(PostingJob *job);

    void updateGroups(const QString &groups);    


    inline int nbThreads() const;
    inline int getSocketTimeout() const;
    inline QString nzbPath() const;
    void setNzbName(const QFileInfo &fileInfo);
    QString nzbPath(const QString &monitorFolder);


    inline bool removeNntpServer(NntpServerParams *server);

    inline QString randomFrom(ushort length = 13) const;
    QString randomPass(uint length = 13) const;


    inline QList<QString> languages() const;

    inline bool isPosting() const;
    inline bool hasPostingJobs() const;
    void closeAllPostingJobs();

    bool hasMonitoringPostingJobs() const;
    void closeAllMonitoringJobs();


    inline bool debugMode() const;
    inline bool debugFull() const;
    inline void setDebug(ushort level);

    inline bool dispPostingFile() const;

    void saveConfig();

    void setDelFilesAfterPosted(bool delFiles);
    void addMonitoringFolder(const QString &dirPath);

    void changeLanguage(const QString &lang);


    void doNzbPostCMD(const QString &nzbFilePath);



    inline std::string from() const;
    inline void setAutoCompress(bool checked);

    inline bool removeRarRootFolder() const;

    bool isPaused() const;
    void pause() const;
    void resume() const;

    inline bool tryResumePostWhenConnectionLost() const;
    inline ushort waitDurationBeforeAutoResume() const;

signals:
    void log(QString msg, bool newline); //!< in case we signal from another thread
    void error(QString msg); //!< in case we signal from another thread

public slots:
    void onCheckForNewVersion();
    void onDonation();
    void onAboutClicked();


    void onPostingJobStarted();
    void onPackingDone();
    void onPostingJobFinished();

    void onShutdownProcReadyReadStandardOutput();
    void onShutdownProcReadyReadStandardError();
    void onShutdownProcFinished(int exitCode);
//    void onShutdownProcStarted();
//    void onShutdownProcStateChanged(QProcess::ProcessState newState);
    void onShutdownProcError(QProcess::ProcessError error);

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    void onNetworkAccessibleChanged(QNetworkAccessManager::NetworkAccessibility accessible);
#endif

private slots:
    void onLog(QString msg, bool newline);
    void onError(QString msg);
    void onErrorConnecting(QString err);
    void onRefreshprogressbarBar();

    void onNewFileToProcess(const QFileInfo &fileInfo);


private:
    void _loadTanslators();

    void _post(const QFileInfo &fileInfo, const QString &monitorFolder = "");
    void _finishPosting();

    void _prepareNextPacking();


    void _startMonitoring(const QString &folderPath);
    void _stopMonitoring();

    void _log(const QString &aMsg, bool newline = true) const; //!< log function for QString
    void _error(const QString &error) const;    
    void _error(const QString &error, ERROR_CODE code);


    void _syntax(char *appName);
    QString _parseConfig(const QString &configPath);

#ifdef __DEBUG__
    void _dumpParams() const;
#endif

    void _showVersionASCII() const;

    inline void _enableAutoCompress();


// Static functions
public:
    inline static const QString &space();

    inline static qint64 articleSize();
    inline static const std::string &aticleSignature();

    inline static const QString & proFileUrl();
    inline static const QString & donationURL();
    inline static const QString & asciiArt();
    inline static QString asciiArtWithVersion();
    inline static QString desc(bool useHTML = false);

    inline static QString quickJobName();
    inline static QString folderMonitoringName();
    inline static QString donationTooltip();

    inline static std::string randomStdFrom(ushort length = 13);

    inline static QStringList parseCombinedArgString(const QString &program);

};

QString NgPost::quickJobName() { return tr(sQuickJobName); }
QString NgPost::folderMonitoringName() { return tr(sFolderMonitoringName); }
QString NgPost::donationTooltip() { return tr(sDonationTooltip); }

std::string NgPost::from() const
{
    if (_genFrom || _from.empty())
        return randomStdFrom();
    else
        return _from;
}

void NgPost::setAutoCompress(bool checked)
{
    _autoCompress = checked;
    _doCompress   = checked;
    _genName      = checked;
    _genPass      = checked;
    _doPar2       = checked;
}

bool NgPost::removeRarRootFolder() const { return _rarNoRootFolder; }
bool NgPost::tryResumePostWhenConnectionLost() const { return _tryResumePostWhenConnectionLost; }
ushort NgPost::waitDurationBeforeAutoResume() const { return _waitDurationBeforeAutoResume; }

const std::string &NgPost::aticleSignature() { return sArticleIdSignature; }
const char *NgPost::appName() { return sAppName; }

int NgPost::errCode() const     { return static_cast<int>(_err); }

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

QList<QString> NgPost::languages() const{ return _translators.keys(); }

bool NgPost::isPosting() const { return _activeJob != nullptr; }
bool NgPost::hasPostingJobs() const { return (_activeJob || _pendingJobs.size()) ? true : false;}

bool NgPost::debugMode() const     { return _debug != 0; }
bool NgPost::debugFull() const     { return _debug == 2; }
void NgPost::setDebug(ushort level){ _debug = level; }

bool NgPost::dispPostingFile() const { return _dispFilesPosting; }


qint64 NgPost::articleSize()  { return sArticleSize; }
const QString &NgPost::space(){ return sSpace; }



QString NgPost::randomFrom(ushort length) const { return QString::fromStdString(randomStdFrom(length));}

std::string NgPost::randomStdFrom(ushort length) {
    size_t nbLetters = sRandomAlphabet.length();
    std::string randomFrom;
    randomFrom.reserve(length+sArticleIdSignature.length()+5);
    for (size_t i = 0 ; i < length ; ++i)
        randomFrom.push_back(sRandomAlphabet.at(std::rand()%nbLetters));
    randomFrom.push_back('@');
    randomFrom.append(sArticleIdSignature);
    randomFrom.append(".com");

    return randomFrom;
}

QStringList NgPost::parseCombinedArgString(const QString &program)
{
    // from Qt old code (https://code.woboq.org/qt5/qtbase/src/corelib/io/qprocess.cpp.html#_ZL22parseCombinedArgStringRK7QString)
    // cf https://forum.qt.io/topic/116527/deprecation-of-qprocess-start-and-execute-without-argument-list
    QStringList args;
    QString tmp;
    int quoteCount = 0;
    bool inQuote = false;
    // handle quoting. tokens can be surrounded by double quotes
    // "hello world". three consecutive double quotes represent
    // the quote character itself.
    for (int i = 0; i < program.size(); ++i) {
        if (program.at(i) == QLatin1Char('"')) {
            ++quoteCount;
            if (quoteCount == 3) {
                // third consecutive quote
                quoteCount = 0;
                tmp += program.at(i);
            }
            continue;
        }
        if (quoteCount) {
            if (quoteCount == 1)
                inQuote = !inQuote;
            quoteCount = 0;
        }
        if (!inQuote && program.at(i).isSpace()) {
            if (!tmp.isEmpty()) {
                args += tmp;
                tmp.clear();
            }
        } else {
            tmp += program.at(i);
        }
    }
    if (!tmp.isEmpty())
        args += tmp;
    return args;
}

void NgPost::_enableAutoCompress()
{
    _autoCompress = true;
    _doCompress   = true;
    _genName      = true;
    _genPass      = true;
    _doPar2       = true;
}


QString NgPost::desc(bool useHTML)
{
    QString desc;
    if (useHTML)
        desc = QString("%1 %2<br/>%3<br/><br/>%4<ul><li>%5</li><li>%6</li><li>%7</li><li>%8</li><li>%9</li><li>%10</li><li>%11</li></ul>%12<br/><br/>%13<br/>");
    else
        desc = QString("%1 %2\n%3\n\n%4\n    - %5\n    - %6\n    - %7\n    - %8\n    - %9\n    - %10\n    - %11\n%12\n\n%13\n");
    return desc.arg(sAppName).arg(
            tr("is a CMD/GUI Usenet binary poster developped in C++11/Qt5:")).arg(
            tr("It is designed to be as fast as possible and offer all the main features to post data easily and safely.")).arg(
            tr("Here are the main features and advantages of ngPost:")).arg(
            tr("compress (using your external rar binary) and generate the par2 before posting!")).arg(
            tr("scan folder(s) and post each file/folder individually after having them compressed")).arg(
            tr("monitor folder(s) to post each new file/folder individually after having them compressed")).arg(
            tr("auto delete files/folders once posted (only in command line with --auto or --monitor)")).arg(
            tr("generate the nzb")).arg(
            tr("invisible mode: full article obfuscation, unique feature making all Articles completely unrecognizable without the nzb")).arg(
            "...").arg(
            tr("for more details, cf %1").arg(
                    useHTML ? "<a href=\"https://github.com/mbruel/ngPost/\">https://github.com/mbruel/ngPost</a>"
                            : "https://github.com/mbruel/ngPost")).arg(
                tr("If you'd like to translate ngPost in your language, it's easy, please contact me at Matthieu.Bruel@gmail.com"));
}

#endif // NGPOST_H
