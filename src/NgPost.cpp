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

#include "NgPost.h"
#include "PostingJob.h"
#include "FoldersMonitorForNewFiles.h"
#include "nntp/NntpArticle.h"
#include "nntp/NntpServerParams.h"
#include "hmi/MainWindow.h"
#include "hmi/PostingWidget.h"
#include "hmi/AutoPostWidget.h"
#include "FileUploader.h"

#include <cmath>
#include <QApplication>
#include <QThread>
#include <QTextStream>
#include <QCommandLineParser>
#include <QDebug>
#include <iostream>
#include <QNetworkReply>
#include <QMessageBox>
#include <QRegularExpression>
#include <QDir>

const char *NgPost::sAppName       = "ngPost";
const QString NgPost::sVersion     = QString::number(APP_VERSION);
const QString NgPost::sProFileURL  = "https://raw.githubusercontent.com/mbruel/ngPost/master/src/ngPost.pro";
const QString NgPost::sDonationURL = "https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=W2C236U6JNTUA&item_name=ngPost&currency_code=EUR";

const QString NgPost::sMainThreadName     = "MainThread";
const char *NgPost::sFolderMonitoringName = QT_TRANSLATE_NOOP("NgPost", "Auto Posting");
const char *NgPost::sQuickJobName         = QT_TRANSLATE_NOOP("NgPost", "Quick Post");
const char *NgPost::sDonationTooltip      = QT_TRANSLATE_NOOP("NgPost", "Donations are welcome, I spent quite some time to develop this app and make a sexy GUI although I'm not using it ;)");

std::string NgPost::sArticleIdSignature   = sDefaultMsgIdSignature;
const std::string NgPost::sRandomAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

qint64        NgPost::sArticleSize = sDefaultArticleSize;
const QString NgPost::sSpace       = sDefaultSpace;

const QMap<NgPost::Opt, QString> NgPost::sOptionNames =
{
    {Opt::HELP,           "help"},
    {Opt::LANG,           "lang"},
    {Opt::VERSION,        "version"},
    {Opt::CONF,           "conf"},
    {Opt::SHUTDOWN_CMD,   "shutdown_cmd"},
    {Opt::DISP_PROGRESS,  "disp_progress"},
    {Opt::DEBUG,          "debug"},
    {Opt::DEBUG_FULL,     "fulldebug"},
    {Opt::POST_HISTORY,   "post_history"},
    {Opt::FIELD_SEPARATOR,"field_separator"},
    {Opt::NZB_UPLOAD_URL, "nzb_upload_url"},
    {Opt::NZB_POST_CMD,   "nzb_post_cmd"},
    {Opt::NZB_RM_ACCENTS, "nzb_rm_accents"},
    {Opt::AUTO_CLOSE_TABS,"auto_close_tabs"},
    {Opt::RAR_NO_ROOT_FOLDER, "rar_no_root_folder"},

    {Opt::NO_RESUME_AUTO,  "no_resume_auto"},
    {Opt::RESUME_WAIT,     "resume_wait"},
    {Opt::SOCK_TIMEOUT,    "sock_timeout"},
    {Opt::PREPARE_PACKING, "prepare_packing"},


    {Opt::INPUT,        "input"},
    {Opt::AUTO_DIR,     "auto"},
    {Opt::MONITOR_DIR,  "monitor"},
    {Opt::DEL_AUTO,     "rm_posted"},
    {Opt::OUTPUT,       "output"},
    {Opt::NZB_PATH,     "nzbpath"},
    {Opt::THREAD,       "thread"},

    {Opt::MONITOR_FOLDERS,    "monitor_nzb_folders"},
    {Opt::MONITOR_EXT,        "monitor_extensions"},
    {Opt::MONITOR_IGNORE_DIR, "monitor_ignore_dir"},

    {Opt::MSG_ID,       "msg_id"},
    {Opt::META,         "meta"},
    {Opt::ARTICLE_SIZE, "article_size"},
    {Opt::FROM,         "from"},
    {Opt::GROUPS,       "groups"},
    {Opt::NB_RETRY,     "retry"},
    {Opt::GEN_FROM,     "gen_from"},

    {Opt::OBFUSCATE,    "obfuscate"},
    {Opt::INPUT_DIR,    "inputdir"},


    {Opt::TMP_DIR,      "tmp_dir"},
    {Opt::RAR_PATH,     "rar_path"},
    {Opt::RAR_EXTRA,    "rar_extra"},
    {Opt::RAR_SIZE,     "rar_size"},
    {Opt::RAR_MAX,      "rar_max"},
    {Opt::KEEP_RAR,     "keep_rar"},

    {Opt::PAR2_PCT,     "par2_pct"},
    {Opt::PAR2_PATH,    "par2_path"},
    {Opt::PAR2_ARGS,    "par2_args"},

    {Opt::AUTO_COMPRESS,"auto_compress"},
    {Opt::COMPRESS,     "compress"},
    {Opt::GEN_PAR2,     "gen_par2"},
    {Opt::GEN_NAME,     "gen_name"},
    {Opt::GEN_PASS,     "gen_pass"},
    {Opt::RAR_NAME,     "rar_name"},
    {Opt::RAR_PASS,     "rar_pass"},
    {Opt::LENGTH_NAME,  "length_name"},
    {Opt::LENGTH_PASS,  "length_pass"},

    {Opt::SERVER,       "server"},
    {Opt::HOST,         "host"},
    {Opt::PORT,         "port"},
    {Opt::SSL,          "ssl"},
    {Opt::USER,         "user"},
    {Opt::PASS,         "pass"},
    {Opt::CONNECTION,   "connection"},
    {Opt::ENABLED,      "enabled"},
};

const QList<QCommandLineOption> NgPost::sCmdOptions = {
    { sOptionNames[Opt::HELP],                tr("Help: display syntax")},
    {{"v", sOptionNames[Opt::VERSION]},       tr( "app version")},
    {{"c", sOptionNames[Opt::CONF]},          tr( "use configuration file (if not provided, we try to load $HOME/.ngPost)"), sOptionNames[Opt::CONF]},
    { sOptionNames[Opt::DISP_PROGRESS],       tr( "display cmd progressbar: NONE (default), BAR or FILES"), sOptionNames[Opt::DISP_PROGRESS]},
    {{"d", sOptionNames[Opt::DEBUG]},         tr( "display extra information")},
    { sOptionNames[Opt::DEBUG_FULL],          tr( "display full debug information")},
    {{"l", sOptionNames[Opt::LANG]},          tr( "application language"), sOptionNames[Opt::LANG]},

// automated posting (scanning and/or monitoring)
    { sOptionNames[Opt::AUTO_DIR],            tr("parse directory and post every file/folder separately. You must use --compress, should add --gen_par2, --gen_name and --gen_pass"), sOptionNames[Opt::AUTO_DIR]},
    { sOptionNames[Opt::MONITOR_DIR],         tr("monitor directory and post every new file/folder. You must use --compress, should add --gen_par2, --gen_name and --gen_pass"), sOptionNames[Opt::MONITOR_DIR]},
    { sOptionNames[Opt::DEL_AUTO],            tr("delete file/folder once posted. You must use --auto or --monitor with this option.")},

// quick posting (several files/folders)
    {{"i", sOptionNames[Opt::INPUT]},         tr("input file to upload (single file or directory), you can use it multiple times"), sOptionNames[Opt::INPUT]},
    {{"o", sOptionNames[Opt::OUTPUT]},        tr("output file path (nzb)"), sOptionNames[Opt::OUTPUT]},   

// general options
    {{"x", sOptionNames[Opt::OBFUSCATE]},     tr("obfuscate the subjects of the articles (CAREFUL you won't find your post if you lose the nzb file)")},
    {{"g", sOptionNames[Opt::GROUPS]},        tr("newsgroups where to post the files (coma separated without space)"), sOptionNames[Opt::GROUPS]},
    {{"m", sOptionNames[Opt::META]},          tr("extra meta data in header (typically \"password=qwerty42\")"), sOptionNames[Opt::META]},
    {{"f", sOptionNames[Opt::FROM]},          tr("poster email (random one if not provided)"), sOptionNames[Opt::FROM]},
    {{"a", sOptionNames[Opt::ARTICLE_SIZE]},  tr("article size (default one: %1)").arg(sDefaultArticleSize), sOptionNames[Opt::ARTICLE_SIZE]},
    {{"z", sOptionNames[Opt::MSG_ID]},        tr("msg id signature, after the @ (default one: %1)").arg(sDefaultMsgIdSignature), sOptionNames[Opt::MSG_ID]},
    {{"r", sOptionNames[Opt::NB_RETRY]},      tr("number of time we retry to an Article that failed (default: %1)").arg(NntpArticle::nbMaxTrySending()), sOptionNames[Opt::NB_RETRY]},
    {{"t", sOptionNames[Opt::THREAD]},        tr("number of Threads (the connections will be distributed amongs them)"), sOptionNames[Opt::THREAD]},
    { sOptionNames[Opt::GEN_FROM],            tr("generate a new random email for each Post (--auto or --monitor)")},


// for compression and par2 support
    { sOptionNames[Opt::TMP_DIR],             tr( "temporary folder where the compressed files and par2 will be stored"), sOptionNames[Opt::TMP_DIR]},
    { sOptionNames[Opt::RAR_PATH],            tr( "RAR absolute file path (external application)"), sOptionNames[Opt::RAR_PATH]},
    { sOptionNames[Opt::RAR_SIZE],            tr( "size in MB of the RAR volumes (0 by default meaning NO split)"), sOptionNames[Opt::RAR_SIZE]},
    { sOptionNames[Opt::RAR_MAX],             tr( "maximum number of archive volumes"), sOptionNames[Opt::RAR_MAX]},
    { sOptionNames[Opt::PAR2_PCT],            tr( "par2 redundancy percentage (0 by default meaning NO par2 generation)"), sOptionNames[Opt::PAR2_PCT]},
    { sOptionNames[Opt::PAR2_PATH],           tr( "par2 absolute file path (in case of self compilation of ngPost)"), sOptionNames[Opt::PAR2_PCT]},

    { sOptionNames[Opt::AUTO_COMPRESS],       tr( "compress inputs with random name and password and generate par2 (equivalent of --compress --gen_name --gen_pass --gen_par2)")},
    { sOptionNames[Opt::COMPRESS],            tr( "compress inputs using RAR or 7z")},
    { sOptionNames[Opt::GEN_PAR2],            tr( "generate par2 (to be used with --compress)")},
    { sOptionNames[Opt::RAR_NAME],            tr( "provide the RAR file name (to be used with --compress)"), sOptionNames[Opt::RAR_NAME]},
    { sOptionNames[Opt::RAR_PASS],            tr( "provide the RAR password (to be used with --compress)"), sOptionNames[Opt::RAR_PASS]},
    { sOptionNames[Opt::GEN_NAME],            tr( "generate random RAR name (to be used with --compress)")},
    { sOptionNames[Opt::GEN_PASS],            tr( "generate random RAR password (to be used with --compress)")},
    { sOptionNames[Opt::LENGTH_NAME],         tr( "length of the random RAR name (to be used with --gen_name), default: %1").arg(sDefaultLengthName), sOptionNames[Opt::LENGTH_NAME]},
    { sOptionNames[Opt::LENGTH_PASS],         tr( "length of the random RAR password (to be used with --gen_pass), default: %1").arg(sDefaultLengthPass), sOptionNames[Opt::LENGTH_PASS]},
    { sOptionNames[Opt::RAR_NO_ROOT_FOLDER],  tr( "Remove root (parent) folder when compressing Folders using RAR")},


    {{"S", sOptionNames[Opt::SERVER]},        tr("NNTP server following the format (<user>:<pass>@@@)?<host>:<port>:<nbCons>:(no)?ssl"), sOptionNames[Opt::SERVER]},
// without config file, you can provide all the parameters to connect to ONE SINGLE server
    {{"h", sOptionNames[Opt::HOST]},          tr("NNTP server hostname (or IP)"), sOptionNames[Opt::HOST]},
    {{"P", sOptionNames[Opt::PORT]},          tr("NNTP server port"), sOptionNames[Opt::PORT]},
    {{"s", sOptionNames[Opt::SSL]},           tr("use SSL")},
    {{"u", sOptionNames[Opt::USER]},          tr("NNTP server username"), sOptionNames[Opt::USER]},
    {{"p", sOptionNames[Opt::PASS]},          tr("NNTP server password"), sOptionNames[Opt::PASS]},
    {{"n", sOptionNames[Opt::CONNECTION]},    tr("number of NNTP connections"), sOptionNames[Opt::CONNECTION]},
};


#ifdef __DEBUG__
#include <QNetworkConfigurationManager>
#endif
NgPost::NgPost(int &argc, char *argv[]):
    QObject (), CmdOrGuiApp(argc, argv),
    _cout(stdout),
    _cerr(stderr),
    _err(ERROR_CODE::NONE),
#ifdef __DEBUG__
    _debug(2),
#else
    _debug(0),
#endif
    _dispProgressBar(false),
    _dispFilesPosting(false),
    _nzbName(),
    _nbFiles(0),
    _nntpServers(),
    _obfuscateArticles(false), _obfuscateFileName(false),
    _genFrom(false), _saveFrom(false), _from(), _groups(sDefaultGroups),
    _meta(), _grpList(),
    _nbThreads(QThread::idealThreadCount()),
    _socketTimeOut(sDefaultSocketTimeOut), _nzbPath(sDefaultNzbPath), _nzbPathConf(sDefaultNzbPath),
    _progressbarTimer(), _refreshRate(sDefaultRefreshRate),
    _tmpPath(), _rarPath(), _rarArgs(), _rarSize(0), _rarMax(sDefaultRarMax), _useRarMax(false),
    _par2Pct(0), _par2Path(), _par2Args(), _par2PathConfig(),
    _doCompress(false), _doPar2(false), _genName(), _genPass(),
    _lengthName(sDefaultLengthName), _lengthPass(sDefaultLengthPass),
    _rarName(), _rarPass(), _rarPassFixed(),
    _inputDir(),
    _activeJob(nullptr), _pendingJobs(), _packingJob(nullptr),
    _historyFieldSeparator(sDefaultFieldSeparator),
    _postHistoryFile(),
    _autoDirs(),
    _folderMonitor(nullptr), _monitorThread(nullptr),
    _delAuto(false),
    _monitor_nzb_folders(false), _monitorExtensions(), _monitorIgnoreDir(false),
    _keepRar(false), _autoCompress(false),
    _lang("en"), _translators(),
    _netMgr(), _urlNzbUpload(nullptr), _urlNzbUploadStr(),
    _doShutdownWhenDone(false), _shutdownProc(nullptr),
#if defined(WIN32) || defined(__MINGW64__)
    _shutdownCmd(sDefaultShutdownCmdWindows),
#elif defined(__APPLE__)|| defined(__MACH__)
    _shutdownCmd(sDefaultShutdownCmdMacOS),
#else
    _shutdownCmd(sDefaultShutdownCmdLinux),
#endif
    _removeAccentsOnNzbFileName(false),
    _autoCloseTabs(false),
    _rarNoRootFolder(false),
    _tryResumePostWhenConnectionLost(true),
    _waitDurationBeforeAutoResume(sDefaultResumeWaitInSec),
    _nzbPostCmd(), _preparePacking(false)
{
    QThread::currentThread()->setObjectName(sMainThreadName);

    if (_hmi)
        _hmi->setWindowTitle(QString("%1_v%2").arg(sAppName).arg(sVersion));       

    // in case we want to generate random uploader (_from not provided)
    std::srand(static_cast<uint>(QDateTime::currentMSecsSinceEpoch()));

    // check if the embedded par2 is available (windows or appImage)
    QString par2Embedded;
#if defined(WIN32) || defined(__MINGW64__)
    par2Embedded = QString("%1/par2.exe").arg(qApp->applicationDirPath());
#else
    par2Embedded = QString("%1/par2").arg(qApp->applicationDirPath());
#endif

    QFileInfo fi(par2Embedded);
    if (fi.exists() && fi.isFile() && fi.isExecutable())
        _par2Path = par2Embedded;

    connect(this, &NgPost::log,   this, &NgPost::onLog,   Qt::QueuedConnection);
    connect(this, &NgPost::error, this, &NgPost::onError, Qt::QueuedConnection);

    _loadTanslators();

#if defined(__DEBUG__) && QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    QNetworkConfigurationManager netConfMgr;
    for (const QNetworkConfiguration &conf : netConfMgr.allConfigurations())
    {
        qDebug() << "net conf: " << conf.name()
                 << ", bearerType: " << conf.bearerTypeName()
                 << ", timeout: " << conf.connectTimeout()
                 << ", isValid: " << conf.isValid()
                 << ", state: " << conf.state();
    }

    QNetworkConfiguration conf = netConfMgr.defaultConfiguration();
    qDebug() << "DEFAULT conf: " << conf.name()
             << ", bearerType: " << conf.bearerTypeName()
             << ", timeout: " << conf.connectTimeout()
             << ", isValid: " << conf.isValid()
             << ", state: " << conf.state();
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(&_netMgr, &QNetworkAccessManager::networkAccessibleChanged, this, &NgPost::onNetworkAccessibleChanged);
#endif
}

void NgPost::_startMonitoring(const QString &folderPath)
{
    qDebug() << "Start Monitoring " << folderPath;
    _monitorThread = new QThread();
    _monitorThread->setObjectName("Monitoring");
    _folderMonitor = new FoldersMonitorForNewFiles(folderPath);
    _folderMonitor->moveToThread(_monitorThread);
    connect(_folderMonitor, &FoldersMonitorForNewFiles::newFileToProcess, this, &NgPost::onNewFileToProcess, Qt::QueuedConnection);
    _monitorThread->start();
}

void NgPost::_stopMonitoring()
{
    if (_folderMonitor)
    {
        _folderMonitor->stopListening();
        _monitorThread->quit();
        _monitorThread->wait();
        delete _folderMonitor;
        delete _monitorThread;
        _folderMonitor = nullptr;
        _monitorThread = nullptr;
    }
}

NgPost::~NgPost()
{
#ifdef __DEBUG__
    _log("Destuction NgPost...");
#endif

    _stopMonitoring();

//    _finishPosting();

    _progressbarTimer.stop();

    closeAllPostingJobs();

    if (_activeJob)
        delete _activeJob;
    qDeleteAll(_pendingJobs);

    qDeleteAll(_nntpServers);

    if (_urlNzbUpload)
        delete _urlNzbUpload;
}

void NgPost::_finishPosting()
{
    if (_hmi || _dispProgressBar)
        disconnect(&_progressbarTimer, &QTimer::timeout, this, &NgPost::onRefreshprogressbarBar);

    if (_activeJob && _activeJob->hasUploaded())
    {
        if (_hmi || _dispProgressBar)
        {
            onRefreshprogressbarBar();
            if (!_hmi)
                std::cout << std::endl;
        }
    }
}

void NgPost::updateGroups(const QString &groups)
{
    _groups = groups.toStdString();

    _grpList.clear();
    for (const QString &grp : groups.split(","))
        _grpList << grp;
}



int NgPost::startHMI()
{
    QString err = parseDefaultConfig();
    if (!err.isEmpty())
        _error(err);

    if (_from.empty())
            _from = randomStdFrom();
#ifdef __DEBUG__
    _dumpParams();
#endif
    _hmi->init(this);
    _hmi->show();
    changeLanguage(_lang); // reforce lang set up...

    return _app->exec();
}


void NgPost::onLog(QString msg, bool newline)
{
    _log(msg, newline);
}

void NgPost::onError(QString msg)
{
    _error(msg, ERROR_CODE::COMPLETED_WITH_ERRORS);
}


void NgPost::onErrorConnecting(QString err)
{
    _error(err, ERROR_CODE::COMPLETED_WITH_ERRORS);
}


void NgPost:: onRefreshprogressbarBar()
{
    uint nbArticlesUploaded = 0,  nbArticlesTotal = 0;
    QString avgSpeed;
#ifdef __COMPUTE_IMMEDIATE_SPEED__
    QString immediateSpeed("0 B/s");
#endif
    if (_activeJob)
    {
        nbArticlesTotal    = _activeJob->nbArticlesTotal();
        nbArticlesUploaded = _activeJob->nbArticlesUploaded();
        avgSpeed           = _activeJob->avgSpeed();
#ifdef __COMPUTE_IMMEDIATE_SPEED__
        immediateSpeed     = _activeJob->immediateSpeed();
#endif
    }
    if (_hmi)
#ifdef __COMPUTE_IMMEDIATE_SPEED__
        _hmi->updateProgressBar(nbArticlesTotal, nbArticlesUploaded, avgSpeed, immediateSpeed);
#else
        _hmi->updateProgressBar(nbArticlesTotal, nbArticlesUploaded, avgSpeed);
#endif
    else
    {
        float progressbar = static_cast<float>(nbArticlesUploaded);
        progressbar /= nbArticlesTotal;

//        qDebug() << "[NgPost::onRefreshprogressbarBar] uploaded: " << nbArticlesUploaded
//                 << " / " << nbArticlesTotal
//                 << " => progressbar: " << progressbar << "\n";

        std::cout << "\r[";
        int pos = static_cast<int>(std::floor(progressbar * sprogressbarBarWidth));
        for (int i = 0; i < sprogressbarBarWidth; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << int(progressbar * 100) << " %"
                  << " (" << nbArticlesUploaded << " / " << nbArticlesTotal << ")"
                  << " avg. speed: " << avgSpeed.toStdString();
        std::cout.flush();
    }
    if (nbArticlesUploaded < nbArticlesTotal)
        _progressbarTimer.start(_refreshRate);
}

void NgPost::onNewFileToProcess(const QFileInfo & fileInfo)
{
    if (fileInfo.isDir())
    {
        if (_monitorIgnoreDir)
        {
            if (_debug)
                _log(tr("MONITOR_IGNORE_DIR ON => Ignoring new incoming folder %1").arg(fileInfo.absoluteFilePath()));
            return;
        }
    }
    else if (!_monitorExtensions.isEmpty() && !_monitorExtensions.contains(fileInfo.suffix()))
    {
#ifdef __DEBUG__
        qDebug() << "MONITOR_EXTENSIONS ON => Ignoring new incoming file: " << fileInfo.absoluteFilePath()
                 << ", _monitorExtensions: " << _monitorExtensions
                 << ", size: " << _monitorExtensions.size()
                 << ", suffix: " << fileInfo.suffix();
#endif
        if (_debug)
            _log(tr("MONITOR_EXTENSIONS ON => Ignoring new incoming file %1").arg(fileInfo.absoluteFilePath()));
        return;
    }

    if (_hmi)
    {
        _hmi->updateAutoPostingParams();
        _hmi->setJobLabel(-1);
        _delAuto = _hmi->autoWidget()->deleteFilesOncePosted();
        _hmi->autoWidget()->newFileToProcess(fileInfo);
    }
    _log(tr("Processing new incoming file: %1").arg(fileInfo.absoluteFilePath()));
    _post(fileInfo, _monitor_nzb_folders ? QDir(fileInfo.absolutePath()).dirName() : QString());
}

#include <QTranslator>
void NgPost::_loadTanslators()
{
    QDir dir(sTranslationPath);
    for (const QFileInfo &qmFile : dir.entryInfoList(QStringList("ngPost_*.qm")))
    {
        QTranslator *translator = new QTranslator();
        QString lang = qmFile.completeBaseName(); // ngPost_en
        lang.remove(0, lang.indexOf('_') + 1); // en

        if (translator->load(qmFile.absoluteFilePath()))
            _translators.insert(lang, translator);
        else
        {
            if (lang == "en")
                _translators.insert(lang, nullptr);
            else
                _cerr << tr("error loading translator %1").arg(qmFile.absoluteFilePath()) << "\n" << MB_FLUSH;
            delete translator;
        }
    }
#ifdef __DEBUG__
    qDebug() << "available translators: " << _translators.keys().join(", ");
#endif
    if (_lang != "en"){
        if (_translators.contains(_lang))
            _app->installTranslator(_translators[_lang]);
        else
        {
            _cerr << tr("ERROR: couldn't find translator for lang %1").arg(_lang) << "\n" << MB_FLUSH;
            _lang = "en";
        }
    }
}

void NgPost::changeLanguage(const QString &lang)
{
    _app->removeTranslator(_translators[_lang]);
    _lang = lang;

    if (_translators[_lang])
        _app->installTranslator(_translators[_lang]);
}

void NgPost::checkForNewVersion()
{
    QUrl proFileURL(NgPost::proFileUrl());
    QNetworkRequest req(proFileURL);
    req.setRawHeader( "User-Agent" , "ngPost C++ app" );

    QNetworkReply *reply = _netMgr.get(req);
    QObject::connect(reply, &QNetworkReply::finished, this, &NgPost::onCheckForNewVersion);
}

void NgPost::doNzbPostCMD(const QString &nzbFilePath)
{
    // first NZB_UPLOAD_URL
    if (_urlNzbUpload)
    {
        FileUploader *testUpload = new FileUploader(_netMgr, nzbFilePath);
        connect(testUpload, &FileUploader::error,      this,       &NgPost::onError,      Qt::DirectConnection);
        connect(testUpload, &FileUploader::log,        this,       &NgPost::onLog,        Qt::DirectConnection);
        connect(testUpload, &FileUploader::readyToDie, testUpload, &QObject::deleteLater, Qt::QueuedConnection);
        testUpload->startUpload(*_urlNzbUpload);
    }

    // second NZB_POST_CMD
    if (!_nzbPostCmd.isEmpty())
    {
        QString  fullCmd = _nzbPostCmd.arg(nzbFilePath);
        QStringList args = parseCombinedArgString(fullCmd);
        QString     cmd  = args.takeFirst();
        qDebug() << "cmd: " << cmd << ", args: " << args;
        int res = QProcess::execute(cmd, args);
        if (debugMode())
            _log(tr("NZB Post cmd: %1 exitcode: %2").arg(fullCmd).arg(res));
        else
            _log(fullCmd);
    }

}

bool NgPost::isPaused() const
{
    if (_activeJob && _activeJob->isPaused())
        return true;
    else
        return false;
}

void NgPost::pause() const
{
    if (_activeJob && !_activeJob->isPaused())
    {
        _activeJob->pause();
        if (_hmi)
            _hmi->setPauseIcon(false);
    }
}

void NgPost::resume() const
{
    if (_activeJob && _activeJob->isPaused())
    {
        _activeJob->resume();
        if (_hmi)
            _hmi->setPauseIcon(true);
    }
}



void NgPost::_post(const QFileInfo &fileInfo, const QString &monitorFolder)
{
    setNzbName(fileInfo);
    QString nzbFilePath = nzbPath(monitorFolder);
    if (!nzbFilePath.endsWith(".nzb"))
        nzbFilePath += ".nzb";

    _rarName = _nzbName;
    if (_genName)
        _rarName = randomPass(_lengthName);

    _rarPass = "";
    if (_genPass) // shall we gen password?
        _rarPass = randomPass(_lengthPass);        
    if (!_rarPassFixed.isEmpty()) // rar pass fixed would take other
        _rarPass = _rarPassFixed;
    if (!_rarPass.isEmpty())
        _meta.remove("password");

    qDebug() << "Start posting job for " << _nzbName
             << " with rar_name: " << _rarName << " and pass: " << _rarPass
             << " (auto delete: " << _delAuto << ")";

    startPostingJob(new PostingJob(this, nzbFilePath, {fileInfo}, nullptr,
                                   _grpList, _groups, from(),
                                   _obfuscateArticles, _obfuscateFileName,
                                   _tmpPath, _rarPath,
                                   _rarSize, _useRarMax, _par2Pct,
                                   _doCompress, _doPar2, _rarName, _rarPass,
                                   _keepRar, _delAuto, false));
}



void NgPost::onCheckForNewVersion()
{
    QNetworkReply      *reply = static_cast<QNetworkReply*>(sender());
    QRegularExpression appVersionRegExp("^DEFINES \\+= \"APP_VERSION=\\\\\"((\\d+)\\.(\\d+))\\\\\"\"$");
    QStringList v = sVersion.split(".");
    int currentMajor = v.at(0).toInt(), currentMinor = v.at(1).toInt();
    while (!reply->atEnd())
    {
        QString line = reply->readLine().trimmed();
        QRegularExpressionMatch match = appVersionRegExp.match(line);
        if (match.hasMatch())
        {
            QString lastRealease = match.captured(1);
            int lastMajor = match.captured(2).toInt(), lastMinor = match.captured(3).toInt();
            qDebug() << "lastMajor: " << lastMajor << ", lastMinor: " << lastMinor
                     << " (currentMajor: " << currentMajor << ", currentMinor: " << currentMinor << ")";

            if (lastMajor > currentMajor ||
                    (lastMajor == currentMajor && lastMinor > currentMinor) )
            {
                if (_hmi)
                {
                    QString msg = tr("<center><h3>New version available on GitHUB</h3></center>");
                    msg += tr("<br/>The last release is now <b>v%1</b>").arg(lastRealease);
                    msg += tr("<br/><br/>You can download it from the <a href='https://github.com/mbruel/ngPost/releases/tag/v%1'>release directory</a>").arg(lastRealease);
                    msg += tr("<br/><br/>Here are the full <a href='https://github.com/mbruel/ngPost/blob/master/release_notes.txt'>release_notes</a>");

                    QMessageBox::information(_hmi, tr("New version available"), msg);
                }
                else
                    qCritical() << "There is a new version available on GitHUB: v" << lastRealease
                                << " (visit https://github.com/mbruel/ngPost/ to get it)";
            }

            break; // no need to continue to parse the page
        }
    }
    reply->deleteLater();
}

#include <QDesktopServices>
void NgPost::onDonation()
{
    QDesktopServices::openUrl(sDonationURL);
}

#include "hmi/AboutNgPost.h"
void NgPost::onAboutClicked()
{
    AboutNgPost about(this);
    about.exec();
}

void NgPost::onPostingJobStarted()
{
    if (_hmi || _dispProgressBar)
    {
        connect(&_progressbarTimer, &QTimer::timeout, this, &NgPost::onRefreshprogressbarBar, Qt::DirectConnection);
        _progressbarTimer.start(_refreshRate);
    }
}

void NgPost::onPackingDone()
{
    if (_preparePacking)
    {
        PostingJob *job = static_cast<PostingJob*>(sender());
        if (job == _activeJob)
        {
#ifdef __DEBUG__
            _log("[NgPost::onPackingDone] Active Job packed :)");
#endif
            // The previous active Job finished posting before the packing of the next one
            if (_packingJob == nullptr)
            {
                if (!job->hasPostStarted())
                {
#ifdef __DEBUG__
            _log("[NgPost::onPackingDone] Active Job didn't start posting...");
#endif
                    job->_postFiles();
                }
                _prepareNextPacking();
            }
        }
        else if (job != _packingJob)
            _error("unexpected job finished packing..."); // should never happen...
#ifdef __DEBUG__
        else
            _log("[NgPost::onPackingDone] Packing Job done :)");
#endif
    }
}

void NgPost::_prepareNextPacking()
{
    if (_pendingJobs.size())
    {
        _packingJob = _pendingJobs.first();
        if (_packingJob->hasCompressed())
            emit _packingJob->startPosting(false);
    }
}

void NgPost::onPostingJobFinished()
{
    PostingJob *job = static_cast<PostingJob*>(sender());
    if (job == _activeJob)
    {
        if (_hmi && !job->widget())
            _hmi->autoWidget()->updateFinishedJob(job->getFirstOriginalFile(),
                                                  job->nbArticlesTotal(),
                                                  job->nbArticlesUploaded(),
                                                  job->nbArticlesFailed());

        if (_activeJob->hasPostFinished() && !_postHistoryFile.isEmpty())
        {
            QFile hist(_postHistoryFile);
            if (hist.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text))
            {
                QTextStream stream(&hist);
                stream << QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm:ss")
                       << _historyFieldSeparator << _activeJob->nzbName()
                       << _historyFieldSeparator << _activeJob->postSize()
                       << _historyFieldSeparator << _activeJob->avgSpeed();
                if (_activeJob->hasCompressed())
                    stream << _historyFieldSeparator << _activeJob->rarName()
                           << _historyFieldSeparator << _activeJob->rarPass();
                else
                    stream << _historyFieldSeparator << _historyFieldSeparator;

                stream << _historyFieldSeparator << _activeJob->groups()
                       << _historyFieldSeparator << _activeJob->from()
                       << "\n" << MB_FLUSH;
                hist.close();
            }
        }

        if (_hmi && _autoCloseTabs && _activeJob->hasPostFinishedSuccessfully())
            _hmi->closeTab(_activeJob->widget());

        _activeJob->deleteLater();
        _activeJob = nullptr;

        if (_pendingJobs.size())
        {
            _activeJob = _pendingJobs.dequeue();

            if (_hmi)
                _hmi->setTab(_activeJob->widget());

            if (_preparePacking)
            {
                if (_packingJob == _activeJob)
                {
                    _packingJob = nullptr;
                    if (_activeJob->isPacked())
                    {
                        _activeJob->_postFiles();
                        _prepareNextPacking();
                    }
                    // otherwise it will be triggered automatically when the packing is finished
                    // as it is now the active job ;)
                }
                else
                    _error("next active job different to the packing one..."); // should never happen...
            }
            else
                emit _activeJob->startPosting(true);
        }
        else if (_doShutdownWhenDone && !_shutdownCmd.isEmpty())
        {
            //cf https://forum.qt.io/topic/111602/qprocess-signals-not-received-in-slots-except-in-debug-with-breakpoints/
//            int exitCode = QProcess::execute("echo \\\"toto\\\" | /usr/bin/sudo -S /bin/ls -al");
//            qDebug() << QString("Shutdown exit code: %1").arg(exitCode);
            _shutdownProc = new QProcess();
            connect(_shutdownProc, &QProcess::readyReadStandardOutput, this, &NgPost::onShutdownProcReadyReadStandardOutput, Qt::DirectConnection);
            connect(_shutdownProc, &QProcess::readyReadStandardError,  this, &NgPost::onShutdownProcReadyReadStandardError,  Qt::DirectConnection);
            connect(_shutdownProc, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &NgPost::onShutdownProcFinished, Qt::QueuedConnection);
//            connect(_shutdownProc, &QProcess::started, this, &NgPost::onShutdownProcStarted, Qt::DirectConnection);
//            connect(_shutdownProc, &QProcess::stateChanged,  this, &NgPost::onShutdownProcStateChanged,  Qt::DirectConnection);
            connect(_shutdownProc, &QProcess::errorOccurred,  this, &NgPost::onShutdownProcError, Qt::DirectConnection);
//            _shutdownProc->start("/bin/ls", QStringList() << "-al");
//            _shutdownProc->start("/usr/bin/sudo", QStringList() << "-n" << "/sbin/poweroff");

//            qDebug() << " cmd: " << _shutdownCmd;

//            QStringList args = _shutdownCmd.split(QRegularExpression("\\s+"));
            QStringList args = parseCombinedArgString(_shutdownCmd);
            QString     cmd  = args.takeFirst();
            qDebug() << "cmd: " << cmd << ", args: " << args;
            _shutdownProc->start(cmd, args);
        }
        else if (!_folderMonitor && !_hmi)
        {
	    if( debugFull())
                _error(tr(" => closing application"));
            qApp->quit();
        }
    }
    else
    {
        // it was a Pending job that has been cancelled
        _pendingJobs.removeOne(job);
        job->deleteLater();
    }
}

void NgPost::onShutdownProcReadyReadStandardOutput()
{
    QString line(_shutdownProc->readAllStandardOutput());
//    _cout << "Shutdown out: " << line << "\n" << MB_FLUSH;
    _log(QString("Shutdown out: %1").arg(QString(line)));
}

void NgPost::onShutdownProcReadyReadStandardError()
{
    QString line(_shutdownProc->readAllStandardError());
//    _cout << "Shutdown ERROR: " << line << "\n" << MB_FLUSH;
    _error(QString("Shutdown ERROR: %1").arg(line));
}

void NgPost::onShutdownProcFinished(int exitCode)
{
    if (_debug)
        _log(QString("Shutdown proc exitCode: ").arg(exitCode));
    _shutdownProc->deleteLater();
    _shutdownProc = nullptr;
}

//void NgPost::onShutdownProcStarted()
//{
//    qDebug() << "Shutdown proc Started";
//}

//void NgPost::onShutdownProcStateChanged(QProcess::ProcessState newState)
//{
//    qDebug() << "Shutdown proc new State: " << newState;
//}

void NgPost::onShutdownProcError(QProcess::ProcessError error)
{
    _error(QString("Shutdown process Error: %1").arg(error));
}

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
void NgPost::onNetworkAccessibleChanged(QNetworkAccessManager::NetworkAccessibility accessible)
{
    qDebug() << "[NgPost::onNetworkAccessibleChanged] accessible: " << accessible;
    QString msg(tr("Network access changed: %1"));
    if (accessible == QNetworkAccessManager::NetworkAccessibility::Accessible)
    {
        _log(msg.arg("ON"));
#ifndef __USE_CONNECTION_TIMEOUT__
        if (_activeJob && _activeJob->isPaused())
            _activeJob->resume();
#endif
    }
    else
    {
        _error(msg.arg("OFF"));
#ifndef __USE_CONNECTION_TIMEOUT__
        if (_activeJob)
            _activeJob->pause();
#endif
    }
}
#endif

void NgPost::_log(const QString &aMsg, bool newline) const
{
    if (_hmi)
        _hmi->log(aMsg, newline);
    else
    {
        _cout << aMsg;
        if (newline)
            _cout << "\n";
        _cout << MB_FLUSH;
    }
}

void NgPost::_error(const QString &error) const
{
    if (_hmi)
        _hmi->logError(error);
    else
        _cerr << error << "\n" << MB_FLUSH;
}

void NgPost::_error(const QString &error, NgPost::ERROR_CODE code)
{
    _err = code;
    _error(error);
}


QString NgPost::randomPass(uint length) const
{
    QString pass, alphabet("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    int nbLetters = alphabet.length();
    for (uint i = 0 ; i < length ; ++i)
        pass.append(alphabet.at(std::rand()%nbLetters));

    return pass;
}

void NgPost::closeAllPostingJobs()
{
    qDeleteAll(_pendingJobs);
    if (_activeJob)
        _activeJob->onStopPosting();
}

void NgPost::closeAllMonitoringJobs()
{
    auto it = _pendingJobs.begin();
    while (it != _pendingJobs.end())
    {
        PostingJob *job = *it;
        if (!job->widget())
        {
            it = _pendingJobs.erase(it);
            if (_debug)
                _error(tr("Cancelling monitoring job: %1").arg(job->getFirstOriginalFile()));
            delete  job;
        }
        else
            ++it;
    }
    if (_activeJob && !_activeJob->widget())
    {
        emit _activeJob->stopPosting();
        if (_debug)
            _error(tr("Stopping monitoring job: %1").arg(_activeJob->getFirstOriginalFile()));

    }
}

bool NgPost::hasMonitoringPostingJobs() const
{
    if (_activeJob)
    {
        if (!_activeJob->widget())
            return true;
    }
    else if (_pendingJobs.size())
    {
        for (PostingJob *job : _pendingJobs)
        {
            if (!job->widget())
                return true;
        }
    }
    return false;
}


bool NgPost::parseCommandLine(int argc, char *argv[])
{
    QString appVersion = QString("%1_v%2").arg(sAppName).arg(sVersion);
    QCommandLineParser parser;
    parser.setApplicationDescription(appVersion);
    parser.addOptions(sCmdOptions);


    // Process the actual command line arguments given by the user
    QStringList args;
    for (int i = 0; i < argc; ++i)
        args << argv[i];

    bool res = parser.parse(args);
#ifdef __DEBUG__
    qDebug() << "args: " << args
             << "=> parsing: " << res << " (error: " << parser.errorText() << ")";
#endif

    if (parser.isSet(sOptionNames[Opt::CONF]))
    {
        QString err = _parseConfig(parser.value(sOptionNames[Opt::CONF]));
        if (!err.isEmpty())
        {
            _error(err, ERROR_CODE::ERR_CONF_FILE);
            return false;
        }
    }
    else
    {
        QString err = parseDefaultConfig();
        if (!err.isEmpty())
        {
            _error(err, ERROR_CODE::ERR_CONF_FILE);
            return false;
        }
    }

    if (!parser.parse(args))
    {
        _error(tr("Error syntax: %1\nTo list the available options use: %2 --help\n").arg(parser.errorText()).arg(argv[0]),
                ERROR_CODE::ERR_WRONG_ARG);
        return false;
    }

    if (parser.isSet(sOptionNames[Opt::LANG]))
    {
        QString lang = parser.value(sOptionNames[Opt::LANG]).toLower();
        _cout << "Lang: " << lang << "\n" << MB_FLUSH;
        changeLanguage(lang);
    }

    if (parser.isSet(sOptionNames[Opt::HELP]))
    {
        _showVersionASCII();
        _syntax(argv[0]);
        return false;
    }

    if (parser.isSet(sOptionNames[Opt::VERSION]))
    {
        _showVersionASCII();
        return false;
    }

    if (!parser.isSet(sOptionNames[Opt::INPUT]) && !parser.isSet(sOptionNames[Opt::AUTO_DIR]) && !parser.isSet(sOptionNames[Opt::MONITOR_DIR]))
    {
        _error(tr("Error syntax: you should provide at least one input file or directory using the option -i, --auto or --monitor"),
               ERROR_CODE::ERR_NO_INPUT);
        return false;
    }
    if (parser.isSet(sOptionNames[Opt::DEL_AUTO]))
    {
        if ( !parser.isSet(sOptionNames[Opt::AUTO_DIR]) && !parser.isSet(sOptionNames[Opt::MONITOR_DIR]))
        {
            _error(tr("Error syntax: --del option is only available with --auto or --monitor"),
                   ERROR_CODE::ERR_DEL_AUTO);
            return false;
        }
        else
            _delAuto = true;
    }

    if (parser.isSet(sOptionNames[Opt::AUTO_DIR]))
    {
        if (!_autoCompress && !parser.isSet(sOptionNames[Opt::COMPRESS]))
        {
            _error(tr("Error syntax: --auto only works with --compress or AUTO_COMPRESS in config"),
                   ERROR_CODE::ERR_AUTO_NO_COMPRESS);
            return false;
        }
        for (const QString &filePath : parser.values(sOptionNames[Opt::AUTO_DIR]))
        {
            QFileInfo fi(filePath);
            if (!fi.exists() || !fi.isDir())
            {
                _error(tr("Error syntax: --auto only uses folders as argument..."),
                       ERROR_CODE::ERR_AUTO_INPUT);
                return false;
            }
            else
                _autoDirs << QDir(fi.absoluteFilePath());
        }
    }        

    if (parser.isSet(sOptionNames[Opt::MONITOR_DIR]))
    {
        if (!parser.isSet(sOptionNames[Opt::COMPRESS]))
        {
            _error(tr("Error syntax: --monitor only works with --compress"),
                   ERROR_CODE::ERR_MONITOR_NO_COMPRESS);
            return false;
        }
        for (const QString &filePath : parser.values(sOptionNames[Opt::MONITOR_DIR]))
        {
            QFileInfo fi(filePath);
            if (!fi.exists() || !fi.isDir())
            {
                _error(tr("Error syntax: --monitor only uses folders as argument..."),
                       ERROR_CODE::ERR_MONITOR_INPUT);
                return false;
            }
            else
            {
                _cout << "[FolderMonitor] " << tr("start monitoring: ") << fi.absoluteFilePath() << "\n" << MB_FLUSH;
                if (_folderMonitor)
                    _folderMonitor->addFolder(fi.absoluteFilePath());
                else
                    _startMonitoring(fi.absoluteFilePath());
            }
        }
    }

    if (parser.isSet(sOptionNames[Opt::OBFUSCATE]))
    {
        _obfuscateArticles = true;
        _cout << tr("Do article obfuscation (the subject of each Article will be a UUID)\n") << MB_FLUSH;
    }

    if (parser.isSet(sOptionNames[Opt::DEBUG]))
    {
        _debug = 1;
        _cout << tr("Extra logs are ON\n") << MB_FLUSH;
    }
    if (parser.isSet(sOptionNames[Opt::DEBUG_FULL]))
    {
        _debug = 2;
        _cout << tr("Full debug logs are ON\n") << MB_FLUSH;
    }

    if (parser.isSet(sOptionNames[Opt::THREAD]))
    {
        bool ok;
        _nbThreads = parser.value(sOptionNames[Opt::THREAD]).toInt(&ok);
        if (_nbThreads < 1)
            _nbThreads = 1;
        if (!ok)
        {
            _error(tr("You should give an integer for the number of threads (option -t)"),
                   ERROR_CODE::ERR_NB_THREAD);
            return false;
        }
    }

    if (parser.isSet(sOptionNames[Opt::META]))
    {
        for (const QString &meta : parser.values(sOptionNames[Opt::META]))
        {
            QStringList mList = meta.split("=");
            if (mList.size() == 2)
                _meta.insert(escapeXML(mList[0]), escapeXML(mList[1]));
        }
    }


    if (parser.isSet(sOptionNames[Opt::GROUPS]))
        updateGroups(parser.value(sOptionNames[Opt::GROUPS]));

    if (parser.isSet(sOptionNames[Opt::FROM]))
    {
        QString val = parser.value(sOptionNames[Opt::FROM]);
        QRegularExpression email("\\w+@\\w+\\.\\w+");
        if (!email.match(val).hasMatch())
            val += "@ngPost.com";
        val = escapeXML(val);
        _from = val.toStdString();
    }


    if (parser.isSet(sOptionNames[Opt::GEN_FROM]))
    {
        _genFrom = true;
        if (_debug)
            _cout << tr("Generate new random poster for each post") << "\n" << MB_FLUSH;
    }
    else if (_from.empty())
        _from = randomStdFrom();


    if (parser.isSet(sOptionNames[Opt::DISP_PROGRESS]))
    {
        QString val = parser.value(sOptionNames[Opt::DISP_PROGRESS]);
        val = val.trimmed().toLower();
        if (val == "bar")
        {
            _dispProgressBar  = true;
            _dispFilesPosting = false;
            qDebug() << "Display progressbar bar\n";
        }
        else if (val == "files")
        {
            _dispProgressBar  = false;
            _dispFilesPosting = true;
            qDebug() << "Display Files when start posting\n";
        }
        else if (val == "none")
        { // force it in case in the config file something was on
            _dispProgressBar  = false;
            _dispFilesPosting = false;
        }
    }

    if (parser.isSet(sOptionNames[Opt::MSG_ID]))
        sArticleIdSignature = escapeXML(parser.value(sOptionNames[Opt::MSG_ID])).toStdString();

    if (parser.isSet(sOptionNames[Opt::ARTICLE_SIZE]))
    {
        bool ok;
        int size = parser.value(sOptionNames[Opt::ARTICLE_SIZE]).toInt(&ok);
        if (ok)
            sArticleSize = size;
        else
        {
            _error(tr("You should give an integer for the article size (option -a)"),
                   ERROR_CODE::ERR_ARTICLE_SIZE);
            return false;
        }
    }

    if (parser.isSet(sOptionNames[Opt::NB_RETRY]))
    {
        bool ok;
        ushort nbRetry = parser.value(sOptionNames[Opt::NB_RETRY]).toUShort(&ok);
        if (ok)
            NntpArticle::setNbMaxRetry(nbRetry);
        else
        {
            _error(tr("You should give an unisgned integer for the number of retry for posting an Article (option -r)"),
                   ERROR_CODE::ERR_NB_RETRY);
            return false;
        }
    }



    // compression section
    if (parser.isSet(sOptionNames[Opt::TMP_DIR]))
        _tmpPath = parser.value(sOptionNames[Opt::TMP_DIR]);
    if (parser.isSet(sOptionNames[Opt::RAR_PATH]))
        _rarPath = parser.value(sOptionNames[Opt::RAR_PATH]);
    if (parser.isSet(sOptionNames[Opt::RAR_SIZE]))
    {
        bool ok;
        uint nb = parser.value(sOptionNames[Opt::RAR_SIZE]).toUInt(&ok);
        if (ok)
            _rarSize = nb;
    }
    if (parser.isSet(sOptionNames[Opt::RAR_MAX]))
    {
        _useRarMax = true;
        bool ok;
        uint nb = parser.value(sOptionNames[Opt::RAR_MAX]).toUInt(&ok);
        if (ok)
            _rarMax = nb;
    }
    if (parser.isSet(sOptionNames[Opt::PAR2_PCT]))
    {
        bool ok;
        uint nb = parser.value(sOptionNames[Opt::PAR2_PCT]).toUInt(&ok);
        if (ok)
        {
            _par2Pct = nb;
            if (nb > 0)
                _doPar2 = true;
        }
    }
    if (parser.isSet(sOptionNames[Opt::PAR2_PATH]))
    {
        QString val = parser.value(sOptionNames[Opt::PAR2_PATH]);
        if (!val.isEmpty())
        {
            QFileInfo fi(val);
            if (fi.exists() && fi.isFile() && fi.isExecutable())
                _par2Path = val;
        }
    }

    if (parser.isSet(sOptionNames[Opt::COMPRESS]))
        _doCompress = true;
    if (parser.isSet(sOptionNames[Opt::GEN_PAR2]))
        _doPar2 = true;
    if (parser.isSet(sOptionNames[Opt::GEN_NAME]))
        _genName = true;
    if (parser.isSet(sOptionNames[Opt::GEN_PASS]))
        _genPass = true;
    if (parser.isSet(sOptionNames[Opt::RAR_NO_ROOT_FOLDER]))
        _rarNoRootFolder = true;

    if (parser.isSet(sOptionNames[Opt::AUTO_COMPRESS]))
        _enableAutoCompress();

    if (_doPar2 && _par2Pct == 0 && _par2Args.isEmpty())
    {
        _error(tr("Error: can't generate par2 if the redundancy percentage is null or PAR2_ARGS is not provided...\nEither use --par2_pct or set PAR2_PCT or PAR2_ARGS in the config file."),
               ERROR_CODE::ERR_PAR2_ARGS);
        return false;
    }

    if (parser.isSet(sOptionNames[Opt::RAR_NAME]))
        _rarName = parser.value(sOptionNames[Opt::RAR_NAME]);
    if (parser.isSet(sOptionNames[Opt::RAR_PASS]))
    {
        _rarPass      = parser.value(sOptionNames[Opt::RAR_PASS]);
        _rarPassFixed = _rarPass;
    }

    if (parser.isSet(sOptionNames[Opt::LENGTH_NAME]))
    {
        bool ok;
        uint nb = parser.value(sOptionNames[Opt::LENGTH_NAME]).toUInt(&ok);
        if (ok)
            _lengthName = nb;
    }
    if (parser.isSet(sOptionNames[Opt::LENGTH_PASS]))
    {
        bool ok;
        uint nb = parser.value(sOptionNames[Opt::LENGTH_PASS]).toUInt(&ok);
        if (ok)
            _lengthPass = nb;
    }



    if (parser.isSet(sOptionNames[Opt::SERVER]))
    {
        _nntpServers.clear();
        QRegularExpression regExp(sNntpServerStrRegExp,  QRegularExpression::CaseInsensitiveOption);
        for (const QString &serverParam : parser.values(sOptionNames[Opt::SERVER]))
        {
            QRegularExpressionMatch match = regExp.match(serverParam);
            if (match.hasMatch())
            {
                bool    auth  = !match.captured(1).isEmpty();
                QString user  = match.captured(2);
                QString pass  = match.captured(3);
                QString host  = match.captured(4);
                ushort  port  = match.captured(5).toUShort();
                int     nbCon = match.captured(6).toInt();
                bool    ssl   = match.captured(7).isEmpty();
#ifdef __DEBUG__
                qDebug() << "NNTP Server: " << user << ":" << pass
                         << "@" << host << ":" << port << ":" << nbCon << ":" << ssl;
#endif
                NntpServerParams *server = new NntpServerParams(host,
                                                                port,
                                                                auth,
                                                                user.toStdString(),
                                                                pass.toStdString(),
                                                                nbCon,
                                                                ssl);
                _nntpServers << server;
            }
            else
            {
                _error(tr("Syntax error on server details for %1, the format should be: %2").arg(
                           serverParam).arg("(<user>:<pass>@@@)?<host>:<port>:<nbCons>:(no)?ssl"),
                       ERROR_CODE::ERR_SERVER_REGEX);
                return false;
            }
        }
    }

    // Server Section under
    // check if the server params are given in the command line
    if (parser.isSet(sOptionNames[Opt::HOST]))
    {
        QString host = parser.value(sOptionNames[Opt::HOST]);


        if (!parser.isSet(sOptionNames[Opt::SERVER]))
            _nntpServers.clear();
        NntpServerParams *server = new NntpServerParams(host);
        _nntpServers << server;

        if (parser.isSet(sOptionNames[Opt::SSL]))
        {
            server->useSSL = true;
            server->port = NntpServerParams::sDefaultSslPort;
        }

        if (parser.isSet(sOptionNames[Opt::PORT]))
        {
            bool ok;
            ushort port = parser.value(sOptionNames[Opt::PORT]).toUShort(&ok);
            if (ok)
                server->port = port;
            else
            {
                _error(tr("You should give an integer for the port (option -P)"),
                       ERROR_CODE::ERR_SERVER_PORT);
                return false;
            }
        }

        if (parser.isSet(sOptionNames[Opt::USER]))
        {
            server->auth = true;
            server->user = parser.value(sOptionNames[Opt::USER]).toStdString();

            if (parser.isSet(sOptionNames[Opt::PASS]))
                server->pass = parser.value(sOptionNames[Opt::PASS]).toStdString();

        }

        if (parser.isSet(sOptionNames[Opt::CONNECTION]))
        {
            bool ok;
            int nbCons = parser.value(sOptionNames[Opt::CONNECTION]).toInt(&ok);
            if (ok)
                server->nbCons = nbCons;
            else
            {
                _error(tr("You should give an integer for the number of connections (option -n)"),
                       ERROR_CODE::ERR_SERVER_CONS);
                return false;
            }
        }
    }



    QList<QFileInfo> filesToUpload;
    QStringList filesPath;
    for (const QString &filePath : parser.values(sOptionNames[Opt::INPUT]))
    {
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists() || !fileInfo.isReadable())
        {
            _error(tr("Error: the input file '%1' is not readable...").arg(parser.value("input")),
                   ERROR_CODE::ERR_INPUT_READ);
            return false;
        }
        else
        {
            if (_nzbName.isEmpty())
                setNzbName(fileInfo); // The first file will be used
            if (fileInfo.isFile())
            {
                filesToUpload << fileInfo;
                filesPath     << fileInfo.absoluteFilePath();
            }
            else
            {
                QDir dir(fileInfo.absoluteFilePath());
                for (const QFileInfo &subFile : dir.entryInfoList(QDir::Files, QDir::Name))
                {
                    if (subFile.isReadable())
                    {
                        filesToUpload << subFile;
                        filesPath     << subFile.absoluteFilePath();
                    }
                    else
                    {
                        _error(tr("Error: the input file '%1' is not readable...").arg(subFile.absoluteFilePath()),
                               ERROR_CODE::ERR_INPUT_READ);
                        return false;
                    }
                }
            }
        }
    }


    if (parser.isSet("o"))
    {
        QFileInfo nzb(parser.value(sOptionNames[Opt::OUTPUT]));
        setNzbName(nzb);
        _nzbPath = nzb.absolutePath();
    }

    if (_rarName.isEmpty())
        _rarName = _nzbName;

    if (_doCompress)
    {
        if (_genName)
            _rarName = randomPass(_lengthName);

        if (_genPass)
        {
            _rarPass = randomPass(_lengthPass);
            _meta.remove("password");
        }
    }

#ifdef __DEBUG__
    _dumpParams();
#endif


    if (filesToUpload.size())
    { // input files provided with -i
        QString nzbFilePath = nzbPath();
        if (!nzbFilePath.endsWith(".nzb"))
            nzbFilePath += ".nzb";
        startPostingJob(new PostingJob(this, nzbFilePath, filesToUpload, nullptr,
                                       _grpList, _groups, from(),
                                       _obfuscateArticles, _obfuscateFileName,
                                       _tmpPath, _rarPath,
                                       _rarSize, _useRarMax, _par2Pct,
                                       _doCompress, _doPar2, _rarName, _rarPass,
                                       _keepRar, _delAuto, false));
    }

    if (_autoDirs.size())
    {
        for (const QDir &dir : _autoDirs)
        {
            _cout << "===> Auto dir: " << dir.absolutePath() << "\n" << MB_FLUSH;
            for (const QFileInfo & fileInfo : dir.entryInfoList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot, QDir::Name))
                _post(fileInfo);
        }
    }


    return true;
}

void NgPost::setNzbName(const QFileInfo &fileInfo)
{
    _nzbName = fileInfo.isDir() ? QDir(fileInfo.absoluteFilePath()).dirName() : fileInfo.completeBaseName();
    if (_removeAccentsOnNzbFileName)
    {
        _nzbName.replace(QRegExp("[ÀÁÂÃÄÅ]"), "A");
        _nzbName.replace(QRegExp("[àáâãäå]"), "a");
        _nzbName.replace("Ç","C");
        _nzbName.replace("ç","c");
        _nzbName.replace(QRegExp("[ÈÉÊË]"),   "E");
        _nzbName.replace(QRegExp("[èéêë]"),   "e");
        _nzbName.replace(QRegExp("[ÌÍÎÏ]"),   "I");
        _nzbName.replace(QRegExp("[ìíîï]"),   "i");
        _nzbName.replace("Ñ","N");
        _nzbName.replace("ñ","n");
        _nzbName.replace(QRegExp("[ÒÓÔÕÖØ]"), "O");
        _nzbName.replace(QRegExp("[òóôõöø]"), "o");
        _nzbName.replace(QRegExp("[ÙÚÛÜ]"),   "U");
        _nzbName.replace(QRegExp("[ùúûü]"),   "u");
        _nzbName.replace(QRegExp("[ÿý]"),     "y");
        _nzbName.replace(QRegExp("[^A-Za-z0-9\\.,_\\-\\(\\)\\[\\]\\{\\}!#&'\\+ ]"), "");
    }
}

QString NgPost::nzbPath(const QString &monitorFolder)
{
    if (monitorFolder.isEmpty())
        return nzbPath();
    else
    {
        QString path;
        if (_nzbPath.isEmpty())
            path = monitorFolder;
        else
            path = QString("%1/%2").arg(_nzbPath).arg(monitorFolder);
        QDir dir(path);
        if (!dir.exists())
            QDir().mkdir(path);

        return QString("%1/%2").arg(path).arg(_nzbName);
    }
}


QString NgPost::_parseConfig(const QString &configPath)
{
    QString err;
    QFileInfo fileInfo(configPath);
    if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable())
        err = tr("The config file '%1' is not readable...").arg(configPath);

    QFile file(fileInfo.absoluteFilePath());
    if (file.open(QIODevice::ReadOnly))
    {
        NntpServerParams *serverParams = nullptr;
        QTextStream stream(&file);
        while (!stream.atEnd())
        {
            QString line = stream.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#') || line.startsWith('/'))
                continue;
            else if (line == "[server]")
            {
                serverParams = new NntpServerParams();
                _nntpServers << serverParams;
            }
            else
            {
                QStringList args = line.split('=');
                if (args.size() >= 2)
                {
                    QString opt = args.at(0).trimmed().toLower(),
                            val = args.at(1).trimmed();
                    bool ok = false;
                    if (opt == sOptionNames[Opt::THREAD])
                    {
                        int nb = val.toInt(&ok);
                        if (ok)
                        {
                            if (nb < 1)
                                _nbThreads = 1;
                            else
                                _nbThreads = nb;
                        }
                    }
                    else if (opt == sOptionNames[Opt::NZB_PATH])
                    {
                        QFileInfo nzbFI(val);
                        if (nzbFI.exists() && nzbFI.isDir() && nzbFI.isWritable())
                        {
                            _nzbPath     = val;
                            _nzbPathConf = val;
                        }
                        else
                            err += tr("the nzbPath '%1' is not writable...\n").arg(val);
                    }
                    else if (opt == sOptionNames[Opt::NZB_UPLOAD_URL])
                    {
                        _urlNzbUploadStr = val;
                        for (int i = 2; i < args.size(); ++i)
                            _urlNzbUploadStr += QString("=%1").arg(args.at(i).trimmed());
                        _urlNzbUpload    = new QUrl(_urlNzbUploadStr);
                        QStringList allowedProtocols = {"ftp", "http", "https"};
                        if (!allowedProtocols.contains(_urlNzbUpload->scheme()))
                        {
                            err += tr("Unsupported protocol for NZB_UPLOAD_URL (%1). You can only use: %2\n").arg(
                                        _urlNzbUpload->toString(QUrl::RemoveUserInfo)).arg(allowedProtocols.join(", "));
                            delete _urlNzbUpload;
                            _urlNzbUpload = nullptr;
                        }
                    }
                    else if (opt == sOptionNames[Opt::RESUME_WAIT])
                    {
                        ushort nb = val.toUShort(&ok);
                        if (ok && nb > sDefaultResumeWaitInSec)
                            _waitDurationBeforeAutoResume = nb;
                    }
                    else if (opt == sOptionNames[Opt::NO_RESUME_AUTO])
                    {
                        val = val.toLower();
                        if (val == "true" || val == "on" || val == "1")
                            _tryResumePostWhenConnectionLost = false;
                    }
                    else if (opt == sOptionNames[Opt::PREPARE_PACKING])
                    {
                        val = val.toLower();
                        if (val == "true" || val == "on" || val == "1")
                            _preparePacking = true;
                    }
                    else if (opt == sOptionNames[Opt::SOCK_TIMEOUT])
                    {
                        int nb = val.toInt(&ok);
                        if (ok)
                        {
                            int timeout = nb *1000;
                            if (timeout > sMinSocketTimeOut)
                                _socketTimeOut = timeout;
                        }
                    }
                    else if (opt == sOptionNames[Opt::MONITOR_FOLDERS])
                    {
                        val = val.toLower();
                        if (val == "true" || val == "on" || val == "1")
                            _monitor_nzb_folders = true;
                    }
                    else if (opt == sOptionNames[Opt::MONITOR_IGNORE_DIR])
                    {
                        val = val.toLower();
                        if (val == "true" || val == "on" || val == "1")
                            _monitorIgnoreDir = true;
                    }
                    else if (opt == sOptionNames[Opt::NZB_RM_ACCENTS])
                    {
                        val = val.toLower();
                        if (val == "true" || val == "on" || val == "1")
                            _removeAccentsOnNzbFileName = true;
                    }
                    else if (opt == sOptionNames[Opt::AUTO_CLOSE_TABS])
                    {
                        val = val.toLower();
                        if (val == "true" || val == "on" || val == "1")
                            _autoCloseTabs = true;
                    }
                    else if (opt == sOptionNames[Opt::MONITOR_EXT])
                    {
                        for (const QString &extension : val.split(","))
                            _monitorExtensions << extension.trimmed();
                    }
                    else if (opt == sOptionNames[Opt::OBFUSCATE])
                    {
                        if (val.toLower().startsWith("article"))
                        {
                            _obfuscateArticles = true;
                            qDebug() << "Do article obfuscation (the subject of each Article will be a UUID)\n";
                        }
                    }
                    else if (opt == sOptionNames[Opt::DISP_PROGRESS])
                    {
                        val = val.toLower();
                        if (val == "bar")
                        {
                            _dispProgressBar = true;
                            qDebug() << "Display progressbar bar\n";
                        }
                        else if (val == "files")
                        {
                            _dispFilesPosting = true;
                            qDebug() << "Display Files when start posting\n";
                        }
                    }
                    else if (opt == sOptionNames[Opt::MSG_ID])
                    {
                        sArticleIdSignature = val.toStdString();
                    }
                    else if (opt == sOptionNames[Opt::ARTICLE_SIZE])
                    {
                        int nb = val.toInt(&ok);
                        if (ok)
                            sArticleSize = nb;
                    }
                    else if (opt == sOptionNames[Opt::NB_RETRY])
                    {
                        ushort nb = val.toUShort(&ok);
                        if (ok)
                            NntpArticle::setNbMaxRetry(nb);
                    }
                    else if (opt == sOptionNames[Opt::FROM])
                    {
                        QRegularExpression email("\\w+@\\w+\\.\\w+");
                        if (!email.match(val).hasMatch())
                            val += "@ngPost.com";
                        val = escapeXML(val);
                        _from = val.toStdString();
                        _saveFrom = true;
                    }
                    else if (opt == sOptionNames[Opt::GEN_FROM])
                    {
                        val = val.toLower();
                        if (val == "true" || val == "on" || val == "1")
                        {
                            _genFrom = true;
                            if (_debug)
                                _cout << tr("Generate new random poster for each post") << "\n" << MB_FLUSH;
                        }
                    }
                    else if (opt == sOptionNames[Opt::GROUPS])
                        updateGroups(val);

                    else if (opt == sOptionNames[Opt::LANG])
                        changeLanguage(val.toLower());

                    else if (opt == sOptionNames[Opt::SHUTDOWN_CMD])
                        _shutdownCmd = val;

                    else if (opt == sOptionNames[Opt::NZB_POST_CMD])
                        _nzbPostCmd = val;

                    else if (opt == sOptionNames[Opt::INPUT_DIR])
                        _inputDir = val;

                    else if (opt == sOptionNames[Opt::POST_HISTORY])
                    {
                        _postHistoryFile = val;
                        QFileInfo fi(val);
                        if (fi.isDir())
                            err += tr("the post history '%1' can't be a directory...\n").arg(val);
                        else
                        {
                            if (fi.exists())
                            {
                                if (!fi.isWritable())
                                    err += tr("the post history file '%1' is not writable...\n").arg(val);
                            }
                            else if (!QFileInfo(fi.absolutePath()).isWritable())
                                err += tr("the post history file '%1' is not writable...\n").arg(val);
                        }
                    }

                    else if (opt == sOptionNames[Opt::FIELD_SEPARATOR])
                        _historyFieldSeparator = val;

                    // compression section
                    else if (opt == sOptionNames[Opt::TMP_DIR])
                        _tmpPath = val;
                    else if (opt == sOptionNames[Opt::RAR_PATH])
                        _rarPath = val;
                    else if (opt == sOptionNames[Opt::RAR_PASS])
                    {
                        _rarPassFixed = val;
                        _rarPass      = val;
                    }
                    else if (opt == sOptionNames[Opt::RAR_EXTRA])
                        _rarArgs = val;
                    else if (opt == sOptionNames[Opt::RAR_SIZE])
                    {
                        uint nb = val.toUInt(&ok);
                        if (ok)
                            _rarSize = nb;
                    }
                    else if (opt == sOptionNames[Opt::RAR_MAX])
                    {
                        _useRarMax = true;
                        uint nb = val.toUInt(&ok);
                        if (ok)
                            _rarMax = nb;
                    }
                    else if (opt == sOptionNames[Opt::KEEP_RAR])
                    {
                        val = val.toLower();
                        if (val == "true" || val == "on" || val == "1")
                            _keepRar = true;
                    }
                    else if (opt == sOptionNames[Opt::AUTO_COMPRESS])
                    {
                        val = val.toLower();
                        if (val == "true" || val == "on" || val == "1")
                            _enableAutoCompress();
                    }
                    else if (opt == sOptionNames[Opt::RAR_NO_ROOT_FOLDER])
                    {
                        val = val.toLower();
                        if (val == "true" || val == "on" || val == "1")
                            _rarNoRootFolder = true;
                    }

                    else if (opt == sOptionNames[Opt::PAR2_PCT])
                    {
                        uint nb = val.toUInt(&ok);
                        if (ok)
                            _par2Pct = nb;
                    }
                    else if (opt == sOptionNames[Opt::PAR2_PATH])
                    {
                        if (!val.isEmpty())
                        {
                            QFileInfo fi(val);
                            if (fi.exists() && fi.isFile() && fi.isExecutable())
                            {
                                _par2Path       = val;
                                _par2PathConfig = val;
                            }
                        }
                    }
                    else if (opt == sOptionNames[Opt::PAR2_ARGS])
                        _par2Args = val;
                    else if (opt == sOptionNames[Opt::LENGTH_NAME])
                    {
                        uint nb = val.toUInt(&ok);
                        if (ok)
                            _lengthName = nb;
                    }
                    else if (opt == sOptionNames[Opt::LENGTH_PASS])
                    {
                        uint nb = val.toUInt(&ok);
                        if (ok)
                            _lengthPass = nb;
                    }





                    // Server Section under
                    else if (opt == sOptionNames[Opt::HOST])
                    {
                        serverParams->host = val;
                    }
                    else if (opt == sOptionNames[Opt::PORT])
                    {
                        ushort nb = val.toUShort(&ok);
                        if (ok)
                            serverParams->port = nb;

                    }
                    else if (opt == sOptionNames[Opt::SSL])
                    {
                        val = val.toLower();
                        if (val == "true" || val == "on" || val == "1")
                        {
                            serverParams->useSSL = true;
                            if (serverParams->port == NntpServerParams::sDefaultPort)
                                serverParams->port = NntpServerParams::sDefaultSslPort;
                        }
                    }
                    else if (opt == sOptionNames[Opt::ENABLED])
                    {
                        val = val.toLower();
                        if (val == "true" || val == "on" || val == "1")
                            serverParams->enabled = true;
                        else
                            serverParams->enabled = false;
                    }
                    else if (opt == sOptionNames[Opt::USER])
                    {
                        serverParams->user = val.toStdString();
                        serverParams->auth = true;
                    }
                    else if (opt == sOptionNames[Opt::PASS])
                    {
                        serverParams->pass = val.toStdString();
                        serverParams->auth = true;
                    }
                    else if (opt == sOptionNames[Opt::CONNECTION])
                    {
                        int nb = val.toInt(&ok);
                        if (ok)
                            serverParams->nbCons = nb;
                    }               
                }
            }
        }
        file.close();
    }

    if (err.isEmpty() && !_postHistoryFile.isEmpty())
    {
        QFile file(_postHistoryFile);
        if (!file.exists() && file.open(QIODevice::WriteOnly|QIODevice::Text))
        {
            QTextStream stream(&file);
            stream << tr("date")
                   << _historyFieldSeparator << tr("nzb name")
                   << _historyFieldSeparator << tr("size")
                   << _historyFieldSeparator << tr("avg. speed")
                   << _historyFieldSeparator << tr("archive name")
                   << _historyFieldSeparator << tr("archive pass")
                   << _historyFieldSeparator << tr("groups")
                   << _historyFieldSeparator << tr("from") << "\n";

            file.close();
        }
    }

    return err;
}

void NgPost::_syntax(char *appName)
{
    QString app = QFileInfo(appName).fileName();
    _cout << desc() << "\n"
          << tr("Syntax: ") << app << " (options)* (-i <file or folder> | --auto <folder> | --monitor <folder>)+\n";
    for (const QCommandLineOption & opt : sCmdOptions)
    {
        if (opt.valueName() == sOptionNames[Opt::SERVER])
            _cout << "\n// " << tr("you can provide servers in one string using -S and/or split the parameters for ONE SINGLE server (this will overwrite the configuration file)") << "\n";
        else if (opt.valueName() == sOptionNames[Opt::TMP_DIR])
            _cout << "\n// " << tr("for compression and par2 support") << "\n";
        else if (opt.valueName() == sOptionNames[Opt::AUTO_DIR])
            _cout << "\n// " << tr("automated posting (scanning and/or monitoring)") << "\n";
        else if (opt.valueName() == sOptionNames[Opt::INPUT])
            _cout << "\n// " << tr("quick posting (several files/folders)") << "\n";
        else if (opt.valueName() == sOptionNames[Opt::OBFUSCATE])
            _cout << "\n// " << tr("general options") << "\n";

        if (opt.names().size() == 1)
            _cout << QString("\t--%1: %2\n").arg(opt.names().first(), -17).arg(tr(opt.description().toLocal8Bit().constData()));
        else
            _cout << QString("\t-%1: %2\n").arg(opt.names().join(" or --"), -18).arg(tr(opt.description().toLocal8Bit().constData()));
    }

    _cout << "\n" << tr("Examples:") << "\n"
          << "  - " << tr("with monitoring") << ": " << app << " --monitor --rm_posted /Downloads/testNgPost --compress --gen_par2 --gen_name --gen_pass --rar_size 42 --disp_progress files\n"
          << "  - " << tr("with auto post")  << ": " << app << " --auto /Downloads/testNgPost --compress --gen_par2 --gen_name --gen_pass --rar_size 42 --disp_progress files\n"
          << "  - " << tr("with compression, filename obfuscation, random password and par2") << ": " << app << " -i /tmp/file1 -i /tmp/folder1 -o /nzb/myPost.nzb --compress --gen_name --gen_pass --gen_par2\n"
          << "  - " << tr("with config file") << ": " << app << " -c ~/.ngPost -m \"password=qwerty42\" -f ngPost@nowhere.com -i /tmp/file1 -i /tmp/file2 -i /tmp/folderToPost1 -i /tmp/folderToPost2\n"
          << "  - " << tr("with all params") << ":  " << app << " -t 1 -m \"password=qwerty42\" -m \"metaKey=someValue\" -h news.newshosting.com -P 443 -s -u user -p pass -n 30 -f ngPost@nowhere.com \
 -g \"alt.binaries.test,alt.binaries.test2\" -a 64000 -i /tmp/folderToPost -o /tmp/folderToPost.nzb\n"
          << "\n"
          << tr("If you don't provide the output file (nzb file), we will create it in the nzbPath with the name of the first file or folder given in the command line.") << "\n"
          << tr("so in the second example above, the nzb would be: /tmp/file1.nzb") << "\n"
          << MB_FLUSH;
}

QString NgPost::parseDefaultConfig()
{
#if defined(WIN32) || defined(__MINGW64__)
    QString conf = sDefaultConfig;
#else
    QString conf = QString("%1/%2").arg(getenv("HOME")).arg(sDefaultConfig);
#endif
    QString err;
    QFileInfo defaultConf(conf);
    if (defaultConf.exists() && defaultConf.isFile())
    {
        qCritical() << "Using default config file: " << conf;
        err = _parseConfig(conf);
    }
    else
        qCritical() << "The default config file doesn't exist: " << conf;

    return err;
}

bool NgPost::startPostingJob(PostingJob *job)
{
    if (_hmi)
    {
        connect(job, &PostingJob::articlesNumber, _hmi, &MainWindow::onSetProgressBarRange,  Qt::QueuedConnection);
        if (!job->widget())
            connect(job, &PostingJob::postingStarted,  _hmi->autoWidget(), &AutoPostWidget::onMonitorJobStart);
    }

    if (_activeJob)
    {
        _pendingJobs << job;
        if (_preparePacking)
        {
            if (_activeJob->isPacked() && !_packingJob)
                _prepareNextPacking();
        }
        return false;
    }
    else
    {
        _activeJob = job;
        emit job->startPosting(true);
        return true;
    }
}

#ifdef __DEBUG__
void NgPost::_dumpParams() const
{
    QString servers;
    for (NntpServerParams *srv : _nntpServers)
        servers += srv->str() + " ";
    qDebug() << "[NgPost::_dumpParams]>>>>>>>>>>>>>>>>>>\n"
             << "nb Servers: " << _nntpServers.size() << ": " << servers

             << "\n\nnbThreads: " << _nbThreads << " nb Inputs: " << _nbFiles
             << ", nzbPath: " << _nzbPath << ", nzbName" << _nzbName

             << "\ninputDir: " << _inputDir << ", autoDelete: " << _delAuto
             << ", auto_compress:" << _autoCompress << ", autoClose: " << _autoCloseTabs

             << "\n\nfrom: " << _from.c_str() << ", genFrom: " << _genFrom << ", saveFrom: " << _saveFrom
             << ", groups: " << _groups.c_str()
             << "\narticleSize: " << sArticleSize
             << ", obfuscate articles: " << _obfuscateArticles
             << ", disp progress bar: " << _dispProgressBar
             << ", disp posting files: " << _dispFilesPosting

             << "\n\ncompression settings: <tmp_path: " << _tmpPath << ">"
             << ", <rar_path: " << _rarPath << ">"
             << ", <rar_size: " << _rarSize << ">"
             << "\n<par2_pct: " << _par2Pct << ">"
             << ", <par2_path: " << _par2Path << ">"
             << ", <par2_pathCfg: " << _par2PathConfig << ">"
             << ", <par2_args: " << _par2Args << ">"
             << "\n\ncompress: " << _doCompress << ", doPar2: " << _doPar2
             << ", gen_name: " << _genName << ", genPass: " << _genPass
             << "\nrarName: " << _rarName << ", rarPass: " << _rarPass
             << ", lengthName: " << _lengthName << ", lengthPass: " << _lengthPass
             << "\n[NgPost::_dumpParams]<<<<<<<<<<<<<<<<<<\n";
}
#endif


void NgPost::_showVersionASCII() const
{
    _cout << sNgPostASCII
          << "                          v" << sVersion << "\n\n" << MB_FLUSH;
}

void NgPost::saveConfig()
{
#if defined(WIN32) || defined(__MINGW64__)
    QString conf = sDefaultConfig;
#else
    QString conf = QString("%1/%2").arg(getenv("HOME")).arg(sDefaultConfig);
#endif

    QFile file(conf);
    if (file.open(QIODevice::WriteOnly|QIODevice::Text))
    {
        QTextStream stream(&file);
        stream << tr("# ngPost configuration file") << "\n"
               << "#\n"
               << "#\n"
               << "\n"
               << tr("## Lang for the app. Currently supported: EN, FR, ES, DE") << "\n"
               << "lang = " << _lang.toUpper() << "\n"
               << "\n"
               << tr("## destination folder for all your nzb") << "\n"
               << tr("## if you don't put anything, the nzb will be generated in the folder of ngPost on Windows and in /tmp on Linux") << "\n"
               << tr("## this will be overwritten if you use the option -o with the full path of the nzb") << "\n"
               << "nzbPath  = " << (_nzbPath.isEmpty() ? _nzbPathConf : _nzbPath) << "\n"
               << "\n"
               << tr("## Shutdown command to switch off the computer when ngPost is done with all its queued posting") << "\n"
               << tr("## this should mainly used with the auto posting") << "\n"
               << tr("## you could use whatever script instead (like to send a mail...)") << "\n"
               << tr("#SHUTDOWN_CMD = shutdown /s /f /t 0  (Windows)") << "\n"
               << tr("#SHUTDOWN_CMD = sudo -n /sbin/poweroff  (Linux, make sure poweroff has sudo rights without any password or change the command)") << "\n"
               << tr("#SHUTDOWN_CMD = sudo -n shutdown -h now (MacOS, same make sure you've sudo rights)") << "\n"
               << "SHUTDOWN_CMD = " << _shutdownCmd << "\n"
               << "\n"
               << tr("## upload the nzb to a specific URL") << "\n"
               << tr("## only http, https or ftp (neither ftps or sftp are supported)") << "\n"
               << tr("#NZB_UPLOAD_URL = ftp://user:pass@url_or_ip:21") << "\n"
               << (_urlNzbUploadStr.isEmpty() ? QString() : QString("NZB_UPLOAD_URL = %1\n").arg(_urlNzbUpload->url()))
               << "\n"
               << tr("## launch a command or script at the end of each Post (cf examples)") << "\n"
               << tr("## the full path of the nzb file is provided in the %1 placeholder (Qt style)") << "\n"
               << "#NZB_POST_CMD = scp %1 myBox.com:~/nzbs/\n"
               << "#NZB_POST_CMD = zip ~/nzbZip/$(basename %1).zip %1; rm %1\n"
               << "#NZB_POST_CMD = ~/scripts/postNZB.sh %1\n"
               << (_nzbPostCmd.isEmpty() ? "" : QString("NZB_POST_CMD = %1\n").arg(_nzbPostCmd))
               << "\n"
               << tr("## nzb files are normally all created in nzbPath") << "\n"
               << tr("## but using this option, the nzb of each monitoring folder will be stored in their own folder (created in nzbPath)") << "\n"
               << (_monitor_nzb_folders  ? "" : "#") << "MONITOR_NZB_FOLDERS = true\n"
               << "\n"
               << tr("## for monitoring, extension file filter for new incoming files (coma separated, no dot)") << "\n"
               << (_monitorExtensions.isEmpty()  ? "#" : "") << "MONITOR_EXTENSIONS = "
               << (_monitorExtensions.isEmpty() ? "mkv,mp4,avi,zip,tar,gz,iso" : _monitorExtensions.join(",")) << "\n"
               << "\n"
               << tr("## for monitoring, ignore new incoming folders") << "\n"
               << (_monitorIgnoreDir  ? "" : "#") << "MONITOR_IGNORE_DIR = true\n"
               << "\n\n"
               << tr("## Default folder to open to select files from the HMI") << "\n"
               << "inputDir = " << _inputDir << "\n"
               << "\n"
               << tr("## History posting file") << "\n"
               << tr("## each succesful post will append a line with the date, the file name, the archive name, the password...") << "\n"
               << (_postHistoryFile.isEmpty()  ? "#" : "") <<"POST_HISTORY = "
               << (_postHistoryFile.isEmpty()  ? "/nzb/ngPost_history.csv" : _postHistoryFile) << "\n"
               << "\n"
               << tr("## Character used to separate fields in the history posting file") << "\n"
               << (_historyFieldSeparator == sDefaultFieldSeparator ? "#" : "") << "FIELD_SEPARATOR = " << _historyFieldSeparator << "\n"
               << "\n"
               << "GROUPS   = " << _groups.c_str() << "\n"
               << "\n"
               << "\n"
               << tr("## uncomment the next line if you want a fixed uploader email (in the nzb and in the header of each articles)") << "\n"
               << tr("## if you let it commented, we'll generate ONE random email for all the posts of the session") << "\n"
               << (_saveFrom  ? "" : "#") << "FROM = " << _from.c_str() << "\n"
               << "\n"
               << tr("## Generate new random poster for each post (--auto or --monitor)") << "\n"
               << tr("## if this option is set the FROM email just above will be ignored") << "\n"
               << (_genFrom  ? "" : "#") << "GEN_FROM = true" << "\n"
               << "\n"
               << "\n"
               << tr("## uncomment the next line to limit the number of threads,  (by default it'll use the number of cores)") << "\n"
               << tr("## all the connections are spread equally on those posting threads") << "\n"
               << "thread  =  " << _nbThreads << "\n"
               << "\n"
               << "\n"
               << tr("## How to display progressbar in command line: NONE, BAR, FILES") << "\n"
               << (_dispProgressBar  ? "" : "#") << "DISP_Progress = BAR\n"
               << (_dispFilesPosting ? "" : "#") << "DISP_Progress = FILES\n"
               << "\n"
               << "\n"
               << tr("## suffix of the msg_id for all the articles (cf nzb file)") << "\n"
               << (sArticleIdSignature == "ngPost" ? "#msg_id  =  ngPost\n" : QString("msg_id  =  %1\n").arg(sArticleIdSignature.c_str()))
               << "\n"
               << tr("## article size (default 700k)") << "\n"
               << "article_size = " << sArticleSize << "\n"
               << "\n"
               << tr("## number of retry to post an Article in case of failure (probably due to an already existing msg-id)") << "\n"
               << "retry = " << NntpArticle::nbMaxTrySending() << "\n"
               << "\n"
               << "\n"
               << tr("## uncomment the following line to obfuscate the subjects of each Article") << "\n"
               << tr("## /!\\ CAREFUL you won't find your post if you lose the nzb file /!\\") << "\n"
               << (_obfuscateArticles ? "" : "#") << "obfuscate = article\n"
               << "\n"
               << tr("## remove accents and special characters from the nzb file names") << "\n"
               << (_removeAccentsOnNzbFileName  ? "" : "#") << "NZB_RM_ACCENTS = true\n"
               << "\n"
               << tr("## close Quick Post Tabs when posted successfully (for the GUI)") << "\n"
               << (_autoCloseTabs  ? "" : "#") << "AUTO_CLOSE_TABS = true\n"
               << "\n"
               << "\n"
               << tr("## Time to wait (seconds) before trying to resume a Post automatically in case of loss of Network (min: %1)").arg(
                      sDefaultResumeWaitInSec) << "\n"
               << "RESUME_WAIT = " << _waitDurationBeforeAutoResume << "\n"
               << "\n"
               << tr("## By default, ngPost tries to resume a Post if the network is down.") << "\n"
               << tr("## it won't stop trying until the network is back and the post is finished properly") << "\n"
               << tr("## you can disable this feature and thus stop a post when you loose the network") << "\n"
               << (_tryResumePostWhenConnectionLost  ? "#" : "") << "NO_RESUME_AUTO = true\n"
               << "\n"
               << tr("## if there is no activity on a connection it will be closed and restarted") << "\n"
               << tr("## The duration is in second, default: %1, min: %2)").arg(sDefaultSocketTimeOut/1000).arg(sMinSocketTimeOut/1000) << "\n"
               << "SOCK_TIMEOUT = " << _socketTimeOut / 1000 << "\n"
               << "\n"
               << tr("## when several Posts are queued, prepare the packing of the next Post while uploading the current one") << "\n"
               << (_preparePacking ? "" : "#") << "PREPARE_PACKING = true" << "\n"
               << "\n"
               << "\n"
               << "\n"
               << "\n"
               << "##############################################################\n"
               << "##           Compression and par2 section                   ##\n"
               << "##############################################################\n"
               << "\n"
               << tr("## Auto compression for all posts with random archive name, password and par2 generation") << "\n"
               << (_autoCompress  ? "" : "#") << "AUTO_COMPRESS = true\n"
               << "\n"
               << tr("## use the same Password for all your Posts using compression") << "\n"
               << (_hmi?(_hmi->useFixedPassword()?"":"#"):(_rarPassFixed.isEmpty()  ? "#" : ""))
               << "RAR_PASS = " << (_rarPassFixed.isEmpty()  ? "yourPassword" : _rarPassFixed) << "\n"
               << "\n"
               << tr("## temporary folder where the compressed files and par2 will be stored") << "\n"
               << tr("## so we can post directly a compressed (obfuscated or not) archive of the selected files") << "\n"
               << tr("## /!\\ The directory MUST HAVE WRITE PERMISSION /!\\") << "\n"
               << tr("## this is set for Linux environment, Windows users MUST change it") << "\n"
               << "TMP_DIR = " << _tmpPath << "\n"
               << "\n"
               << tr("## RAR or 7zip absolute file path (external application)") << "\n"
               << tr("## /!\\ The file MUST EXIST and BE EXECUTABLE /!\\") << "\n"
               << tr("## this is set for Linux environment, Windows users MUST change it") << "\n"
               << "RAR_PATH = " << _rarPath << "\n"
               << "\n"
               << tr("## RAR EXTRA options (the first 'a' and '-idp' will be added automatically)") << "\n"
               << tr("## -hp will be added if you use a password with --gen_pass, --rar_pass or using the HMI") << "\n"
               << tr("## -v42m will be added with --rar_size or using the HMI") << "\n"
               << tr("## you could change the compression level, lock the archive, add redundancy...") << "\n"
               << "#RAR_EXTRA = -ep1 -m0 -k -rr5p\n"
               << "#RAR_EXTRA = -mx0 -mhe=on   (for 7-zip)\n"
               << (_rarArgs.isEmpty() ? "" : QString("RAR_EXTRA = %1\n").arg(_rarArgs) )
               << "\n"
               << tr("## size in MB of the RAR volumes (0 by default meaning NO split)") << "\n"
               << tr("## feel free to change the value or to comment the next line if you don't want to split the archive") << "\n"
               << "RAR_SIZE = " << _rarSize << "\n"
               << "\n"
               << tr("## maximum number of archive volumes") << "\n"
               << tr("## we'll use RAR_SIZE except if it genereates too many volumes") << "\n"
               << tr("## in that case we'll update rar_size to be <size of post> / rar_max") << "\n"
               << "RAR_MAX = " << _rarMax << "\n"
               << "\n"
               << tr("##  keep rar folder after posting (otherwise it is automatically deleted uppon successful post)") << "\n"
               << (_keepRar  ? "" : "#") << "KEEP_RAR = true\n"
               << "\n"
               << "## " << tr("Remove root (parent) folder when compressing Folders using RAR") << "\n"
               << (_rarNoRootFolder  ? "" : "#") << "RAR_NO_ROOT_FOLDER = true\n"
               << "\n"
               << tr("## par2 redundancy percentage (0 by default meaning NO par2 generation)") << "\n"
               << "PAR2_PCT = " << _par2Pct << "\n"
               << "\n"
               << tr("## par2 (or alternative) absolute file path") << "\n"
               << tr("## this is only useful if you compile from source (as par2 is included on Windows and the AppImage)") << "\n"
               << tr("## or if you wish to use an alternative to par2 (for exemple Multipar on Windows)") << "\n"
               << tr("## (in that case, you may need to set also PAR2_ARGS)") << "\n";
        if (!_par2PathConfig.isEmpty())
            stream << "PAR2_PATH = " << _par2PathConfig << "\n";
#if defined(WIN32) || defined(__MINGW64__)
        stream << "#PAR2_PATH = <your_path>parpar.exe\n"
               << "#PAR2_PATH = <your_path>par2j64.exe\n";
#else
        stream << "#PAR2_PATH = /usr/bin/par2\n"
               << "#PAR2_PATH = <your_path>/parpar\n";
#endif
        stream << "\n"
               << tr("## fixed parameters for the par2 (or alternative) command") << "\n"
               << tr("## you could for exemple use Multipar on Windows") << "\n"
               << "#PAR2_ARGS = -s5M -r1n*0.6 -m2048M -p1l --progress stdout -q   (for parpar)\n"
               << "#PAR2_ARGS = c -l -m1024 -r8 -s768000                 (for par2cmdline)\n"
               << "#PAR2_ARGS = create /rr8 /lc40 /lr /rd2               (for Multipar)\n"
               << (_par2Args.isEmpty() ? "" : QString("PAR2_ARGS = %1\n").arg(_par2Args))
               << "\n"
               << "\n"
               << tr("## length of the random generated archive's file name") << "\n"
               << "LENGTH_NAME = " << _lengthName << "\n"
               << "\n"
               << tr("## length of the random archive's passsword") << "\n"
               << "LENGTH_PASS = "<< _lengthPass << "\n"
               << "\n"
               << "\n"
               << "\n"
               << "\n"
               << "##############################################################\n"
               << "##                   servers section                        ##\n"
               << "##############################################################\n"
               << "\n";

        for (NntpServerParams *param : _nntpServers)
        {
            stream << "[server]\n"
                   << "host = " << param->host << "\n"
                   << "port = " << param->port << "\n"
                   << "ssl  = " << (param->useSSL ? "true" : "false") << "\n"
                   << "user = " << param->user.c_str() << "\n"
                   << "pass = " << param->pass.c_str() << "\n"
                   << "connection = " << param->nbCons << "\n"
                   << "enabled = " << (param->enabled ? "true":"false") << "\n"
                   << "\n\n";
        }
        stream << tr("## You can add as many server if you have several providers by adding other \"server\" sections") << "\n"
               << "#[server]\n"
               << "#host = news.otherprovider.com\n"
               << "#port = 563\n"
               << "#ssl  = true\n"
               << "#user = myOtherUser\n"
               << "#pass = myOtherPass\n"
               << "#connection = 15\n"
               << "#enabled = false\n"
               << "\n";


        _log(tr("the config '%1' file has been updated").arg(conf));
        file.close();
    }
    else
        _error(tr("Error: Couldn't write default configuration file: %1").arg(conf));

}

void NgPost::setDelFilesAfterPosted(bool delFiles)
{
    // Thread safe, only main thread is using this (NgPost or HMI)
    if (_activeJob)
        _activeJob->setDelFilesAfterPosted(delFiles);
    for (PostingJob *job : _pendingJobs)
        job->setDelFilesAfterPosted(delFiles);
}

void NgPost::addMonitoringFolder(const QString &dirPath)
{
    if (_folderMonitor)
        _folderMonitor->addFolder(dirPath);
}

const QString NgPost::sNgPostASCII = QString("\
                   __________               __\n\
       ____    ____\\______   \\____  _______/  |_\n\
      /    \\  / ___\\|     ___/  _ \\/  ___/\\   __\\\n\
     |   |  \\/ /_/  >    |  (  <_> )___ \\  |  |\n\
     |___|  /\\___  /|____|   \\____/____  > |__|\n\
          \\//_____/                    \\/\n\
");

