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
#include "NntpConnection.h"
#include "nntp/NntpFile.h"
#include "nntp/NntpArticle.h"
#include "nntp/NntpServerParams.h"
#include <cmath>
#include <QApplication>
#include <QThread>
#include <QTextStream>
#include <QCommandLineParser>
#include <QDir>
#include <QDebug>
#include <iostream>
#include "hmi/MainWindow.h"

#define NB_ARTICLES_TO_PREPARE_PER_CONNECTION 2

const QString NgPost::sAppName     = "ngPost";
const QString NgPost::sVersion     = QString::number(APP_VERSION);
const QString NgPost::sProFileURL  = "https://raw.githubusercontent.com/mbruel/ngPost/master/src/ngPost.pro";
const QString NgPost::sDonationURL = "https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=W2C236U6JNTUA&item_name=ngPost&currency_code=EUR";

const QString NgPost::sMainThreadName = "MainThread";

qint64        NgPost::sArticleSize = sDefaultArticleSize;
const QString NgPost::sSpace       = sDefaultSpace;

const QMap<NgPost::Opt, QString> NgPost::sOptionNames =
{
    {Opt::HELP,         "help"},
    {Opt::VERSION,      "version"},
    {Opt::CONF,         "conf"},
    {Opt::DISP_PROGRESS,"disp_progress"},
    {Opt::DEBUG,        "debug"},

    {Opt::INPUT,        "input"},
    {Opt::OUTPUT,       "output"},
    {Opt::NZB_PATH,     "nzbpath"},
    {Opt::THREAD,       "thread"},

    {Opt::MSG_ID,       "msg_id"},
    {Opt::META,         "meta"},
    {Opt::ARTICLE_SIZE, "article_size"},
    {Opt::FROM,         "from"},
    {Opt::GROUPS,       "groups"},
    {Opt::NB_RETRY,     "retry"},

    {Opt::OBFUSCATE,    "obfuscate"},
    {Opt::INPUT_DIR,    "inputdir"},


    {Opt::TMP_DIR,      "tmp_dir"},
    {Opt::RAR_PATH,     "rar_path"},
    {Opt::RAR_EXTRA,    "rar_extra"},
    {Opt::RAR_SIZE,     "rar_size"},
    {Opt::PAR2_PCT,     "par2_pct"},
    {Opt::PAR2_PATH,    "par2_path"},
    {Opt::PAR2_ARGS,    "par2_args"},
    {Opt::COMPRESS,     "compress"},
    {Opt::GEN_PAR2,     "gen_par2"},
    {Opt::GEN_NAME,     "gen_name"},
    {Opt::GEN_PASS,     "gen_pass"},
    {Opt::RAR_NAME,     "rar_name"},
    {Opt::RAR_PASS,     "rar_pass"},
    {Opt::LENGTH_NAME,  "length_name"},
    {Opt::LENGTH_PASS,  "length_pass"},


    {Opt::HOST,         "host"},
    {Opt::PORT,         "port"},
    {Opt::SSL,          "ssl"},
    {Opt::USER,         "user"},
    {Opt::PASS,         "pass"},
    {Opt::CONNECTION,   "connection"},

};

const QList<QCommandLineOption> NgPost::sCmdOptions = {
    { sOptionNames[Opt::HELP],                tr("Help: display syntax")},
    {{"v", sOptionNames[Opt::VERSION]},       tr( "app version")},
    {{"c", sOptionNames[Opt::CONF]},          tr( "use configuration file (if not provided, we try to load $HOME/.ngPost)"), sOptionNames[Opt::CONF]},
    { sOptionNames[Opt::DISP_PROGRESS],       tr( "display cmd progress: NONE (default), BAR or FILES"), sOptionNames[Opt::DISP_PROGRESS]},
    {{"d", sOptionNames[Opt::DEBUG]},         tr( "display some debug logs")},

    {{"i", sOptionNames[Opt::INPUT]},         tr("input file to upload (single file or directory), you can use it multiple times"), sOptionNames[Opt::INPUT]},
    {{"o", sOptionNames[Opt::OUTPUT]},        tr("output file path (nzb)"), sOptionNames[Opt::OUTPUT]},
    {{"t", sOptionNames[Opt::THREAD]},        tr("number of Threads (the connections will be distributed amongs them)"), sOptionNames[Opt::THREAD]},
    {{"x", sOptionNames[Opt::OBFUSCATE]},     tr("obfuscate the subjects of the articles (CAREFUL you won't find your post if you lose the nzb file)")},

    {{"g", sOptionNames[Opt::GROUPS]},        tr("newsgroups where to post the files (coma separated without space)"), sOptionNames[Opt::GROUPS]},
    {{"m", sOptionNames[Opt::META]},          tr("extra meta data in header (typically \"password=qwerty42\")"), sOptionNames[Opt::META]},
    {{"f", sOptionNames[Opt::FROM]},          tr("poster email (random one if not provided)"), sOptionNames[Opt::FROM]},
    {{"a", sOptionNames[Opt::ARTICLE_SIZE]},  tr("article size (default one: %1)").arg(sDefaultArticleSize), sOptionNames[Opt::ARTICLE_SIZE]},
    {{"z", sOptionNames[Opt::MSG_ID]},        tr("msg id signature, after the @ (default one: %1)").arg(sDefaultMsgIdSignature), sOptionNames[Opt::MSG_ID]},
    {{"r", sOptionNames[Opt::NB_RETRY]},      tr("number of time we retry to an Article that failed (default: %1)").arg(NntpArticle::nbMaxTrySending()), sOptionNames[Opt::NB_RETRY]},

//    TMP_PATH, RAR_PATH, RAR_SIZE, PAR2_PCT, PAR2_PATH,
    { sOptionNames[Opt::TMP_DIR],             tr( "temporary folder where the compressed files and par2 will be stored"), sOptionNames[Opt::TMP_DIR]},
    { sOptionNames[Opt::RAR_PATH],            tr( "RAR absolute file path (external application)"), sOptionNames[Opt::RAR_PATH]},
    { sOptionNames[Opt::RAR_SIZE],            tr( "size in MB of the RAR volumes (0 by default meaning NO split)"), sOptionNames[Opt::RAR_SIZE]},
    { sOptionNames[Opt::PAR2_PCT],            tr( "par2 redundancy percentage (0 by default meaning NO par2 generation)"), sOptionNames[Opt::PAR2_PCT]},
    { sOptionNames[Opt::PAR2_PATH],           tr( "par2 absolute file path (in case of self compilation of ngPost)"), sOptionNames[Opt::PAR2_PCT]},
    { sOptionNames[Opt::COMPRESS],            tr( "compress inputs using RAR")},
    { sOptionNames[Opt::GEN_PAR2],            tr( "generate par2 (to be used with --compress)")},
    { sOptionNames[Opt::RAR_NAME],            tr( "provide the RAR file name (to be used with --compress)"), sOptionNames[Opt::RAR_NAME]},
    { sOptionNames[Opt::RAR_PASS],            tr( "provide the RAR password (to be used with --compress)"), sOptionNames[Opt::RAR_PASS]},
    { sOptionNames[Opt::GEN_NAME],            tr( "generate random RAR name (to be used with --compress)")},
    { sOptionNames[Opt::GEN_PASS],            tr( "generate random RAR password (to be used with --compress)")},
    { sOptionNames[Opt::LENGTH_NAME],         tr( "length of the random RAR name (to be used with --gen_name), default: %1").arg(sDefaultLengthName), sOptionNames[Opt::LENGTH_NAME]},
    { sOptionNames[Opt::LENGTH_PASS],         tr( "length of the random RAR password (to be used with --gen_pass), default: %1").arg(sDefaultLengthPass), sOptionNames[Opt::LENGTH_PASS]},


    // for a single server...
    {{"h", sOptionNames[Opt::HOST]},          tr("NNTP server hostname (or IP)"), sOptionNames[Opt::HOST]},
    {{"P", sOptionNames[Opt::PORT]},          tr("NNTP server port"), sOptionNames[Opt::PORT]},
    {{"s", sOptionNames[Opt::SSL]},           tr("use SSL")},
    {{"u", sOptionNames[Opt::USER]},          tr("NNTP server username"), sOptionNames[Opt::USER]},
    {{"p", sOptionNames[Opt::PASS]},          tr("NNTP server password"), sOptionNames[Opt::PASS]},
    {{"n", sOptionNames[Opt::CONNECTION]},    tr("number of NNTP connections"), sOptionNames[Opt::CONNECTION]},

};



NgPost::NgPost(int &argc, char *argv[]):
    QObject (),
    _app(nullptr),
    _mode(argc > 1 ? AppMode::CMD : AppMode::HMI),
    _hmi(nullptr),
    _cout(stdout),
    _cerr(stderr),
#ifdef __DEBUG__
    _debug(true),
#else
    _debug(false),
#endif
    _dispProgressBar(false),
    _dispFilesPosting(false),
    _nzbName(),
    _filesToUpload(), _filesInProgress(),
    _nbFiles(0), _nbPosted(0),
    _threadPool(), _nntpConnections(), _nntpServers(),
    _obfuscateArticles(false), _from(),
    _groups(sDefaultGroups),
    _articleIdSignature(sDefaultMsgIdSignature),
    _nzb(nullptr), _nzbStream(),
    _nntpFile(nullptr), _file(nullptr), _buffer(nullptr), _part(0), _secureFile(),
    _articles(), _secureArticlesQueue(),
    _timeStart(), _totalSize(0),
    _meta(), _grpList(),
    _nbConnections(sDefaultNumberOfConnections), _nbThreads(QThread::idealThreadCount()),
    _socketTimeOut(sDefaultSocketTimeOut), _nzbPath(sDefaultNzbPath), _nzbPathConf(sDefaultNzbPath),
    _nbArticlesUploaded(0), _nbArticlesFailed(0),
    _uploadedSize(0), _nbArticlesTotal(0), _progressTimer(), _refreshRate(sDefaultRefreshRate),
    _stopPosting(0x0), _noMoreFiles(0x0),
    _extProc(nullptr), _compressDir(nullptr),
    _tmpPath(), _rarPath(), _rarArgs(), _rarSize(0), _par2Pct(0), _par2Path(), _par2Args(),
    _doCompress(false), _doPar2(false), _genName(), _genPass(),
    _lengthName(sDefaultLengthName), _lengthPass(sDefaultLengthPass),
    _rarName(), _rarPass(), _inputDir()
{
    if (_mode == AppMode::CMD)
        _app =  new QCoreApplication(argc, argv);
    else
    {
        _app = new QApplication(argc, argv);
        _hmi = new MainWindow(this);
        _hmi->setWindowTitle(QString("%1_v%2").arg(sAppName).arg(sVersion));
    }

    QThread::currentThread()->setObjectName(sMainThreadName);
    connect(this, &NgPost::scheduleNextArticle, this, &NgPost::onPrepareNextArticle, Qt::QueuedConnection);

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
}




NgPost::~NgPost()
{
#ifdef __DEBUG__
    _log("Destuction NgPost...");
#endif

    _finishPosting();
    qDeleteAll(_nntpServers);
    delete _app;
}

void NgPost::_finishPosting()
{
    _stopPosting = 0x1;

    if (_hmi || _dispProgressBar)
        disconnect(&_progressTimer, &QTimer::timeout, this, &NgPost::onRefreshProgressBar);
    if (!_timeStart.isNull())
    {
        _nbArticlesUploaded = _nbArticlesTotal; // we might not have processed the last onArticlePosted
        _uploadedSize       = _totalSize;
        if (_hmi || _dispProgressBar)
        {
            onRefreshProgressBar();
            if (!_hmi)
                std::cout << std::endl;
        }
    }

#ifdef __DEBUG__
    _log("Finishing posting...");
#endif

    // 1.: print stats
    if (!_timeStart.isNull())
        _printStats();


    // 2.: close nzb file
    _closeNzb();


    // 3.: close all the connections (they're living in the _threadPool)
    for (NntpConnection *con : _nntpConnections)
        emit con->killConnection();

    qApp->processEvents();


    // 4.: stop and wait for all threads
    for (QThread *th : _threadPool)
    {
#ifdef __DEBUG__
        _log(tr("Stopping thread %1").arg(th->objectName()));
#endif
        th->quit();
        th->wait();
    }

#ifdef __DEBUG__
    _log("All connections are closed...");
    _log(tr("prepared articles queue size: %1").arg(_articles.size()));
#endif

    // 5.: print out the list of files that havn't been posted
    // (in case of disconnection)
    int nbPendingFiles = _filesToUpload.size() + _filesInProgress.size();
    if (nbPendingFiles)
    {
        _error(QString("ERROR: there were %1 on %2 that havn't been posted:").arg(
                   nbPendingFiles).arg(_nbFiles));
        for (NntpFile *file : _filesInProgress)
            _error(QString("  - %1").arg(file->path()));
        for (NntpFile *file : _filesToUpload)
            _error(QString("  - %1").arg(file->path()));

        _error(tr("you can try to repost only those and concatenate the nzb with the current one ;)"));
    }

    // 6.: free all resources
    qDeleteAll(_filesInProgress); _filesInProgress.clear();
    qDeleteAll(_filesToUpload);   _filesToUpload.clear();
    qDeleteAll(_nntpConnections); _nntpConnections.clear();
    qDeleteAll(_threadPool);      _threadPool.clear();
    if (_file)
    {
        _file->close();
        delete _file;
        _file = nullptr;
    }

    if (_hmi)
        _hmi->setIDLE();
}

void NgPost::stopPosting()
{
    _stopPosting = 0x1;

    // 0.: close all the connections (they're living in the _threadPool)
    for (NntpConnection *con : _nntpConnections)
        emit con->killConnection();

    qApp->processEvents();


    if (_hmi || _dispProgressBar)
    {
        disconnect(&_progressTimer, &QTimer::timeout, this, &NgPost::onRefreshProgressBar);
        if (!_timeStart.isNull())
        {
            onRefreshProgressBar();
            if (!_hmi)
                std::cout << std::endl;
        }
    }

#ifdef __DEBUG__
    _log("Stop posting...");
#endif

    // 1.: print stats
    if (!_timeStart.isNull())
        _printStats();


    // 2.: close nzb file
    _closeNzb();


    // 4.: stop and wait for all threads
    for (QThread *th : _threadPool)
    {
#ifdef __DEBUG__
        _log(tr("Stopping thread %1").arg(th->objectName()));
#endif
        th->quit();
        th->wait();
    }

#ifdef __DEBUG__
    _log("All connections are closed...");
    _log(tr("prepared articles queue size: %1").arg(_articles.size()));
#endif

    // 5.: print out the list of files that havn't been posted
    // (in case of disconnection)
    int nbPendingFiles = _filesToUpload.size() + _filesInProgress.size();
    if (nbPendingFiles)
    {
        _error(QString("There were %1 on %2 that havn't been posted:").arg(
                   nbPendingFiles).arg(_nbFiles));
        for (NntpFile *file : _filesInProgress)
            _error(QString("  - %1").arg(file->path()));
        for (NntpFile *file : _filesToUpload)
            _error(QString("  - %1").arg(file->path()));

        _error(tr("you can try to repost only those and concatenate the nzb with the current one ;)"));
    }

    // 6.: free all resources
    qDeleteAll(_filesInProgress); _filesInProgress.clear();
    qDeleteAll(_filesToUpload);   _filesToUpload.clear();
    qDeleteAll(_nntpConnections); _nntpConnections.clear();
    qDeleteAll(_threadPool);      _threadPool.clear();
    if (_file)
    {
        _file->close();
        delete _file;
        _file = nullptr;
    }

    if (_hmi)
        _hmi->setIDLE();
}

bool NgPost::startPosting()
{
    qDebug() << "start posting... nzb: " << nzbPath();

    _stopPosting        = 0x0;
    _noMoreFiles        = 0x0;
    _nbPosted           = 0;
    _nbArticlesUploaded = 0;
    _nbArticlesFailed   = 0;
    _uploadedSize       = 0;
    _totalSize          = 0;
    _nntpFile           = nullptr;
    _file               = nullptr;
    _articles.clear();

    if (_nbThreads < 1)
        _nbThreads = 1;
    else if (_nbThreads > QThread::idealThreadCount())
        _nbThreads = QThread::idealThreadCount();


    if (!_nzb->open(QIODevice::WriteOnly))
    {
        _error(QString("Error: Can't create nzb output file: %1").arg(nzbPath()));
        return false;
    }
    else
    {
        QString tab = space();
        _nzbStream.setDevice(_nzb);
        _nzbStream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   << "<!DOCTYPE nzb PUBLIC \"-//newzBin//DTD NZB 1.1//EN\" \"http://www.newzbin.com/DTD/nzb/nzb-1.1.dtd\">\n"
                   << "<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n";

        if (_meta.size())
        {
            _nzbStream << tab << "<head>\n";
            for (auto itMeta = _meta.cbegin(); itMeta != _meta.cend() ; ++itMeta)
                _nzbStream << tab << tab << "<meta type=\"" << itMeta.key() << "\">" << itMeta.value() << "</meta>\n";
            _nzbStream << tab << "</head>\n\n";
        }
        _nzbStream << flush;
    }

    int  nbTh = _nbThreads, nbCon = _createNntpConnections();
    if (!nbCon)
    {
        _error("Error: there are no NntpConnection...");
        return false;
    }

    _timeStart.start();

    if (nbTh > nbCon)
        nbTh = nbCon; // we can't have more thread than available connections


    _threadPool.reserve(nbTh);
    int nbConPerThread = static_cast<int>(std::floor(nbCon / nbTh));
    int nbExtraCon     = nbCon - nbConPerThread * nbTh;
    qDebug() << "[NgPost::startPosting] nbFiles: " << _filesToUpload.size()
             << ", nbThreads: " << nbTh
             << ", nbCons: " << nbCon
             << " => nbCon per Threads: " << nbConPerThread
             << " (nbExtraCon: " << nbExtraCon << ")";

    int conIdx = 0;
    for (int threadIdx = 0; threadIdx < nbTh ; ++threadIdx)
    {
        QThread *thread = new QThread();
        QString threadName = QString("Thread #%1").arg(threadIdx+1);
        thread->setObjectName(threadName);
        _threadPool.append(thread);

        for (int i = 0 ; i < nbConPerThread ; ++i)
            _startConnectionInThread(conIdx++, thread, threadName);

        if (nbExtraCon-- > 0)
            _startConnectionInThread(conIdx++, thread, threadName);
        thread->start();
    }    

    // Prepare 2 Articles for each connections
    _prepareArticles();

    if (_hmi || _dispProgressBar)
    {
        connect(&_progressTimer, &QTimer::timeout, this, &NgPost::onRefreshProgressBar, Qt::DirectConnection);
        _progressTimer.start(_refreshRate);
    }

    return true;
}

void NgPost::updateGroups(const QString &groups)
{
    _groups = groups.toStdString();

    _grpList.clear();
    for (const QString &grp : groups.split(","))
        _grpList << grp;
}

void NgPost::_startConnectionInThread(int conIdx, QThread *thread, const QString &threadName)
{
    NntpConnection *con = _nntpConnections.at(conIdx);
    con->setThreadName(threadName);
    con->moveToThread(thread);
    emit con->startConnection();
}


int NgPost::startEventLoop()
{
    return _app->exec();
}

#include <QSslSocket>
int NgPost::startHMI()
{
    parseDefaultConfig();
    if (_from.empty())
            _from = _randomFrom();
#ifdef __DEBUG__
    _dumpParams();
#endif
    _hmi->init();
    _hmi->show();

    _log(QString("SSL support: %1, build version: %2, system version: %3").arg(QSslSocket::supportsSsl() ? "yes" : "no").arg(
                 QSslSocket::sslLibraryBuildVersionString()).arg(QSslSocket::sslLibraryVersionString()));
    return _app->exec();
}

NntpArticle *NgPost::getNextArticle(const QString &threadName)
{
    if (_stopPosting.load())
        return nullptr;

#ifdef __USE_MUTEX__
    _secureArticlesQueue.lock();
#endif

    NntpArticle *article = nullptr;
    if (_articles.size())
    {
#ifdef __DEBUG__
        emit log(tr("[%1][NgPost::getNextArticle] _articles.size() = %2").arg(threadName).arg(_articles.size()));
#endif
        article = _articles.dequeue();
        _secureArticlesQueue.unlock();
    }
    else
    {
        _secureArticlesQueue.unlock();
        if (!_noMoreFiles.load())
        {
            // we should never come here as the goal is to have articles prepared in advance in the queue

            if (_debug)
                emit log(tr("[%1][NgPost::getNextArticle] no article prepared...").arg(threadName));

            article = _prepareNextArticle(threadName, false);
        }
    }

    if (article)
        emit scheduleNextArticle(); // schedule the preparation of another Article in main thread

    return article;
}


void NgPost::onNntpFileStartPosting()
{
    NntpFile *nntpFile = static_cast<NntpFile*>(sender());
    _cout << "[avg. speed: " << avgSpeed() << "] >>>>> " << nntpFile->name() << "\n" << flush;
}

void NgPost::onNntpFilePosted()
{
    NntpFile *nntpFile = static_cast<NntpFile*>(sender());
    _totalSize += nntpFile->fileSize();
    ++_nbPosted;
    if (_hmi)
        _hmi->setFilePosted(nntpFile);

    if (_dispFilesPosting)
        _cout << "[avg. speed: " << avgSpeed() << "] <<<<< " << nntpFile->name() << "\n" << flush;

    nntpFile->writeToNZB(_nzbStream, _articleIdSignature.c_str());
    _filesInProgress.remove(nntpFile);
    emit nntpFile->scheduleDeletion();
    if (_nbPosted == _nbFiles)
    {
#ifdef __DEBUG__
        _log(QString("All files have been posted => closing application (nb article uploaded: %1, failed: %2)").arg(
                 _nbArticlesUploaded).arg(_nbArticlesFailed));
#endif
        if (_compressDir)
            _cleanCompressDir();

        if (_hmi)
            _finishPosting();
        else
            qApp->quit();
    }
}

void NgPost::onLog(QString msg)
{
    _log(msg);
}

void NgPost::onError(QString msg)
{
    _error(msg);
}

void NgPost::onDisconnectedConnection(NntpConnection *con)
{
    if (_stopPosting.load())
        return; // we're destructing all the connections

    _error(tr("Error: disconnected connection: #%1\n").arg(con->getId()));
    _nntpConnections.removeOne(con);
    delete con;
    if (_nntpConnections.isEmpty())
    {
        _error(tr("we lost all the connections..."));
        if (_hmi)
            _finishPosting();
        else
        {
            _error(tr(" => closing application"));
            qApp->quit();
        }
    }
}

void NgPost::onPrepareNextArticle()
{
    _prepareNextArticle(sMainThreadName);
}

void NgPost::onErrorConnecting(QString err)
{
    _error(err);
}



#include <iostream>
void NgPost:: onRefreshProgressBar()
{
    if (_hmi)
        _hmi->updateProgressBar();
    else
    {
        float progress = _nbArticlesUploaded;
        progress /= _nbArticlesTotal;

        qDebug() << "[NgPost::onRefreshProgressBar] uploaded: " << _nbArticlesUploaded
                 << " / " << _nbArticlesTotal
                 << " => progress: " << progress << "\n";

        std::cout << "\r[";
        int pos = static_cast<int>(std::floor(progress * sProgressBarWidth));
        for (int i = 0; i < sProgressBarWidth; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << int(progress * 100) << " %"
                  << " (" << _nbArticlesUploaded << " / " << _nbArticlesTotal << ")"
                  << " avg. speed: " << avgSpeed().toStdString();
        std::cout.flush();
    }
    if (_nbArticlesUploaded < _nbArticlesTotal)
        _progressTimer.start(_refreshRate);
}



#include <QNetworkReply>
#include <QMessageBox>
#include <QRegularExpression>
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
                    msg += tr("<br/><br/>You can download it from the <a href='https://github.com/mbruel/ngPost/tree/master/release'>release directory</a>");
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
}

#include <QDesktopServices>
void NgPost::onDonation()
{
    QDesktopServices::openUrl(sDonationURL);
}



void NgPost::_initPosting(const QList<QFileInfo> &filesToUpload)
{
    // For HMI mode, we might need to clean if the posting failed from the beginning
    _cleanInit();

    // initialize buffer and nzb file
    _buffer  = new char[articleSize()+1];
    if (!_nzbName.endsWith(".nzb"))
        _nzbName += ".nzb";
    _nzb     = new QFile(nzbPath());
    _nbFiles = filesToUpload.size();

    // initialize the NntpFiles (active objects)
    _filesToUpload.reserve(filesToUpload.size());
    int fileNum = 0;
    for (const QFileInfo &file : filesToUpload)
    {
        NntpFile *nntpFile = new NntpFile(this, file, ++fileNum, _nbFiles, _grpList);
        connect(nntpFile, &NntpFile::allArticlesArePosted, this, &NgPost::onNntpFilePosted, Qt::QueuedConnection);
        if (_dispFilesPosting && debugMode())
            connect(nntpFile, &NntpFile::startPosting, this, &NgPost::onNntpFileStartPosting, Qt::QueuedConnection);

        _filesToUpload.enqueue(nntpFile);
        _nbArticlesTotal += nntpFile->nbArticles();
    }
}

void NgPost::_cleanInit()
{
    if (_buffer)
        delete _buffer;
    if (_nzb)
        delete _nzb;
    qDeleteAll(_filesToUpload);_filesToUpload.clear();
}


#ifndef __USE_MUTEX__
void NgPost::onRequestArticle(NntpConnection *con)
{
    NntpArticle * article = getNextArticle();
    if (article)
        emit con->pushArticle(article);
}
#endif

int NgPost::_createNntpConnections()
{
    _nbConnections = 0;
    for (NntpServerParams *srvParams : _nntpServers)
        _nbConnections += srvParams->nbCons;

    _nntpConnections.reserve(_nbConnections);
    int conIdx = 0;
    for (NntpServerParams *srvParams : _nntpServers)
    {
        for (int k = 0 ; k < srvParams->nbCons ; ++k)
        {
            NntpConnection *nntpCon = new NntpConnection(this, ++conIdx, *srvParams);
            connect(nntpCon, &NntpConnection::log, this, &NgPost::onLog, Qt::QueuedConnection);
            connect(nntpCon, &NntpConnection::error, this, &NgPost::onError, Qt::QueuedConnection);
            connect(nntpCon, &NntpConnection::errorConnecting, this, &NgPost::onErrorConnecting, Qt::QueuedConnection);
            connect(nntpCon, &NntpConnection::disconnected, this, &NgPost::onDisconnectedConnection, Qt::QueuedConnection);
#ifndef __USE_MUTEX__
            connect(nntpCon, &NntpConnection::requestArticle, this, &NgPost::onRequestArticle, Qt::QueuedConnection);
#endif
            _nntpConnections.append(nntpCon);
        }
    }

    _log(tr("Number of available Nntp Connections: %1").arg(_nbConnections));
    return _nbConnections;
}


void NgPost::_closeNzb()
{
    if (_nzb)
    {
        if (_nzb->isOpen())
        {
            _nzbStream << "</nzb>\n";
            _nzb->close();
        }
        delete _nzb;
        _nzb = nullptr;
    }
}

void NgPost::_printStats() const
{
    QString unit = "B";
    double size = _totalSize;
    if (size > 1024)
    {
        size /= 1024;
        unit = "kB";
    }
    if (size > 1024)
    {
        size /= 1024;
        unit = "MB";
    }

    int duration = _timeStart.elapsed();
    double sec = duration/1000;

    QString msgEnd = tr("\nUpload size: %1 %2 in %3 (%4 sec) \
=> average speed: %5 (%6 connections on %7 threads)\n").arg(
                size).arg(unit).arg(
                QTime::fromMSecsSinceStartOfDay(duration).toString("hh:mm:ss.zzz")).arg(sec).arg(
                avgSpeed()).arg(_nntpConnections.size()).arg(_threadPool.size());

    if (_nbArticlesFailed > 0)
        msgEnd += tr("%1 / %2 articles FAILED to be uploaded (even with %3 retries)...\n").arg(
                      _nbArticlesFailed).arg(_nbArticlesTotal).arg(NntpArticle::nbMaxTrySending());

    if (_nzb)
    {
        msgEnd += QString("nzb file: %1\n").arg(nzbPath());
        if (_doCompress)
        {
            msgEnd += QString("file: %1, rar name: %2").arg(_nzbName).arg(_rarName);
            if (!_rarPass.isEmpty())
                msgEnd += QString(", rar pass: %1").arg(_rarPass);
        }
        msgEnd += "\n";
    }

    _log(msgEnd);
}

void NgPost::_log(const QString &aMsg, bool newline) const
{
    if (_hmi)
        _hmi->log(aMsg, newline);
    else
    {
        _cout << aMsg;
        if (newline)
            _cout << "\n";
        _cout << flush;
    }
}

void NgPost::_error(const QString &error) const
{
    if (_hmi)
        _hmi->logError(error);
    else
        _cerr << error << flush << "\n" << flush;
}

void NgPost::_prepareArticles()
{
    int nbPreparedArticlePerConnection = NB_ARTICLES_TO_PREPARE_PER_CONNECTION;

#ifdef __USE_MUTEX__
    int nbArticlesToPrepare = nbPreparedArticlePerConnection * _nntpConnections.size();
  #ifdef __DEBUG__
    _cout << "[NgPost::_prepareArticles] >>>> preparing " << nbPreparedArticlePerConnection
          << " articles for each connections. Nb cons = " << _nntpConnections.size()
          << " => should prepare " << nbArticlesToPrepare << " articles!\n";
  #endif
    for (int i = 0; i < nbArticlesToPrepare ; ++i)
    {
        if (!_prepareNextArticle(sMainThreadName))
        {
  #ifdef __DEBUG__
            _cout << "[NgPost::_prepareArticles] No more Articles to produce after i = " << i << "\n";
  #endif
            break;
        }
    }
  #ifdef __DEBUG__
    _cout << "[NgPost::_prepareArticles] <<<<< finish preparing "
          << ", current article queue size: " << _articles.size() << "\n" << flush;
  #endif
#else
    for (int i = 0 ; i < nbPreparedArticlePerConnection ; ++i)
    {
        for (NntpConnection *con : _nntpConnections)
        {
            NntpArticle *article = getNextArticle();
            if (article)
                emit con->pushArticle(article);
            else
                return;

        }
    }
#endif
}


NntpArticle *NgPost::_prepareNextArticle(const QString &threadName, bool fillQueue)
{
    NntpArticle *article = _getNextArticle(threadName);
    if (article && fillQueue)
    {
        QMutexLocker lock(&_secureArticlesQueue);
        _articles.enqueue(article);
    }
    return article;
}

std::string NgPost::_randomFrom() const
{
    QString randomFrom, alphabet("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.");
    int nb = 8, nbLetters = alphabet.length();
    for (int i = 0 ; i < nb ; ++i)
        randomFrom.append(alphabet.at(std::rand()%nbLetters));

    randomFrom += QString("@%1.com").arg(_articleIdSignature.c_str());
    return randomFrom.toStdString();
}

QString NgPost::randomFrom() const
{
    QString randomFrom, alphabet("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    int nb = 8, nbLetters = alphabet.length();
    for (int i = 0 ; i < nb ; ++i)
        randomFrom.append(alphabet.at(std::rand()%nbLetters));

    randomFrom += QString("@%1.com").arg(_articleIdSignature.c_str());
    return randomFrom;
}

QString NgPost::randomPass(uint length) const
{
    QString pass, alphabet("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    int nbLetters = alphabet.length();
    for (uint i = 0 ; i < length ; ++i)
        pass.append(alphabet.at(std::rand()%nbLetters));

    return pass;
}

NntpArticle *NgPost::_getNextArticle(const QString &threadName)
{
    _secureFile.lock();
    if (!_nntpFile)
    {
        _nntpFile = _getNextFile();
        if (!_nntpFile)
        {
            _noMoreFiles = 0x1;
#ifdef __DEBUG__
            emit log(tr("[%1] No more file to post...").arg(threadName));
#endif
            _secureFile.unlock();
            return nullptr;
        }
    }

    if (!_file)
    {
        _file = new QFile(_nntpFile->path());
        if (_file->open(QIODevice::ReadOnly))
        {
            if (_debug)
                emit log(tr("[%1] starting processing file %2").arg(threadName).arg(_nntpFile->path()));
            _part = 0;
        }
        else
        {
            if (_debug)
                emit error(tr("[%1] Error: couldn't open file %2").arg(threadName).arg(_nntpFile->path()));
            else
                emit error(tr("Error: couldn't open file %1").arg(_nntpFile->path()));
            delete _file;
            _file = nullptr;
            _nntpFile = nullptr;
            _secureFile.unlock();
            return _getNextArticle(threadName); // Check if we have more files
        }
    }


    if (_file)
    {
        qint64 pos   = _file->pos();
        qint64 bytes = _file->read(_buffer, articleSize());
        if (bytes > 0)
        {
            _buffer[bytes] = '\0';
            if (_debug)
                emit log(tr("[%1] we've read %2 bytes from %3 (=> new pos: %4)").arg(threadName).arg(bytes).arg(pos).arg(_file->pos()));
            ++_part;
            NntpArticle *article = new NntpArticle(_nntpFile, _part, _buffer, pos, bytes,
                                                   _obfuscateArticles ? _randomFrom() : _from,
                                                   _groups, _obfuscateArticles);

#ifdef __SAVE_ARTICLES__
            article->dumpToFile("/tmp", _articleIdSignature);
#endif
            _secureFile.unlock();
            return article;
        }
        else
        {
            if (_debug)
                emit log(tr("[%1] finished processing file %2").arg(threadName).arg(_nntpFile->path()));

            _file->close();
            delete _file;
            _file = nullptr;
            _nntpFile = nullptr;
        }
    }

    _secureFile.unlock();
    return _getNextArticle(threadName); // if we didn't have an Article, check next file
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

    if (!parser.parse(args))
    {
        _error(tr("Error syntax: %1\nTo list the available options use: %2 --help\n").arg(parser.errorText()).arg(argv[0]));
        return false;
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

    if (!parser.isSet(sOptionNames[Opt::INPUT]))
    {
        _error("Error syntax: you should provide at least one input file or directory using the option -i");
        return false;
    }

    if (parser.isSet(sOptionNames[Opt::CONF]))
    {
        QString err = _parseConfig(parser.value(sOptionNames[Opt::CONF]));
        if (!err.isEmpty())
        {
            _error(err);
            return false;
        }
    }
    else
    {
        QString err = parseDefaultConfig();
        if (!err.isEmpty())
        {
            _error(err);
            return false;
        }
    }

    if (parser.isSet(sOptionNames[Opt::OBFUSCATE]))
    {
        _obfuscateArticles = true;
        _cout << "Do article obfuscation (the subject of each Article will be a UUID)\n" << flush;
    }

    if (parser.isSet(sOptionNames[Opt::DEBUG]))
    {
        _debug = true;
        _cout << "Debug logs are ON\n" << flush;
    }

    if (parser.isSet(sOptionNames[Opt::THREAD]))
    {
        bool ok;
        _nbThreads = parser.value(sOptionNames[Opt::THREAD]).toInt(&ok);
        if (_nbThreads < 1)
            _nbThreads = 1;
        if (!ok)
        {
            _error(tr("You should give an integer for the number of threads (option -t)"));
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
    else if (_from.empty())
        _from = _randomFrom();


    if (parser.isSet(sOptionNames[Opt::DISP_PROGRESS]))
    {
        QString val = parser.value(sOptionNames[Opt::DISP_PROGRESS]);
        val = val.trimmed().toLower();
        if (val == "bar")
        {
            _dispProgressBar  = true;
            _dispFilesPosting = false;
            qDebug() << "Display progress bar\n";
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
        _articleIdSignature = escapeXML(parser.value(sOptionNames[Opt::MSG_ID])).toStdString();

    if (parser.isSet(sOptionNames[Opt::ARTICLE_SIZE]))
    {
        bool ok;
        int size = parser.value(sOptionNames[Opt::ARTICLE_SIZE]).toInt(&ok);
        if (ok)
            sArticleSize = size;
        else
        {
            _error(tr("You should give an integer for the article size (option -a)"));
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
            _error(tr("You should give an unisgned integer for the number of retry for posting an Article (option -r)"));
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
    {
        _doPar2 = true;
        if (_par2Pct == 0)
        {
            _error(tr("Error: can't generate par2 if the redundancy percentage is null...\nEither use --par2_pct or set PAR2_PCT in the config file."));
            return false;
        }
    }
    if (parser.isSet(sOptionNames[Opt::GEN_NAME]))
        _genName = true;
    if (parser.isSet(sOptionNames[Opt::GEN_PASS]))
        _genPass = true;

    if (parser.isSet(sOptionNames[Opt::RAR_NAME]))
        _rarName = parser.value(sOptionNames[Opt::RAR_NAME]);
    if (parser.isSet(sOptionNames[Opt::RAR_PASS]))
        _rarPass = parser.value(sOptionNames[Opt::RAR_PASS]);

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



    // Server Section under
    // check if the server params are given in the command line
    if (parser.isSet(sOptionNames[Opt::HOST]))
    {
        QString host = parser.value(sOptionNames[Opt::HOST]);


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
                _error(tr("You should give an integer for the port (option -P)"));
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
                _error(tr("You should give an integer for the number of connections (option -n)"));
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
            _error(tr("Error: the input file '%1' is not readable...").arg(parser.value("input")));
            return false;
        }
        else
        {
            if (_nzbName.isEmpty())
                _nzbName = fileInfo.fileName(); // The first file will be used
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
                        _error(tr("Error: the input file '%1' is not readable...").arg(subFile.absoluteFilePath()));
                        return false;
                    }
                }
            }
        }
    }

    if (_rarName.isEmpty())
        _rarName = _nzbName;
    if (_doCompress)
    {
        if (!_canCompress())
            return false;

        if (_genName)
            _rarName = randomPass(_lengthName);

        if (_genPass)
        {
            _rarPass = randomPass(_lengthPass);
            _meta.insert("password", _rarPass);
        }

        if (_compressFiles(_rarPath, _tmpPath, _rarName, filesPath, _rarPass, _rarSize) !=0 )
            return false;
    }
    if (_doPar2)
    {
        if (!_canGenPar2())
            return false;

        if (_genPar2(_tmpPath, _rarName, _par2Pct, filesPath) != 0)
            return false;
    }

    if (_compressDir)
    {
        filesToUpload.clear();
        if (_debug)
            _cout << "Files to upload:\n";
        for (const QFileInfo & file : _compressDir->entryInfoList(QDir::Files, QDir::Name))
        {
            filesToUpload << file;
            if (_debug)
                _cout << "    - " << file.fileName() << "\n";

        }

        if (!_doCompress && _doPar2)
        {
            for (const QString &path : filesPath)
            {
                filesToUpload << path;
                if (_debug)
                    _cout << "    - " << path << "\n";
            }
        }
        _cout << "\n" << flush;
    }


    if (parser.isSet("o"))
    {
        QFileInfo nzb(parser.value(sOptionNames[Opt::OUTPUT]));
        _nzbName = nzb.fileName();
        _nzbPath = nzb.absolutePath();
    }

    _initPosting(filesToUpload);

#ifdef __DEBUG__
    _dumpParams();
#endif

    return true;
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
                if (args.size() == 2)
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
                        val = val.trimmed().toLower();
                        if (val == "bar")
                        {
                            _dispProgressBar = true;
                            qDebug() << "Display progress bar\n";
                        }
                        else if (val == "files")
                        {
                            _dispFilesPosting = true;
                            qDebug() << "Display Files when start posting\n";
                        }
                    }
                    else if (opt == sOptionNames[Opt::MSG_ID])
                    {
                        _articleIdSignature = val.toStdString();
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
                    }
                    else if (opt == sOptionNames[Opt::GROUPS])
                    {
                        updateGroups(val);
                    }


                    else if (opt == sOptionNames[Opt::INPUT_DIR])
                        _inputDir = val;



                    // compression section
                    else if (opt == sOptionNames[Opt::TMP_DIR])
                        _tmpPath = val;
                    else if (opt == sOptionNames[Opt::RAR_PATH])
                        _rarPath = val;
                    else if (opt == sOptionNames[Opt::RAR_EXTRA])
                        _rarArgs = val;
                    else if (opt == sOptionNames[Opt::RAR_SIZE])
                    {
                        uint nb = val.toUInt(&ok);
                        if (ok)
                            _rarSize = nb;
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
                                _par2Path = val;
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
                        val = val.trimmed();
                        if (val == "true" || val == "on" || val == "1")
                        {
                            serverParams->useSSL = true;
                            if (serverParams->port == NntpServerParams::sDefaultPort)
                                serverParams->port = NntpServerParams::sDefaultSslPort;
                        }
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
    return err;
}

void NgPost::_syntax(char *appName)
{
//    QString appVersion = QString("%1_v%2").arg(sAppName).arg(sVersion);
    QString app = QFileInfo(appName).fileName();
    _cout << sNgPostDesc << "\n"
          << "Syntax: " << app << " (options)? (-i <file or directory to upload>)+\n";
    for (const QCommandLineOption & opt : sCmdOptions)
    {
        if (opt.valueName() == sOptionNames[Opt::HOST])
            _cout << "\n// without config file, you can provide all the parameters to connect to ONE SINGLE server\n";
        else if (opt.valueName() == sOptionNames[Opt::TMP_DIR])
            _cout << "\n// for compression and par2 support\n";

        if (opt.names().size() == 1)
            _cout << QString("\t--%1: %2\n").arg(opt.names().first(), -17).arg(opt.description());
        else
            _cout << QString("\t-%1: %2\n").arg(opt.names().join(" or --"), -18).arg(opt.description());
    }

    _cout << "\nExamples:\n"
          << "  - with compression, filename obfuscation, random password and par2: " << app << " -i /tmp/file1 -i /tmp/folder1 -o /nzb/myPost.nzb --compress --gen_name --gen_pass --gen_par2\n"
          << "  - with config file: " << app << " -c ~/.ngPost -m \"password=qwerty42\" -f ngPost@nowhere.com -i /tmp/file1 -i /tmp/file2 -i /tmp/folderToPost1 -i /tmp/folderToPost2\n"
          << "  - with all params:  " << app << " -t 1 -m \"password=qwerty42\" -m \"metaKey=someValue\" -h news.newshosting.com -P 443 -s -u user -p pass -n 30 -f ngPost@nowhere.com \
 -g \"alt.binaries.test,alt.binaries.test2\" -a 64000 -i /tmp/folderToPost -o /tmp/folderToPost.nzb\n"
          << "\nIf you don't provide the output file (nzb file), we will create it in the nzbPath with the name of the first file or folder given in the command line.\n"
          << "so in the second example above, the nzb would be: /tmp/file1.nzb\n"
          << flush;
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

#ifdef __DEBUG__
void NgPost::_dumpParams() const
{
    QString servers;
    for (NntpServerParams *srv : _nntpServers)
        servers += srv->str() + " ";
    qDebug() << "[NgPost::_dumpParams]>>>>>>>>>>>>>>>>>>\n"
             << "nbThreads: " << _nbThreads << " nb Inputs: " << _nbFiles
             << ", nzbPath: " << _nzbPath << ", nzbName" << _nzbName
             << ", inputDir: " << _inputDir
             << "\nnb Servers: " << _nntpServers.size() << ": " << servers
             << "\nfrom: " << _from.c_str() << ", groups: " << _groups.c_str()
             << "\narticleSize: " << sArticleSize
             << ", obfuscate articles: " << _obfuscateArticles
             << ", display progress bar: " << _dispProgressBar
             << ", display posting files: " << _dispFilesPosting
             << "\ncompression settings: <tmp_path: " << _tmpPath << ">"
             << ", <rar_path: " << _rarPath << ">"
             << ", <rar_size: " << _rarSize << ">"
             << ", <par2_pct: " << _par2Pct << ">"
             << ", <par2_path: " << _par2Path << ">"
             << ", <par2_args: " << _par2Args << ">"
             << "\ncompress: " << _doCompress << ", doPar2: " << _doPar2
             << ", gen_name: " << _genName << ", genPass: " << _genPass
             << ", rarName: " << _rarName << ", rarPass: " << _rarPass
             << ", lengthName: " << _lengthName << ", lengthPass: " << _lengthPass
             << "\n[NgPost::_dumpParams]<<<<<<<<<<<<<<<<<<\n";
}
#endif

#include <QProcess>
int NgPost::compressFiles(const QStringList &files)
{
    if (!_canCompress() || (_doPar2 && !_canGenPar2()))
        return -1;

    // 1.: Compress
    int exitCode = _compressFiles(_rarPath, _tmpPath, _rarName, files, _rarPass, _rarSize);

    if (exitCode == 0 && _doPar2)
        exitCode = _genPar2(_tmpPath, _rarName, _par2Pct, files);

    if (exitCode == 0)
        _log("Ready to post!");

    return exitCode;
}

int NgPost::_compressFiles(const QString &cmdRar,
                           const QString &tmpFolder,
                           const QString &archiveName,
                           const QStringList &files,
                           const QString &pass,
                           uint volSize)
{
    // 1.: create archive temporary folder
    QString archiveTmpFolder = _createArchiveFolder(tmpFolder, archiveName);
    if (archiveTmpFolder.isEmpty())
        return -1;

    _extProc = new QProcess(this);
    connect(_extProc, &QProcess::readyReadStandardOutput, this, &NgPost::onExtProcReadyReadStandardOutput, Qt::DirectConnection);
    connect(_extProc, &QProcess::readyReadStandardError,  this, &NgPost::onExtProcReadyReadStandardError,  Qt::DirectConnection);

    bool is7z = false;
    if (_rarPath.contains("7z"))
    {
        is7z = true;
        if (_rarArgs.isEmpty())
            _rarArgs = sDefault7zOptions;
    }
    else
    {
        if (_rarArgs.isEmpty())
            _rarArgs = sDefaultRarExtraOptions;
    }

    // 2.: create rar args (rar a -v50m -ed -ep1 -m0 -hp"$PASS" "$TMP_FOLDER/$RAR_NAME.rar" "${FILES[@]}")
//    QStringList args = {"a", "-idp", "-ep1", compressLevel, QString("%1/%2.rar").arg(archiveTmpFolder).arg(archiveName)};
    QStringList args = _rarArgs.split(" ");
    if (!args.contains("a"))
        args.prepend("a");
    if (!is7z && !args.contains("-idp"))
        args << "-idp";
    if (!pass.isEmpty())
    {
        if (is7z)
        {
            if (!args.contains("-mhe=on"))
                args << "-mhe=on";
            args << QString("-p%1").arg(pass);
        }
        else
            args << QString("-hp%1").arg(pass);
    }
    if (volSize > 0)
        args << QString("-v%1m").arg(volSize);

    args << QString("%1/%2.%3").arg(archiveTmpFolder).arg(archiveName).arg(is7z ? "7z" : "rar");
    if (!args.contains("-r"))
        args << "-r";

    args << files;

    // 3.: launch rar
    if (_debug || !_hmi)
        _log(tr("Compressing files: %1 %2\n").arg(cmdRar).arg(args.join(" ")));
    else
        _log("Compressing files...\n");
    _limitProcDisplay = false;
    _extProc->start(cmdRar, args);
    _extProc->waitForFinished(-1);
    int exitCode = _extProc->exitCode();
    if (_debug)
        _log(tr("=> rar exit code: %1\n").arg(exitCode));
    else
        _log("");


    // 4.: free process if no par2 after
    if (!_doPar2)
        _cleanExtProc();

    if (exitCode != 0)
    {
        _error(tr("Error during compression: %1").arg(exitCode));
        _cleanCompressDir();
    }

    return exitCode;
}

int NgPost::_genPar2(const QString &tmpFolder,
                     const QString &archiveName,
                     uint redundancy,
                     const QStringList &files)
{
    Q_UNUSED(files)

    QString archiveTmpFolder;
    QStringList args;
    if (_par2Args.isEmpty())
        args << "c" << "-l" << "-m1024" << QString("-r%1").arg(redundancy);
    else
        args << _par2Args.split(" ");

    // we've already compressed => we gen par2 for the files in the archive folder
    if (_extProc)
    {
        archiveTmpFolder = QString("%1/%2").arg(tmpFolder).arg(archiveName);
        if (_par2Path.toLower().contains("parpar"))
        {
            if (args.last().trimmed() != "-o")
                args << "-o";
            args << QString("%1/%2.par2").arg(archiveTmpFolder).arg(archiveName)
                 << "-R" << archiveTmpFolder;
        }
        else
        {
            args << QString("%1/%2.par2").arg(archiveTmpFolder).arg(archiveName)
                 << QString("%1/%2*rar").arg(archiveTmpFolder).arg(archiveName);
            if (_par2Args.isEmpty() && (_debug || !_hmi))
                args << "-q"; // remove the progress bar

        }
    }
    else
    {
        // par2 on Linux wants to have all files in the same folder and generate the par2 there
        // otherwise we get this error: Ignoring out of basepath source file
        // exitcode: 3
        _error("ngPost can't generate par2 without compression cause we'd have to copy all the files in a temporary folder...");
        return -1;

//        _extProc = new QProcess(this);
//        connect(_extProc, &QProcess::readyReadStandardOutput, this, &NgPost::onExtProcReadyReadStandardOutput, Qt::DirectConnection);
//        connect(_extProc, &QProcess::readyReadStandardOutput, this, &NgPost::onExtProcReadyReadStandardOutput, Qt::DirectConnection);

//        // 1.: create archive temporary folder
//        archiveTmpFolder = _createArchiveFolder(tmpFolder, archiveName);
//        if (archiveTmpFolder.isEmpty())
//            return -1;

//        args << QString("%1/%2.par2").arg(archiveTmpFolder).arg(archiveName);
//        for (const QString &file : files)
//            args << file;
    }


    if (_debug || !_hmi)
        _log(tr("Generating par2: %1 %2\n").arg(_par2Path).arg(args.join(" ")));
    else
        _log("Generating par2...\n");
    _limitProcDisplay = true;
    _nbProcDisp = 0;
    _extProc->start(_par2Path, args);
    _extProc->waitForFinished(-1);
    int exitCode = _extProc->exitCode();
    if (_debug)
        _log(tr("=> par2 exit code: %1\n").arg(exitCode));
    else
        _log("");


    if (exitCode != 0)
    {
        _error(tr("Error during par2 generation: %1").arg(exitCode));

        // parpar hack: it can abort after managing to create the par2 (apparently with node v10)
        if (_par2Path.toLower().contains("parpar"))
        {
            QFileInfo fi(QString("%1/%2.par2").arg(archiveTmpFolder).arg(archiveName));
            if (fi.exists())
            {
                _log(tr("=> parpar aborted but the par2 are generated!"));
                exitCode = 0;
            }
        }
    }

    // 4.: free process if no par2 after
    _cleanExtProc();

    if (exitCode != 0)
        _cleanCompressDir();

    return exitCode;
}

void NgPost::_cleanExtProc()
{
    delete _extProc;
    _extProc = nullptr;
}

void NgPost::_cleanCompressDir()
{
    _compressDir->removeRecursively();
    delete _compressDir;
    _compressDir = nullptr;
    if (_debug)
        _log("Compressed files deleted.");
}

QString NgPost::_createArchiveFolder(const QString &tmpFolder, const QString &archiveName)
{
    QString archiveTmpFolder = QString("%1/%2").arg(tmpFolder).arg(archiveName);
    _compressDir = new QDir(archiveTmpFolder);
    if (_compressDir->exists())
    {
        _error(tr("The temporary directory '%1' already exists... (either remove it or change the archive name)").arg(archiveTmpFolder));
        delete _compressDir;
        _compressDir = nullptr;
        return QString();
    }
    else
    {
        if (!_compressDir->mkpath("."))
        {
            _error(tr("Couldn't create the temporary folder: '%1'...").arg(archiveTmpFolder));
            delete _compressDir;
            _compressDir = nullptr;
            return QString();
        }
    }
    return archiveTmpFolder;
}


void NgPost::onExtProcReadyReadStandardOutput()
{
    if (_debug)
        _log(_extProc->readAllStandardOutput(), false);
    else
    {
        if (!_limitProcDisplay || ++_nbProcDisp%42 == 0)
            _log("*", false);
    }
    qApp->processEvents();
}

void NgPost::onExtProcReadyReadStandardError()
{
    _error(_extProc->readAllStandardError());
    qApp->processEvents();
}


bool NgPost::_checkTmpFolder() const
{
    if (_tmpPath.isEmpty())
    {
        _error(tr("NO_POSSIBLE_COMPRESSION: You must define the temporary directory..."));
        return false;
    }

    QFileInfo fi(_tmpPath);
    if (!fi.exists() || !fi.isDir() || !fi.isWritable())
    {
        _error(tr("ERROR: the temporary directory must be a WRITABLE directory..."));
        return false;
    }

    return true;
}

void NgPost::_showVersionASCII() const
{
    _cout << sNgPostASCII
          << "                          v" << sVersion << "\n\n" << flush;
}

bool NgPost::_canCompress() const
{
    //1.: the _tmp_folder must be writable
    if (!_checkTmpFolder())
        return false;

    //2.: check _rarPath is executable
    QFileInfo fi(_rarPath);
    if (!fi.exists() || !fi.isFile() || !fi.isExecutable())
    {
        _error(tr("ERROR: the RAR path is not executable..."));
        return false;
    }

    return true;
}

bool NgPost::_canGenPar2() const
{
    //1.: the _tmp_folder must be writable
    if (!_checkTmpFolder())
        return false;

    //2.: check _ is executable
    QFileInfo fi(_par2Path);
    if (!fi.exists() || !fi.isFile() || !fi.isExecutable())
    {
        _error(tr("ERROR: par2 is not available..."));
        return false;
    }

    return true;
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
        stream << "# ngPost configuration file\n"
               << "#\n"
               << "#\n"
               << "\n"
               << "## destination folder for all your nzb\n"
               << "## if you don't put anything, the nzb will be generated in the folder of ngPost on Windows and in /tmp on Linux\n"
               << "## this will be overwritten if you use the option -o with the full path of the nzb\n"
               << "nzbPath  = " << (_nzbPath.isEmpty() ? _nzbPathConf : _nzbPath) << "\n"
               << "\n"
               << "## Default folder to open to select files from the HMI\n"
               << "inputDir = " << _inputDir << "\n"
               << "\n"
               << "groups   = " << _groups.c_str() << "\n"
               << "\n"
               << "\n"
               << "## uncomment the next line if you want a fixed uploader email (in the nzb and in the header of each articles)\n"
               << "## if you let it commented, we'll generate a random email for the whole post\n"
               << "#from     = someone@ngPost.com\n"
               << "\n"
               << "\n"
               << "## uncomment the next line to limit the number of threads,  (by default it'll use the number of cores)\n"
               << "## all the connections are spread equally on those posting threads\n"
               << "thread  =  " << _nbThreads << "\n"
               << "\n"
               << "\n"
               << "## How to display progress in command line: NONE, BAR, FILES\n"
               << (_dispProgressBar  ? "" : "#") << "DISP_PROGRESS = BAR\n"
               << (_dispFilesPosting ? "" : "#") << "DISP_PROGRESS = FILES\n"
               << "\n"
               << "\n"
               << "## suffix of the msg_id for all the articles (cf nzb file)\n"
               << (_articleIdSignature == "ngPost" ? "#msg_id  =  ngPost\n" : QString("msg_id  =  %1\n").arg(_articleIdSignature.c_str()))
               << "\n"
               << "## article size (default 700k)\n"
               << "article_size = " << sArticleSize << "\n"
               << "\n"
               << "## number of retry to post an Article in case of failure (probably due to an already existing msg-id)\n"
               << "retry = " << NntpArticle::nbMaxTrySending() << "\n"
               << "\n"
               << "\n"
               << "## uncomment the following line to obfuscate the subjects of each Article\n"
               << "## /!\\ CAREFUL you won't find your post if you lose the nzb file /!\\\n"
               << (_obfuscateArticles ? "" : "#") << "obfuscate = article\n"
               << "\n"
               << "\n"
               << "\n"
               << "\n"
               << "##############################################################\n"
               << "##           Compression and par2 section                   ##\n"
               << "##############################################################\n"
               << "\n"
               << "## temporary folder where the compressed files and par2 will be stored\n"
               << "## so we can post directly a compressed (obfuscated or not) archive of the selected files\n"
               << "## /!\\ The directory MUST HAVE WRITE PERMISSION /!\\\n"
               << "## this is set for Linux environment, Windows users MUST change it\n"
               << "TMP_DIR = " << _tmpPath << "\n"
               << "\n"
               << "## RAR absolute file path (external application)\n"
               << "## /!\\ The file MUST EXIST and BE EXECUTABLE /!\\\n"
               << "## this is set for Linux environment, Windows users MUST change it\n"
               << "RAR_PATH = " << _rarPath << "\n"
               << "\n"
               << "## RAR EXTRA options (the first 'a' and '-idp' will be added automatically)\n"
               << "## -hp will be added if you use a password with --gen_pass, --rar_pass or using the HMI\n"
               << "## -v42m will be added with --rar_size or using the HMI\n"
               << "## you could change the compression level, lock the archive, add redundancy...\n"
               << "RAR_EXTRA = " << _rarArgs << "\n"
               << "\n"
               << "## size in MB of the RAR volumes (0 by default meaning NO split)\n"
               << "## feel free to change the value or to comment the next line if you don't want to split the archive\n"
               << "RAR_SIZE = " << _rarSize << "\n"
               << "\n"
               << "## par2 redundancy percentage (0 by default meaning NO par2 generation)\n"
               << "PAR2_PCT = " << _par2Pct << "\n"
               << "\n"
               << "## par2 (or alternative) absolute file path\n"
               << "## this is only useful if you compile from source (as par2 is included on Windows and the AppImage)\n"
               << "## or if you wish to use an alternative to par2 (for exemple Multipar on Windows)\n"
               << "## (in that case, you may need to set also PAR2_ARGS)\n"
               << "PAR2_PATH = " << _par2Path << "\n"
               << "#PAR2_PATH = par2j64.exe\n"
               << "\n"
               << "## fixed parameters for the par2 (or alternative) command\n"
               << "## you could for exemple use Multipar on Windows\n"
               << "#PAR2_ARGS = c -l -m1024 -r8 -s768000\n"
               << "#PAR2_ARGS = create /rr8 /lc40 /lr /rd2\n"
               << (_par2Args.isEmpty() ? "" : QString("PAR2_ARGS = %1\n").arg(_par2Args))
               << "\n"
               << "\n"
               << "## length of the random generated archive's file name\n"
               << "LENGTH_NAME = " << _lengthName << "\n"
               << "\n"
               << "## length of the random archive's passsword\n"
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
                   << "\n\n";
        }
        stream << "## You can add as many server if you have several providers by adding other \"server\" sections\n"
               << "#[server]\n"
               << "#host = news.otherprovider.com\n"
               << "#port = 563\n"
               << "#ssl  = true\n"
               << "#user = myOtherUser\n"
               << "#pass = myOtherPass\n"
               << "#connection = 15\n"
               << "\n";


        _log(tr("the config '%1' file has been updated").arg(conf));
        file.close();
    }
    else
        _error(tr("Error: Couldn't write default configuration file: ").arg(conf));

}

const QString NgPost::sNgPostASCII = QString("\
                   __________               __\n\
       ____    ____\\______   \\____  _______/  |_\n\
      /    \\  / ___\\|     ___/  _ \\/  ___/\\   __\\\n\
     |   |  \\/ /_/  >    |  (  <_> )___ \\  |  |\n\
     |___|  /\\___  /|____|   \\____/____  > |__|\n\
          \\//_____/                    \\/\n\
");

const QString NgPost::sNgPostDesc = QString("%1 is a CMD/GUI Usenet binary poster developped in C++11/Qt5:\n\
It is designed to be as fast as possible and offer all the main features to post data easily and safely.\n\n\
Here are the main features and advantages of ngPost:\n\
    - spread multiple connections (from multiple servers) on several threads\n\
    - post several files and/or directories\n\
    - automate rar compression with obfuscation and par2 generation\n\
    - generate the nzb\n\
    - full Article obfuscation: unique feature making all Aricles completely unrecognizable without the nzb\n\
    - ... \n\
for more details, cf https://github.com/mbruel/ngPost\n\
").arg(sAppName);
