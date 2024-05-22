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
#ifndef NGPOST_H
#define NGPOST_H

#include "utils/CmdOrGuiApp.h"

#include <QCommandLineOption>
#include <QFileInfo>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QProcess>
#include <QQueue>
#include <QSet>
#include <QSettings>
#include <QTextStream>
#include <QTime>
#include <QVector>

class QTranslator;
class NntpConnection;
struct NntpServerParams;
class NntpFile;
class NntpArticle;
class QCoreApplication;
class MainWindow;
class PostingJob;
class FoldersMonitorForNewFiles;
class NzbCheck;
class Database;

#include "NgConf.h"
#include "NgError.h"
#include "PostingParams.h"
#include "utils/NgLogger.h"

#include <QTimer>

/*!
 * \brief The NgPost is responsible to manage the posting of all files, know when it is finished and write the
 * nzb
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

private:
    enum class AppMode
    {
        CMD = 0,
        HMI = 1
    }; //!< supposed to be CMD but a simple HMI has been added

    /*!
     * \brief all posting parameters loaded from conf / command line / or GUI
     * QSharedData => copy on write only for the PostingJobs that would need ;)
     * (in practice, would happen with nice QML GUI where each PostingJobs could edit its parameters
     *  this will probably never implemented... Who knows!?!
     */
    SharedParams _postingParams;

    /*!
     * \brief instantiate and use in command line only (kOptionNames[Opt::CHECK])
     *  check nzb file (if articles are available on Usenet)
     *  cf independant project: https://github.com/mbruel/nzbCheck
     *  As an attribute (cause return to main.cpp its exit code ;))
     */
    NzbCheck *_nzbCheck;

    bool _dispProgressBar;
    bool _dispFilesPosting;

    QTimer    _progressbarTimer; //!< timer to refresh the upload information (progressbar bar, avg. speed)
    int const _refreshRate;      //!< refresh rate

    // Thread safe, only main thread is using this (NgPost or HMI)
    PostingJob          *_activeJob;
    QQueue<PostingJob *> _pendingJobs;
    PostingJob          *_packingJob;

    QString _historyFieldSeparator; //!< deprecated (before 4.17)
    QString _postHistoryFile;       //!< deprecated (before 4.17)
    QString _dbHistoryFile;

    FoldersMonitorForNewFiles *_folderMonitor;
    QThread                   *_monitorThread;

    QString                      _lang;
    QMap<QString, QTranslator *> _translators;

    QNetworkAccessManager _netMgr;

    bool      _doShutdownWhenDone;
    QProcess *_shutdownProc;
    QString   _shutdownCmd;

    QNetworkProxy _proxySocks5;
    QString       _proxyUrl;

    Database *_dbHistory;

#ifdef __USE_HMI__
    bool _isNightMode = false;
#endif

    static constexpr char const *kTranslationPath = ":/lang";

    static constexpr char const *kProxyStrRegExp = "^(([^:]+):([^@]+)@)?([\\w\\.\\-_]+):(\\d+)$";

#ifdef __COMPUTE_IMMEDIATE_SPEED__
    static constexpr int kImmediateSpeedDurationMs = 3000;
#endif

public:
    explicit NgPost(int &argc, char *argv[]);
    ~NgPost() override;

    // pure virtual from CmdOrGuiApp
    bool parseCommandLine(int argc, char *argv[]) override;

    inline char const *appName() override { return NgConf::kAppName; }

    void checkForNewVersion() override;
    bool checkSupportSSL();
#ifdef __USE_HMI__
    int startHMI() override;
#endif

    bool quietMode() const { return _postingParams->quietMode(); }

    void beQuiet()
    {
        NgLogger::setDebug(NgLogger::DebugLevel::None);
        _dispProgressBar  = false;
        _dispFilesPosting = false;
    }

    bool initHistoryDatabase();

    QStringList parseDefaultConfig();

    bool startPostingJob(PostingJob *job);
    bool startPostingJob(QString const                &rarName,
                         QString const                &rarPass,
                         QString const                &nzbFilePath,
                         QFileInfoList const          &files,
                         std::string const            &from,
                         QMap<QString, QString> const &meta);

    QString const &proxyUrl() const { return _proxyUrl; }
    QString const &lang() const { return _lang; }

    inline int nbThreads() const { return _postingParams->nbThreads(); }
    inline int getSocketTimeout() const { return _postingParams->getSocketTimeout(); }
    //    inline QString nzbPath() const;
    QString getNzbName(QFileInfo const &fileInfo) const;
    QString getNzbPath(QString const &monitorFolder);

    inline bool removeNntpServer(NntpServerParams *server);

    inline QList<QString> languages() const;

    inline bool isPosting() const;
    inline bool hasPostingJobs() const;
    void        closeAllPostingJobs();

    bool hasMonitoringPostingJobs() const;
    void closeAllMonitoringJobs();

    inline bool dispPostingFile() const;

    void saveConfig() const;

    void setDelFilesAfterPosted(bool delFiles);
    void addMonitoringFolder(QString const &dirPath);

    void changeLanguage(QString const &lang);

    void doNzbPostCMD(PostingJob *job);

    //    inline std::string from() const;

    bool isPaused() const;
    void pause() const;
    void resume();

    inline QString groups() const;
    //    inline QStringList getPostingGroups() const;
    inline bool groupPolicyPerFile() const;

    inline bool nzbCheck() const;
    int         nbMissingArticles() const; //!< output of the program when doing nzbCheck

    inline QString const &postHistoryFile() const;
    inline QString const &historyFieldSeparator() const;
    bool                  initHistoryDatabase() const;
    inline Database      *historyDatabase() const;

    void           startLogInFile() const;
    void           setDisplayProgress(QString const &txtValue);
    void           setProxy(QString const &url);
    void           setShutdownCmd(QString const &cmd) { _shutdownCmd = cmd; }
    QString const &shutdownCmd() const { return _shutdownCmd; }

    bool dispProgressBar() const { return _dispProgressBar; }
    bool dispFilesPosting() const { return _dispFilesPosting; }

#ifdef __USE_HMI__
    bool hmiUseFixedPassword() const;
#endif

    void showVersionASCII() const;

    bool doNzbCheck(QString const &nzbPath);

    void startFolderMonitoring(QString const &folderPath);
    void post(QFileInfo const &fileInfo, QString const &monitorFolder = "");

public slots:
    void onCheckForNewVersion();
#ifdef __USE_HMI__
    void onDonation();
    void onDonationBTC();
    void onAboutClicked();
#endif

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
    void onRefreshprogressbarBar();

    void onNewFileToProcess(QFileInfo const &fileInfo);

#ifdef __USE_HMI__
    void onSwitchNightMode();
#endif

private:
    void _loadTanslators();

    void _finishPosting();

    void _prepareNextPacking();

    void _stopMonitoring();

    // Static functions
public:
    inline static QString asciiArtWithVersion();
    inline static QString desc(bool useHTML = false);

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    inline static QStringList parseCombinedArgString(QString const &program);
#endif

#ifdef __COMPUTE_IMMEDIATE_SPEED__
    inline static int immediateSpeedDuration();
    inline static int immediateSpeedDurationMs();
#endif

    inline static QString optionName(NgConf::Opt key);

    inline static const QString version();
    inline static QString       confVersion();
};

bool NgPost::nzbCheck() const { return _nzbCheck != nullptr; }

QString NgPost::asciiArtWithVersion()
{
    return QString("%1                          v%2\n").arg(NgConf::kNgPostASCII, NgConf::kVersion);
}

QList<QString> NgPost::languages() const { return _translators.keys(); }

bool NgPost::isPosting() const { return _activeJob != nullptr; }
bool NgPost::hasPostingJobs() const { return (_activeJob || _pendingJobs.size()) ? true : false; }

bool NgPost::dispPostingFile() const { return _dispFilesPosting; }

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
QStringList NgPost::parseCombinedArgString(QString const &program)
{
    // from Qt old code
    // (https://code.woboq.org/qt5/qtbase/src/corelib/io/qprocess.cpp.html#_ZL22parseCombinedArgStringRK7QString)
    // cf https://forum.qt.io/topic/116527/deprecation-of-qprocess-start-and-execute-without-argument-list
    QStringList args;
    QString     tmp;
    int         quoteCount = 0;
    bool        inQuote    = false;
    // handle quoting. tokens can be surrounded by double quotes
    // "hello world". three consecutive double quotes represent
    // the quote character itself.
    for (int i = 0; i < program.size(); ++i)
    {
        if (program.at(i) == QLatin1Char('"'))
        {
            ++quoteCount;
            if (quoteCount == 3)
            {
                // third consecutive quote
                quoteCount = 0;
                tmp += program.at(i);
            }
            continue;
        }
        if (quoteCount)
        {
            if (quoteCount == 1)
                inQuote = !inQuote;
            quoteCount = 0;
        }
        if (!inQuote && program.at(i).isSpace())
        {
            if (!tmp.isEmpty())
            {
                args += tmp;
                tmp.clear();
            }
        }
        else
        {
            tmp += program.at(i);
        }
    }
    if (!tmp.isEmpty())
        args += tmp;
    return args;
}
#endif

inline QString const &NgPost::postHistoryFile() const { return _postHistoryFile; }
inline QString const &NgPost::historyFieldSeparator() const { return _historyFieldSeparator; }

inline Database *NgPost::historyDatabase() const { return _dbHistory; }

#ifdef __COMPUTE_IMMEDIATE_SPEED__
int NgPost::immediateSpeedDuration() { return kImmediateSpeedDurationMs / 1000; }
int NgPost::immediateSpeedDurationMs() { return kImmediateSpeedDurationMs; }
#endif

QString NgPost::optionName(NgConf::Opt key) { return NgConf::kOptionNames.value(key, ""); }

inline QString const NgPost::version() { return NgConf::kVersion; }
inline QString       NgPost::confVersion() { return NgConf::kConfVersion; }

QString NgPost::desc(bool useHTML)
{
    QString desc = useHTML
            ? QString("%1 "
                      "%2<br/>%3<br/><br/>%4<ul><li>%5</li><li>%6</li><li>%7</li><li>%8</li><li>%9</li><li>%10</"
                      "li><li>%11</li></ul>%12<br/><br/>%13<br/>")
            : QString("%1 %2\n%3\n\n%4\n    - %5\n    - %6\n    - %7\n    - %8\n    - %9\n    - %10\n    - "
                      "%11\n%12\n\n%13\n");
    return desc.arg(NgConf::kAppName)
            .arg(tr("is a CMD/GUI Usenet binary poster developped in C++11/Qt5:"))
            .arg(tr("It is designed to be as fast as possible and offer all the main features to post data "
                    "easily "
                    "and safely."))
            .arg(tr("Here are the main features and advantages of ngPost:"))
            .arg(tr("compress (using your external rar binary) and generate the par2 before posting!"))
            .arg(tr("scan folder(s) and post each file/folder individually after having them compressed"))
            .arg(tr("monitor folder(s) to post each new file/folder individually after having them compressed"))
            .arg(tr("auto delete files/folders once posted (only in command line with --auto or --monitor)"))
            .arg(tr("generate the nzb"))
            .arg(tr("invisible mode: full article obfuscation, unique feature making all Articles completely "
                    "unrecognizable without the nzb"))
            .arg("...")
            .arg(tr("for more details, cf %1")
                         .arg(useHTML ? "<a "
                                        "href=\"https://github.com/mbruel/ngPost/\">https://github.com/mbruel/"
                                        "ngPost</a>"
                                      : "https://github.com/mbruel/ngPost"))
            .arg(tr("If you'd like to translate ngPost in your language, it's easy, please contact me at "
                    "Matthieu.Bruel@gmail.com"));
}

#endif // NGPOST_H
