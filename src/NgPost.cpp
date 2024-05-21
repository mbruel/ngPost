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

#include "NgPost.h"
#include <cmath>
#include <iostream>

#ifdef __USE_HMI__
#  include <QApplication>
#  include <QMessageBox>
#else
#  include <QCoreApplication>
#endif

#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTextStream>
#include <QThread>

#include "FileUploader.h"
#include "FoldersMonitorForNewFiles.h"
#include "Migration.h"
#include "nntp/NntpArticle.h"
#include "nntp/NntpServerParams.h"
#include "NzbCheck.h"
#include "PostingJob.h"
#include "utils/Database.h"
#include "utils/Macros.h" // MB_FLUSH
#include "utils/NgCmdLineLoader.h"
#include "utils/NgConfigLoader.h"
#include "utils/NgTools.h"

#ifdef __USE_HMI__
#  include "hmi/AutoPostWidget.h"
#  include "hmi/MainWindow.h"
#  include "hmi/PostingWidget.h"
#endif

#if __DEBUG__ && QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
#  include <QNetworkConfigurationManager>
#endif

using namespace NgConf;

NgPost::NgPost(int &argc, char *argv[])
    : QObject()
    , CmdOrGuiApp(argc, argv)
    , _postingParams(new MainParams)
    , _nzbCheck(nullptr)
    , _dispProgressBar(false)
    , _dispFilesPosting(false)
    , _progressbarTimer()

    , _refreshRate(kDefaultRefreshRate)

    , _activeJob(nullptr)
    , _pendingJobs()
    , _packingJob(nullptr)
    , _historyFieldSeparator(kDefaultFieldSeparator)
    , _postHistoryFile()

#if defined(WIN32) || defined(__MINGW64__)
    , _dbHistoryFile(kDbHistoryFile)
#else
    , _dbHistoryFile(QString("%1/%2").arg(getenv("HOME"), kDbHistoryFile))
#endif
    , _folderMonitor(nullptr)
    , _monitorThread(nullptr)

    , _lang("en")
    , _translators()
    , _netMgr()
    , _doShutdownWhenDone(false)
    , _shutdownProc(nullptr)

#if defined(WIN32) || defined(__MINGW64__)
    , _shutdownCmd(kDefaultShutdownCmdWindows)
#elif defined(__APPLE__) || defined(__MACH__)
    , _shutdownCmd(kDefaultShutdownCmdMacOS)
#else
    , _shutdownCmd(kDefaultShutdownCmdLinux)
#endif

    , _proxySocks5(QNetworkProxy::NoProxy)
    , _proxyUrl()
    , _dbHistory(new Database)
{
    QThread::currentThread()->setObjectName(kThreadNameMain);

#ifdef __USE_HMI__
    if (_hmi)
        _hmi->setWindowTitle(QString("%1_v%2").arg(kAppName).arg(kVersion));
#endif

    // in case we want to generate random uploader (_from not provided)
    //    std::srand(static_cast<uint>(QDateTime::currentMSecsSinceEpoch()));
    std::srand(QUuid::createUuid().data1); // use more random seed

    // check if the embedded par2 is available (windows or appImage)
    QString par2Embedded;
#if defined(WIN32) || defined(__MINGW64__)
    par2Embedded = QString("%1/par2.exe").arg(qApp->applicationDirPath());
#else
    par2Embedded        = QString("%1/par2").arg(qApp->applicationDirPath());
#endif

    QFileInfo fi(par2Embedded);
    if (fi.exists() && fi.isFile() && fi.isExecutable())
        _postingParams->setPar2Path(par2Embedded);

    _loadTanslators();

#if defined(__DEBUG__) && QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    QNetworkConfigurationManager netConfMgr;
    for (QNetworkConfiguration const &conf : netConfMgr.allConfigurations())
    {
        qDebug() << "net conf: " << conf.name() << ", bearerType: " << conf.bearerTypeName()
#  if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
                 << ", timeout: " << conf.connectTimeout()
#  endif
                 << ", isValid: " << conf.isValid() << ", state: " << conf.state();
    }

    QNetworkConfiguration conf = netConfMgr.defaultConfiguration();
    qDebug() << "DEFAULT conf: " << conf.name() << ", bearerType: " << conf.bearerTypeName()
#  if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
             << ", timeout: " << conf.connectTimeout()
#  endif
             << ", isValid: " << conf.isValid() << ", state: " << conf.state();
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(&_netMgr,
            &QNetworkAccessManager::networkAccessibleChanged,
            this,
            &NgPost::onNetworkAccessibleChanged);
#endif
}

void NgPost::startFolderMonitoring(QString const &folderPath)
{
    if (!_postingParams->quietMode())
        NgLogger::log(tr("Start monitoring folder: %1 (delay: %2ms)")
                              .arg(folderPath)
                              .arg(FoldersMonitorForNewFiles::sMSleep),
                      true);

    if (_folderMonitor) // already monitoring folders!
        _folderMonitor->addFolder(folderPath);
    else
    {
        _monitorThread = new QThread();
        _monitorThread->setObjectName(kThreadNameMonitor);
        _folderMonitor = new FoldersMonitorForNewFiles(folderPath);
        _folderMonitor->moveToThread(_monitorThread);
        connect(_folderMonitor,
                &FoldersMonitorForNewFiles::newFileToProcess,
                this,
                &NgPost::onNewFileToProcess,
                Qt::QueuedConnection);
        _monitorThread->start();
    }
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
    NgLogger::log("Destuction NgPost...", true);
#endif

    if (_nzbCheck)
        delete _nzbCheck;

    _stopMonitoring();

    //    _finishPosting();

    _progressbarTimer.stop();

    closeAllPostingJobs();

    if (_activeJob)
        delete _activeJob;
    qDeleteAll(_pendingJobs);
}

int NgPost::nbMissingArticles() const { return _nzbCheck->nbMissingArticles(); }

bool NgPost::initHistoryDatabase() { return _dbHistory->initSQLite(_dbHistoryFile); }

void NgPost::startLogInFile() const
{
#if defined(WIN32) || defined(__MINGW64__)
    QString logFilePath = kDefaultLogFile;
#else
    QString logFilePath = QString("%1/%2").arg(getenv("HOME")).arg(kDefaultLogFile);
#endif
    NgLogger::startLogInFile(logFilePath);
}

void NgPost::setDisplayProgress(QString const &txtValue)
{
    QString val = txtValue.toLower();
    if (val == kDisplayProgress[DISP_PROGRESS::BAR])
    {
        _dispProgressBar  = true;
        _dispFilesPosting = false;
        qDebug() << "Display progressbar bar\n";
    }
    else if (val == kDisplayProgress[DISP_PROGRESS::FILES])
    {
        _dispProgressBar  = false;
        _dispFilesPosting = true;
        qDebug() << "Display Files when start posting\n";
    }
    else if (val == kDisplayProgress[DISP_PROGRESS::NONE])
    { // force it in case in the config file something was on
        _dispProgressBar  = false;
        _dispFilesPosting = false;
    }
    else
        NgLogger::error(tr("Wrong display keyword: %1").arg(val.toUpper()));
}

void NgPost::setProxy(QString const &url)
{
    QRegularExpression      regExp(kProxyStrRegExp, QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = regExp.match(url);
    if (match.hasMatch())
    {
        _proxyUrl = url;
        // "^(([^:]+):([^@]+)@)?([\\w\\.\\-_]+):(\\d+)$";
        QString user = match.captured(2);
        QString pass = match.captured(3);
        QString host = match.captured(4);
        ushort  port = match.captured(5).toUShort();
        _proxySocks5 = QNetworkProxy(QNetworkProxy::Socks5Proxy, host, port, user, pass);
        QNetworkProxy::setApplicationProxy(_proxySocks5);
        NgLogger::log(tr("Proxy used: %1:xxx@%2:%3").arg(user).arg(host).arg(port), true);
    }
    else
        NgLogger::error(
                tr("Error parsing Proxy Socks5 parameters. The syntax should be: %1").arg(kProxyStrRegExp));
}

#ifdef __USE_HMI__
bool NgPost::hmiUseFixedPassword() const { return _hmi->useFixedPassword(); }
#endif

void NgPost::_finishPosting()
{
#ifdef __USE_HMI__
    if (_hmi || _dispProgressBar)
#else
    if (_dispProgressBar)
#endif
        disconnect(&_progressbarTimer, &QTimer::timeout, this, &NgPost::onRefreshprogressbarBar);

    if (_activeJob && _activeJob->hasUploaded())
    {
#ifdef __USE_HMI__
        if (_hmi || _dispProgressBar)
#else
        if (_dispProgressBar)
#endif
        {
            onRefreshprogressbarBar();
#ifdef __USE_HMI__
            if (!_hmi)
#endif
                std::cout << std::endl;
        }
    }
}

#ifdef __USE_HMI__
int NgPost::startHMI()
{
    QStringList errors = parseDefaultConfig();
    { // MB_TODO: check this migration!!! make it even for CMD ;)
        Migration m(*this);
        //        _createDbIfNotExist();
        m.migrate();
    }
    if (!errors.isEmpty())
        NgLogger::error(errors);
#  ifdef __DEBUG__
    _postingParams->dumpParams();
#  endif
    _hmi->init(this);
    _hmi->show();
    changeLanguage(_lang); // reforce lang set up...
    _hmi->setNightMode(false);
    return _app->exec();
}

void NgPost::onSwitchNightMode()
{
    _isNightMode = !_isNightMode;
    if (_isNightMode)
    {
        //    QFile f( ":/style/DarkStyle.qss" );
        QFile f(":/style/QTDark.qss");
        if (!f.exists())
            qWarning() << "Unable to set dark stylesheet, file not found";
        else
        {
            f.open(QFile::ReadOnly | QFile::Text);
            QTextStream ts(&f);
            qApp->setStyleSheet(ts.readAll());
        }
    }
    else
    {
        //    https://successfulsoftware.net/2021/03/31/how-to-add-a-dark-theme-to-your-qt-application/
        //    To unset the stylesheet and return to a light theme just call:

        qApp->setStyleSheet("");
    }
    _hmi->setNightMode(_isNightMode);
}
#endif

// void NgPost::onLog(QString msg, bool newline) const { NgLogger::log(msg, newline); }

// void NgPost::onError(QString msg) { criticalError(msg, ERR_CODE::COMPLETED_WITH_ERRORS); }

// void NgPost::onErrorConnecting(QString err) { criticalError(err, ERR_CODE::COMPLETED_WITH_ERRORS); }

void NgPost::onRefreshprogressbarBar()
{
    if (_activeJob && _activeJob->isPaused())
        return;

    uint    nbArticlesUploaded = 0, nbArticlesTotal = 0;
    QString avgSpeed("0 B/s");
#ifdef __COMPUTE_IMMEDIATE_SPEED__
    QString immediateSpeed("0 B/s");
#endif
    if (_activeJob)
    {
        nbArticlesTotal    = _activeJob->nbArticlesTotal();
        nbArticlesUploaded = _activeJob->nbArticlesUploaded();
        avgSpeed           = _activeJob->avgSpeed();
#ifdef __COMPUTE_IMMEDIATE_SPEED__
        immediateSpeed = _activeJob->immediateSpeed();
#endif
    }
#ifdef __USE_HMI__
    if (_hmi)
#  ifdef __COMPUTE_IMMEDIATE_SPEED__
        _hmi->updateProgressBar(nbArticlesTotal, nbArticlesUploaded, avgSpeed, immediateSpeed);
#  else
        _hmi->updateProgressBar(nbArticlesTotal, nbArticlesUploaded, avgSpeed);
#  endif
    else
#endif
    {
        float progressbar = static_cast<float>(nbArticlesUploaded);
        progressbar /= nbArticlesTotal;

        //        qDebug() << "[NgPost::onRefreshprogressbarBar] uploaded: " << nbArticlesUploaded
        //                 << " / " << nbArticlesTotal
        //                 << " => progressbar: " << progressbar << "\n";

        std::cout << "\r[";
        int pos = static_cast<int>(std::floor(progressbar * kProgressbarBarWidth));
        for (int i = 0; i < kProgressbarBarWidth; ++i)
        {
            if (i < pos)
                std::cout << "=";
            else if (i == pos)
                std::cout << ">";
            else
                std::cout << " ";
        }
        std::cout << "] " << int(progressbar * 100) << " %"
                  << " (" << nbArticlesUploaded << " / " << nbArticlesTotal << ")"
                  << " avg. speed: " << avgSpeed.toStdString();
        std::cout.flush();
    }
    if (nbArticlesUploaded < nbArticlesTotal)
        _progressbarTimer.start(_refreshRate);
}

void NgPost::onNewFileToProcess(QFileInfo const &fileInfo)
{
    if (fileInfo.isDir())
    {
        if (_postingParams->monitorIgnoreDir())
        {
            NgLogger::log(tr("MONITOR_IGNORE_DIR ON => Ignoring new incoming folder %1")
                                  .arg(fileInfo.absoluteFilePath()),
                          true,
                          NgLogger::DebugLevel::Debug);
            return;
        }
    }
    else if (!_postingParams->monitorExtensions().isEmpty()
             && !_postingParams->monitorExtensions().contains(fileInfo.suffix()))
    {
#ifdef __DEBUG__
        qDebug() << "MONITOR_EXTENSIONS ON => Ignoring new incoming file: " << fileInfo.absoluteFilePath()
                 << ", _monitorExtensions: " << _postingParams->monitorExtensions()
                 << ", size: " << _postingParams->monitorExtensions().size()
                 << ", suffix: " << fileInfo.suffix();
#endif
        NgLogger::log(
                tr("MONITOR_EXTENSIONS ON => Ignoring new incoming file %1").arg(fileInfo.absoluteFilePath()),
                true,
                NgLogger::DebugLevel::Debug);
        return;
    }

#ifdef __USE_HMI__
    if (_hmi)
    {
        _hmi->updateAutoPostingParams();
        _hmi->setJobLabel(-1);
        // MB_TODO make sure _params->_delAuto is updaed directly by autoWidget
        //        _delAuto = _hmi->autoWidget()->deleteFilesOncePosted();
        _hmi->autoWidget()->newFileToProcess(fileInfo);
    }
#endif
    NgLogger::log(tr("Processing new incoming file: %1").arg(fileInfo.absoluteFilePath()), true);
    post(fileInfo, _postingParams->monitorNzbFolders() ? QDir(fileInfo.absolutePath()).dirName() : QString());
}

#include <QTranslator>
void NgPost::_loadTanslators()
{
    QDir dir(kTranslationPath);
    for (QFileInfo const &qmFile : dir.entryInfoList(QStringList("ngPost_*.qm")))
    {
        QTranslator *translator = new QTranslator();
        QString      lang       = qmFile.completeBaseName(); // ngPost_en
        lang.remove(0, lang.indexOf('_') + 1);               // en

        if (translator->load(qmFile.absoluteFilePath()))
            _translators.insert(lang, translator);
        else
        {
            if (lang == "en")
                _translators.insert(lang, nullptr);
            else
                NgLogger::error(tr("error loading translator %1").arg(qmFile.absoluteFilePath()));
            delete translator;
        }
    }
#ifdef __DEBUG__
    qDebug() << "available translators: " << _translators.keys().join(", ");
#endif
    if (_lang != "en")
    {
        if (_translators.contains(_lang))
            _app->installTranslator(_translators[_lang]);
        else
        {
            NgLogger::error(tr("ERROR: couldn't find translator for lang %1").arg(_lang));
            _lang = "en";
        }
    }
}

void NgPost::changeLanguage(QString const &lang)
{
    _app->removeTranslator(_translators[_lang]);
    _lang = lang;

    if (_translators[_lang])
        _app->installTranslator(_translators[_lang]);
}

void NgPost::checkForNewVersion()
{
    QUrl            proFileURL(kProFileURL);
    QNetworkRequest req(proFileURL);
    req.setRawHeader("User-Agent", "ngPost C++ app");

    QNetworkReply *reply = _netMgr.get(req);
    QObject::connect(reply, &QNetworkReply::finished, this, &NgPost::onCheckForNewVersion);
}

bool NgPost::checkSupportSSL()
{
    if (!PostingJob::supportsSsl())
    {
        NgLogger::log(tr("SSL issue on your system: %1").arg(PostingJob::sslSupportInfo()), true);
        return false;
    }
    return true;
}

void NgPost::doNzbPostCMD(PostingJob *job)
{
    // first NZB_UPLOAD_URL
    if (_postingParams->urlNzbUpload())
    {
        FileUploader *testUpload = new FileUploader(_netMgr, job->nzbFilePath());
        connect(testUpload, &FileUploader::error, qOverload<QString>(&NgLogger::error));
        connect(testUpload,
                &FileUploader::log,
                [](QString msg, bool newline = true) { NgLogger::log(msg, newline); });
        connect(testUpload, &FileUploader::readyToDie, testUpload, &QObject::deleteLater, Qt::QueuedConnection);
        testUpload->startUpload(*_postingParams->urlNzbUpload());
    }

    // second NZB_POST_CMD
    if (!_postingParams->nzbPostCmd().isEmpty())
    {
        for (QString const &nzbPostCmd : _postingParams->nzbPostCmd())
        {
            QString fullCmd(nzbPostCmd);
            fullCmd.replace("%1", job->nzbFilePath()); // for backwards compatibility
            fullCmd.replace(kPostCmdPlaceHolders[PostCmdPlaceHolders::nzbPath],
#if defined(Q_OS_WIN)
                            QString(job->nzbFilePath()).replace("/", "\\")
#else
                            job->nzbFilePath()
#endif
            );
            fullCmd.replace(kPostCmdPlaceHolders[PostCmdPlaceHolders::nzbName],
                            QFileInfo(job->nzbFilePath()).completeBaseName());
            fullCmd.replace(kPostCmdPlaceHolders[PostCmdPlaceHolders::rarName], job->rarName());
            fullCmd.replace(kPostCmdPlaceHolders[PostCmdPlaceHolders::rarPass], job->rarPass());
            fullCmd.replace(kPostCmdPlaceHolders[PostCmdPlaceHolders::nbArticles],
                            QString::number(job->nbArticlesTotal()));
            fullCmd.replace(kPostCmdPlaceHolders[PostCmdPlaceHolders::nbArticlesFailed],
                            QString::number(job->nbArticlesFailed()));
            fullCmd.replace(kPostCmdPlaceHolders[PostCmdPlaceHolders::sizeInByte],
                            QString::number(job->_totalSize));
            fullCmd.replace(kPostCmdPlaceHolders[PostCmdPlaceHolders::nbFiles], QString::number(job->_nbFiles));
            fullCmd.replace(kPostCmdPlaceHolders[PostCmdPlaceHolders::groups], job->groups());
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
            QStringList args = parseCombinedArgString(fullCmd);
#else
            QStringList args = QProcess::splitCommand(fullCmd);
#endif
            QString cmd = args.takeFirst();
            qDebug() << "cmd: " << cmd << ", args: " << args;
            int res = QProcess::execute(cmd, args);
            if (NgLogger::isDebugMode())
                NgLogger::log(tr("NZB Post cmd: %1 exitcode: %2").arg(fullCmd).arg(res), true);
            else
                NgLogger::log(fullCmd, true);
        }
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
#ifdef __USE_HMI__
        if (_hmi)
            _hmi->setPauseIcon(false);
#endif
    }
}

void NgPost::resume()
{
    if (_activeJob && _activeJob->isPaused())
    {
        _activeJob->resume();
#ifdef __USE_HMI__
        if (_hmi)
            _hmi->setPauseIcon(true);
#endif
        _progressbarTimer.start(_refreshRate);
    }
}

void NgPost::post(QFileInfo const &fileInfo, QString const &monitorFolder)
{
    QString nzbName     = getNzbName(fileInfo);
    QString nzbPath     = getNzbPath(monitorFolder);
    QString nzbFilePath = QFileInfo(QDir(nzbPath), nzbName).absoluteFilePath();

    QString rarName = nzbName;
    QString rarPass = "";
    if (_postingParams->doCompress())
    {
        if (_postingParams->genName())
            rarName = NgTools::randomFileName(_postingParams->lengthName());

        if (!_postingParams->rarPassFixed().isEmpty()) // rar pass fixed would take other
            rarPass = _postingParams->rarPassFixed();
        else if (_postingParams->genPass()) // shall we gen password?
            rarPass = NgTools::randomPass(_postingParams->lengthPass());
    }

    qDebug() << "Start posting job for " << nzbName << " with rar_name: " << rarName << " and pass: " << rarPass;
    startPostingJob(new PostingJob(*this,
                                   rarName,
                                   rarPass,
                                   nzbFilePath,
                                   { fileInfo },
                                   _postingParams->getPostingGroups(),
                                   _postingParams->getFrom(),
                                   _postingParams));
}

void NgPost::onCheckForNewVersion()
{
    QNetworkReply *reply        = static_cast<QNetworkReply *>(sender());
    QStringList    v            = kVersion.split(".");
    int            currentMajor = v.at(0).toInt(), currentMinor = 0;
    if (v.size() == 2) // 5.0 will get kVersion = "5"
        currentMinor = v.at(1).toInt();

    while (!reply->atEnd())
    {
        QString                 line  = reply->readLine().trimmed();
        QRegularExpressionMatch match = kAppVersionRegExp.match(line);
        if (match.hasMatch())
        {
            QString lastRealease = match.captured(1);
            int     lastMajor = match.captured(2).toInt(), lastMinor = match.captured(3).toInt();
            qDebug() << "lastMajor: " << lastMajor << ", lastMinor: " << lastMinor
                     << " (currentMajor: " << currentMajor << ", currentMinor: " << currentMinor << ")";

            if (lastMajor > currentMajor || (lastMajor == currentMajor && lastMinor > currentMinor))
            {
#ifdef __USE_HMI__
                if (_hmi)
                {
                    QString msg = tr("<center><h3>New version available on GitHUB</h3></center>");
                    msg += tr("<br/>The last release is now <b>v%1</b>").arg(lastRealease);
                    msg += tr("<br/><br/>You can download it from the <a "
                              "href='https://github.com/mbruel/ngPost/releases/tag/v%1'>release directory</a>")
                                   .arg(lastRealease);
                    msg += tr("<br/><br/>Here are the full <a "
                              "href='https://github.com/mbruel/ngPost/blob/master/"
                              "release_notes.txt'>release_notes</"
                              "a>");

                    QMessageBox::information(_hmi, tr("New version available"), msg);
                }
                else
#endif
                    qCritical() << "There is a new version available on GitHUB: v" << lastRealease
                                << " (visit https://github.com/mbruel/ngPost/ to get it)";
            }

            break; // no need to continue to parse the page
        }
    }
    reply->deleteLater();
}

#ifdef __USE_HMI__
#  include <QDesktopServices>
void NgPost::onDonation() { QDesktopServices::openUrl(kDonationURL); }
void NgPost::onDonationBTC() { QDesktopServices::openUrl(kDonationBtcURL); }

#  include "hmi/AboutNgPost.h"
void NgPost::onAboutClicked()
{
    AboutNgPost about(this);
    about.exec();
}
#endif

void NgPost::onPostingJobStarted()
{
#ifdef __USE_HMI__
    if (_hmi || _dispProgressBar)
#else
    if (_dispProgressBar)
#endif
    {
        connect(&_progressbarTimer,
                &QTimer::timeout,
                this,
                &NgPost::onRefreshprogressbarBar,
                Qt::DirectConnection);
        _progressbarTimer.start(_refreshRate);
    }
}

void NgPost::onPackingDone()
{
    if (_postingParams->preparePacking())
    {
        PostingJob *job = static_cast<PostingJob *>(sender());
#ifdef __DEBUG__
        qDebug() << "[MB_TRACE][Issue#82][NgPost::onPackingDone] job: " << job << ", file: " << job->nzbName();
#endif
        if (job == _activeJob)
        {
#ifdef __DEBUG__
            NgLogger::log("[NgPost::onPackingDone] Active Job packed :)", true);
#endif
            // The previous active Job finished posting before the packing of the next one
            if (_packingJob == nullptr)
            {
                if (!job->hasPostStarted())
                {
#ifdef __DEBUG__
                    NgLogger::log("[NgPost::onPackingDone] Active Job didn't start posting...", true);
#endif
                    job->_postFiles();
                }
                _prepareNextPacking();
            }
        }
        else if (job != _packingJob)
            NgLogger::error("unexpected job finished packing..."); // should never happen...
#ifdef __DEBUG__
        else
            NgLogger::log("[NgPost::onPackingDone] Packing Job done :)", true);
#endif
    }
}

void NgPost::_prepareNextPacking()
{
    if (_pendingJobs.size())
    {
        _packingJob = _pendingJobs.first();
        if (_packingJob->hasPacking())
            emit _packingJob->startPosting(false);
        NgLogger::log(tr("no packing needed for next pending job %1").arg(_packingJob->nzbName()),
                      true,
                      NgLogger::DebugLevel::FullDebug);
    }
}

void NgPost::onPostingJobFinished()
{
    PostingJob *job = static_cast<PostingJob *>(sender());
#ifdef __DEBUG__
    qDebug() << "[MB_TRACE][Issue#82][NgPost::onPostingJobFinished] job: " << job
             << ", file: " << job->nzbName();
#endif
    if (job == _activeJob)
    {
#ifdef __USE_HMI__
        if (_hmi && !job->widget())
            _hmi->autoWidget()->updateFinishedJob(job->getFirstOriginalFile(),
                                                  job->nbArticlesTotal(),
                                                  job->nbArticlesUploaded(),
                                                  job->nbArticlesFailed());
#endif
        _dbHistory->insertPost(NgTools::currentDateTime(),
                               _activeJob->nzbName(),
                               _activeJob->postSize(),
                               _activeJob->avgSpeed(),
                               _activeJob->hasCompressed() ? _activeJob->rarName() : QString(),
                               _activeJob->hasCompressed() ? _activeJob->rarPass() : QString(),
                               _activeJob->groups(),
                               _activeJob->from(false),
                               _activeJob->tmpPath(),
                               _activeJob->nzbFilePath(),
                               _activeJob->hasPostFinished());

        if (_activeJob->hasPostFinished() && !_postHistoryFile.isEmpty())
        {
            QFile hist(_postHistoryFile);
            if (hist.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            {
                QTextStream stream(&hist);
                stream << NgTools::currentDateTime() << _historyFieldSeparator << _activeJob->nzbName()
                       << _historyFieldSeparator << _activeJob->postSize() << _historyFieldSeparator
                       << _activeJob->avgSpeed();
                if (_activeJob->hasCompressed())
                    stream << _historyFieldSeparator << _activeJob->rarName() << _historyFieldSeparator
                           << _activeJob->rarPass();
                else
                    stream << _historyFieldSeparator << _historyFieldSeparator;

                stream << _historyFieldSeparator << _activeJob->groups() << _historyFieldSeparator
                       << _activeJob->from(false) << "\n"
                       << MB_FLUSH;
                hist.close();
            }
        }

#ifdef __USE_HMI__
        if (_hmi && _postingParams->autoCloseTabs() && _activeJob->hasPostFinishedSuccessfully())
            _hmi->closeTab(_activeJob->widget());
#endif

        _activeJob->deleteLater();
        _activeJob = nullptr;

        if (_pendingJobs.size())
        {
            _activeJob = _pendingJobs.dequeue();

#ifdef __USE_HMI__
            if (_hmi)
                _hmi->setTab(_activeJob->widget());
#endif
            if (_postingParams->preparePacking())
            {
                if (_packingJob == _activeJob)
                {
                    _packingJob = nullptr;
                    if (_activeJob->isPacked())
                    {
                        _activeJob->_postFiles();
                        _prepareNextPacking();
                    }
                    else if (!_activeJob->hasPacking())
                    {
                        NgLogger::log(tr("start non packing job..."), true, NgLogger::DebugLevel::FullDebug);
                        emit _activeJob->startPosting(true);
                        _prepareNextPacking();
                    }
                    // otherwise it will be triggered automatically when the packing is finished
                    // as it is now the active job ;)
                }
                else
                    NgLogger::error("next active job different to the packing one..."); // should never happen...
            }
            else
                emit _activeJob->startPosting(true);
        }
        else if (_doShutdownWhenDone && !_shutdownCmd.isEmpty())
        {
            // cf
            // https://forum.qt.io/topic/111602/qprocess-signals-not-received-in-slots-except-in-debug-with-breakpoints/
            //             int exitCode = QProcess::execute("echo \\\"toto\\\" | /usr/bin/sudo -S /bin/ls -al");
            //             qDebug() << QString("Shutdown exit code: %1").arg(exitCode);
            _shutdownProc = new QProcess();
            connect(_shutdownProc,
                    &QProcess::readyReadStandardOutput,
                    this,
                    &NgPost::onShutdownProcReadyReadStandardOutput,
                    Qt::DirectConnection);
            connect(_shutdownProc,
                    &QProcess::readyReadStandardError,
                    this,
                    &NgPost::onShutdownProcReadyReadStandardError,
                    Qt::DirectConnection);
            connect(_shutdownProc,
                    static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                    this,
                    &NgPost::onShutdownProcFinished,
                    Qt::QueuedConnection);
            //            connect(_shutdownProc, &QProcess::started, this, &NgPost::onShutdownProcStarted,
            //            Qt::DirectConnection); connect(_shutdownProc, &QProcess::stateChanged,  this,
            //            &NgPost::onShutdownProcStateChanged,  Qt::DirectConnection);
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
            connect(_shutdownProc,
                    &QProcess::errorOccurred,
                    this,
                    &NgPost::onShutdownProcError,
                    Qt::DirectConnection);
#endif
            //            _shutdownProc->start("/bin/ls", QStringList() << "-al");
            //            _shutdownProc->start("/usr/bin/sudo", QStringList() << "-n" << "/sbin/poweroff");

            //            qDebug() << " cmd: " << _shutdownCmd;

            //            QStringList args = _shutdownCmd.split(QRegularExpression("\\s+"));
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
            QStringList args = parseCombinedArgString(_shutdownCmd);
#else
            QStringList args = QProcess::splitCommand(_shutdownCmd);
#endif
            QString cmd = args.takeFirst();
            qDebug() << "cmd: " << cmd << ", args: " << args;
            _shutdownProc->start(cmd, args);
        }
#ifdef __USE_HMI__
        else if (!_folderMonitor && !_hmi)
#else
        else if (!_folderMonitor)
#endif
        {
            NgLogger::error(tr(" => closing application"));
            qApp->quit();
        }
    }
    else if (_postingParams->preparePacking() && job == _packingJob)
    {
        _packingJob = nullptr;
        _pendingJobs.dequeue(); // remove the packingJob
        job->deleteLater();
        NgLogger::error(tr("packing job finished unexpectedly..."));
        _prepareNextPacking();
    }
    else
    {
        // it was a Pending job that has been cancelled
        NgLogger::log(tr("Cancelled pending job %1?").arg(NgTools::ptrAddrInHex(job)),
                      true,
                      NgLogger::DebugLevel::Debug);
        _pendingJobs.removeOne(job);
        job->deleteLater();
    }
}

void NgPost::onShutdownProcReadyReadStandardOutput()
{
    QString line(_shutdownProc->readAllStandardOutput());
    //    _cout << "Shutdown out: " << line << "\n" << MB_FLUSH;
    NgLogger::log(QString("Shutdown out: %1").arg(QString(line)), true);
}

void NgPost::onShutdownProcReadyReadStandardError()
{
    NgLogger::error(QString("Shutdown ERROR: %1").arg(_shutdownProc->readAllStandardError().constData()));
}

void NgPost::onShutdownProcFinished(int exitCode)
{
    NgLogger::log(QString("Shutdown proc exitCode: ").arg(exitCode), true, NgLogger::DebugLevel::Debug);
    _shutdownProc->deleteLater();
    _shutdownProc = nullptr;
}

// void NgPost::onShutdownProcStarted()
//{
//     qDebug() << "Shutdown proc Started";
// }

// void NgPost::onShutdownProcStateChanged(QProcess::ProcessState newState)
//{
//     qDebug() << "Shutdown proc new State: " << newState;
// }

void NgPost::onShutdownProcError(QProcess::ProcessError error)
{
    NgLogger::error(QString("Shutdown process Error: %1").arg(error));
}

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
void NgPost::onNetworkAccessibleChanged(QNetworkAccessManager::NetworkAccessibility accessible)
{
    qDebug() << "[NgPost::onNetworkAccessibleChanged] accessible: " << accessible;
    QString msg(tr("Network access changed: %1"));
    if (accessible == QNetworkAccessManager::NetworkAccessibility::Accessible)
    {
        NgLogger::log(msg.arg("ON"));
#  ifndef __USE_CONNECTION_TIMEOUT__
        if (_activeJob && _activeJob->isPaused())
            _activeJob->resume();
#  endif
    }
    else
    {
        NgLogger::error(msg.arg("OFF"));
#  ifndef __USE_CONNECTION_TIMEOUT__
        if (_activeJob)
            _activeJob->pause();
#  endif
    }
}
#endif

void NgPost::closeAllPostingJobs()
{
    qDeleteAll(_pendingJobs);
    if (_activeJob)
        _activeJob->onStopPosting();
}

void NgPost::closeAllMonitoringJobs()
{ // MB_TODO: it doesn'#t look related to Monitoring, is it? otherwise change the name...
    // used only by HMI ? => #ifdef __USE_HMI__
    auto it = _pendingJobs.begin();
    while (it != _pendingJobs.end())
    {
        PostingJob *job = *it;
        if (!job->widget())
        {
            it = _pendingJobs.erase(it);
            if (NgLogger::isDebugMode())
                NgLogger::error(tr("Cancelling monitoring job: %1").arg(job->getFirstOriginalFile()));
            delete job;
        }
        else
            ++it;
    }
    if (_activeJob && !_activeJob->widget())
    {
        emit _activeJob->stopPosting();
        if (NgLogger::isDebugMode())
            NgLogger::error(tr("Stopping monitoring job: %1").arg(_activeJob->getFirstOriginalFile()));
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
    Q_UNUSED(argc);
    return NgCmdLineLoader::loadCmdLine(argv[0], *this, _postingParams);
}

QString NgPost::getNzbName(QFileInfo const &fileInfo) const
{
    QString nzbName =
            fileInfo.isDir() ? QDir(fileInfo.absoluteFilePath()).dirName() : fileInfo.completeBaseName();
    if (_postingParams->removeAccentsOnNzbFileName())
        NgTools::removeAccentsFromString(nzbName);

    if (!nzbName.endsWith(".nzb"))
        nzbName += ".nzb";
    return nzbName;
}

QString NgPost::getNzbPath(QString const &monitorFolder)
{
    QString const &nzbPath = _postingParams->nzbPath();
    if (monitorFolder.isEmpty())
        return nzbPath;
    else
    {
        QString path;
        if (nzbPath.isEmpty())
            path = monitorFolder;
        else
            path = QFileInfo(QDir(nzbPath), monitorFolder).absoluteFilePath();
        QDir dir(path);
        if (!dir.exists())
            QDir().mkdir(path);

        return path;
    }
}

QStringList NgPost::parseDefaultConfig()
{
#if defined(WIN32) || defined(__MINGW64__)
    QString conf = kDefaultConfig;
#else
    QString conf = QString("%1/%2").arg(getenv("HOME")).arg(kDefaultConfig);
    if (!QFileInfo(conf).exists())
    {
        if (!_postingParams->saveConfig(conf, *this))
            return { tr("Couldn't create the default configuration file '%1'...").arg(conf) };

        NgLogger::log(tr("Default configuration file created: %1").arg(conf), true);
        if (!useHMI())
        {
            NgLogger::log(tr("Please fill at least a provider, your tmp, rar and par2 path and set the features "
                             "you like "
                             ";)"),
                          true);
#  if defined(__linux__) || defined(__APPLE__)
            QThread::sleep(3);
            // Open Editor
            QProcess *process = new QProcess(this);
#    ifdef __linux__
            process->setProgram(kDefaultTxtEditorLinux);
#    elif __APPLE__
            process->setProgram(kDefaultTxtEditorLinux);
#    endif
            process->setArguments({ conf });
            connect(process, &QProcess::finished, process, &QObject::deleteLater);
            process->start();
            if (process->waitForStarted())
                NgLogger::log(tr("Please save and close the editor to continue..."), true);
            process->waitForFinished();
#  endif
            return { tr("Once done, rerun the app ;)") };
        }
    }
#endif
    if (!_postingParams->quietMode())
        NgLogger::log(tr("Using default config file: %1").arg(conf), true);
    return NgConfigLoader::loadConfig(*this, conf, _postingParams);
}

bool NgPost::startPostingJob(QString const                &rarName,
                             QString const                &rarPass,
                             QString const                &nzbFilePath,
                             QFileInfoList const          &files,
                             std::string const            &from,
                             QMap<QString, QString> const &meta)
{
    return startPostingJob(new PostingJob(*this,
                                          rarName,
                                          rarPass,
                                          nzbFilePath,
                                          files,
                                          _postingParams->getPostingGroups(),
                                          from,
                                          _postingParams,
                                          meta));
}

bool NgPost::startPostingJob(PostingJob *job)
{
#ifdef __DEBUG__
    qDebug() << "[MB_TRACE][Issue#82][NgPost::startPostingJob] job: " << job << ", file: " << job->nzbName();
#endif
#ifdef __USE_HMI__
    if (_hmi)
    {
        connect(job,
                &PostingJob::articlesNumber,
                _hmi,
                &MainWindow::onSetProgressBarRange,
                Qt::QueuedConnection);
        if (!job->widget())
            connect(job, &PostingJob::postingStarted, _hmi->autoWidget(), &AutoPostWidget::onMonitorJobStart);
    }
#endif

    if (_activeJob)
    {
        _pendingJobs << job;
        if (_postingParams->preparePacking())
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

void NgPost::showVersionASCII() const
{
    NgLogger::log(QString("%1                          v%2\n\n%3\n")
                          .arg(kNgPostASCII)
                          .arg(kVersion)
                          .arg(PostingJob::sslSupportInfo()),
                  true);
}

void NgPost::saveConfig() const
{
    QString configPath = NgTools::getConfigurationFilePath();
    if (_postingParams->saveConfig(configPath, *this))
        NgLogger::log(tr("The config file '%1' has been updated").arg(configPath), true);
    else
        NgLogger::error(tr("Error: couldn't write the config file %1").arg(configPath));
}

void NgPost::setDelFilesAfterPosted(bool delFiles)
{
    // Thread safe, only main thread is using this (NgPost or HMI)
    if (_activeJob)
        _activeJob->setDelFilesAfterPosted(delFiles);
    for (PostingJob *job : _pendingJobs)
        job->setDelFilesAfterPosted(delFiles);
}

void NgPost::addMonitoringFolder(QString const &dirPath)
{
    if (_folderMonitor)
        _folderMonitor->addFolder(dirPath);
}

bool NgPost::doNzbCheck(QString const &nzbPath)
{
    _nzbCheck = new NzbCheck();                           // it's a one shot, no leak ;)
    _nzbCheck->setDebug(NgLogger::isDebugMode() ? 0 : 1); // MB_TODO : make NzbCheck use NgLogger...
    _nzbCheck->setDispProgressBar(_dispProgressBar || _dispFilesPosting);
    _nzbCheck->setQuiet(_postingParams->quietMode());
    int nbArticles = _nzbCheck->parseNzb(nzbPath);
    if (nbArticles == 0)
        return false;

    auto const &nntpServers = _postingParams->nntpServers();
    if (nntpServers.isEmpty())
    {
        NgLogger::criticalError(
                tr("No server found to launch nzbCheck on %1...\nCheck config or add some in command "
                   "line. cf --help")
                        .arg(nzbPath),
                NgError::ERR_CODE::ERR_NO_SERVER);
        return false;
    }

    // _nzbCheck is a big boy, we just let him the hand ;)
    _nzbCheck->checkPost(nntpServers);
    return true;
}

bool NgPost::removeNntpServer(NntpServerParams *server) { return _postingParams->removeNntpServer(server); }
