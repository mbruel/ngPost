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
#ifdef __DISP_PROGRESS_BAR__
#include <iostream>
#endif
#include "hmi/MainWindow.h"



const QString NgPost::sAppName    = "ngPost";
const QString NgPost::sVersion    = QString::number(APP_VERSION);
const QString NgPost::sProFileURL = "https://raw.githubusercontent.com/mbruel/ngPost/master/src/ngPost.pro";

qint64        NgPost::sArticleSize = sDefaultArticleSize;
const QString NgPost::sSpace       = sDefaultSpace;

const QMap<NgPost::Opt, QString> NgPost::sOptionNames =
{
    {Opt::HELP,         "help"},
    {Opt::VERSION,      "version"},
    {Opt::CONF,         "conf"},
    {Opt::DISP_PROGRESS,"disp_progress"},

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
    _debug(false),
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
    _nntpFile(nullptr), _file(nullptr), _buffer(nullptr), _part(0),
    _articles(),
    _timeStart(), _totalSize(0),
    _meta(), _grpList(),
    _nbConnections(sDefaultNumberOfConnections), _nbThreads(QThread::idealThreadCount()),
    _socketTimeOut(sDefaultSocketTimeOut), _nzbPath(sDefaultNzbPath)
  #ifdef __DISP_PROGRESS_BAR__
  , _nbArticlesUploaded(0), _nbArticlesFailed(0),
    _uploadedSize(0), _nbArticlesTotal(0), _progressTimer(), _refreshRate(sDefaultRefreshRate)
  #endif
  , _stopPosting(0x0), _noMoreFiles(false), _noMoreArticles(false)
{
    if (_mode == AppMode::CMD)
        _app =  new QCoreApplication(argc, argv);
    else
    {
        _app = new QApplication(argc, argv);
        _hmi = new MainWindow(this);
        _hmi->setWindowTitle(QString("%1_v%2").arg(sAppName).arg(sVersion));
    }

    QThread::currentThread()->setObjectName("NgPost");
    connect(this, &NgPost::scheduleNextArticle, this, &NgPost::onPrepareNextArticle, Qt::QueuedConnection);

    // in case we want to generate random uploader (_from not provided)
    std::srand(static_cast<uint>(QDateTime::currentMSecsSinceEpoch()));

#ifdef __DEBUG__
    connect(this, &NgPost::log, this, &NgPost::onLog, Qt::QueuedConnection);
#endif
}




NgPost::~NgPost()
{
    _log("Destuction NgPost...");

    _finishPosting();
    qDeleteAll(_nntpServers);
    delete _app;
}

void NgPost::_finishPosting()
{
    _stopPosting = 0x1;

#ifdef __DISP_PROGRESS_BAR__
    if (_dispProgressBar)
        disconnect(&_progressTimer, &QTimer::timeout, this, &NgPost::onRefreshProgressBar);
    if (!_timeStart.isNull())
    {
        _nbArticlesUploaded = _nbArticlesTotal; // we might not have processed the last onArticlePosted
        _uploadedSize       = _totalSize;
        if (_dispProgressBar)
        {
            onRefreshProgressBar();
            std::cout << std::endl;
        }
    }
#endif

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


#ifdef __DISP_PROGRESS_BAR__
    if (_dispProgressBar)
    {
        disconnect(&_progressTimer, &QTimer::timeout, this, &NgPost::onRefreshProgressBar);
        if (!_timeStart.isNull())
        {
            onRefreshProgressBar();
            std::cout << std::endl;
        }
    }
#endif

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
    _noMoreFiles        = false;
    _noMoreArticles     = false;
    _nbPosted           = 0;
    _nbArticlesUploaded = 0;
    _nbArticlesFailed   = 0;
    _uploadedSize       = 0;
    _totalSize          = 0;
    _nntpFile           = nullptr;
    _file               = nullptr;
    _articles.clear();


    if (!_nzb->open(QIODevice::WriteOnly))
    {
        _error(QString("Error: Can't create nzb output file: %1/%2.nzb").arg(nzbPath()).arg(_nzbName));
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

    int  nbTh = nbThreads(), nbCon = _createNntpConnections();


    if (!nbCon)
    {
        qCritical() << "Error: there are no NntpConnection...";
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
        thread->setObjectName(QString("Thread #%1").arg(threadIdx+1));
        _threadPool.append(thread);

        for (int i = 0 ; i < nbConPerThread ; ++i)
        {
            NntpConnection *con = _nntpConnections.at(conIdx++);
            con->moveToThread(thread);
            con->startConnection();
        }
        if (nbExtraCon-- > 0)
        {
            NntpConnection *con = _nntpConnections.at(conIdx++);
            con->moveToThread(thread);
            emit con->startConnection();
        }
        thread->start();
    }    

    // Prepare 2 Articles for each connections
    _prepareArticles();

#ifdef __DISP_PROGRESS_BAR__
    if (_dispProgressBar)
    {
        connect(&_progressTimer, &QTimer::timeout, this, &NgPost::onRefreshProgressBar, Qt::DirectConnection);
        _progressTimer.start(_refreshRate);
    }
#endif

    return true;
}

int NgPost::startEventLoop()
{
    return _app->exec();
}

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
    return _app->exec();
}

NntpArticle *NgPost::getNextArticle()
{
#ifdef __USE_MUTEX__
    QMutexLocker lock(&_mutex);
#endif
    if (_noMoreArticles || _stopPosting.load())
        return nullptr;

    NntpArticle *article = nullptr;
    if (_articles.size())
        article = _articles.dequeue();
    else
    {
        // we should never come here as the goal is to have articles prepared in advance in the queue
        qDebug() << "[NgPost::getNextArticle] No article prepared...";
        article = _prepareNextArticle();
    }

    if (article)
        emit scheduleNextArticle(); // schedule the preparation of another Article in main thread

    return article;
}


void NgPost::onNntpFileStartPosting()
{
    NntpFile *nntpFile = static_cast<NntpFile*>(sender());
    _cout << "[avg. speed: " << avgSpeed()
          << "] starting " << nntpFile->name() << "\n" << flush;
}

void NgPost::onNntpFilePosted()
{
    NntpFile *nntpFile = static_cast<NntpFile*>(sender());
    _totalSize += nntpFile->fileSize();
    ++_nbPosted;
    if (_hmi)
        _hmi->setFilePosted(nntpFile->path());

    nntpFile->writeToNZB(_nzbStream, _articleIdSignature.c_str());
    _filesInProgress.remove(nntpFile);
    emit nntpFile->scheduleDeletion();
    if (_nbPosted == _nbFiles)
    {
#ifdef __DEBUG__
        _log(QString("All files have been posted => closing application (nb article uploaded: %1, failed: %2)").arg(
                 _nbArticlesUploaded).arg(_nbArticlesFailed));
#endif
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
    _prepareNextArticle();
}

void NgPost::onErrorConnecting(QString err)
{
    _error(err);
}

#ifdef __DISP_PROGRESS_BAR__
void NgPost::onArticlePosted(quint64 size)
{
    _uploadedSize += size;
    ++_nbArticlesUploaded;
}

void NgPost::onArticleFailed(quint64 size)
{
    _uploadedSize += size;
    ++_nbArticlesUploaded;
    ++_nbArticlesFailed;
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



void NgPost::_initPosting(const QList<QFileInfo> &filesToUpload)
{
    // For HMI mode, we might need to clean if the posting failed from the beginning
    _cleanInit();

    // initialize buffer and nzb file
    _buffer  = new char[articleSize()+1];
    _nzb     = new QFile(nzbPath());
    _nbFiles = filesToUpload.size();

    // initialize the NntpFiles (active objects)
    _filesToUpload.reserve(filesToUpload.size());
    int fileNum = 0;
    for (const QFileInfo &file : filesToUpload)
    {
        NntpFile *nntpFile = new NntpFile(file, ++fileNum, _nbFiles, _grpList);
        connect(nntpFile, &NntpFile::allArticlesArePosted, this, &NgPost::onNntpFilePosted, Qt::QueuedConnection);
        if (_dispFilesPosting)
            connect(nntpFile, &NntpFile::startPosting, this, &NgPost::onNntpFileStartPosting, Qt::QueuedConnection);

        _filesToUpload.enqueue(nntpFile);
#ifdef __DISP_PROGRESS_BAR__
        _nbArticlesTotal += nntpFile->nbArticles();
#endif
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


#endif

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
    int nbCon = 0;
    for (NntpServerParams *srvParams : _nntpServers)
        nbCon += srvParams->nbCons;

    _nntpConnections.reserve(nbCon);
    nbCon = 0;
    for (NntpServerParams *srvParams : _nntpServers)
    {
        for (int k = 0 ; k < srvParams->nbCons ; ++k)
        {
            NntpConnection *nntpCon = new NntpConnection(this, ++nbCon, *srvParams);
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

    _log(tr("Number of available Nntp Connections: %1").arg(nbCon));
    return nbCon;
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


    if (_hmi)
    {
        _hmi->log(msgEnd);
        if (_nzb)
            _hmi->log(QString("nzb file: %1\n").arg(nzbPath()));
    }
    else
    {
        _cout << msgEnd;
        if (_nzb)
            _cout << QString("nzb file: %1\n").arg(nzbPath());
        _cout << "\n" << flush;
    }
}

void NgPost::_log(const QString &aMsg) const
{
    qDebug() << QString("[%1] %2").arg(QThread::currentThread()->objectName()).arg(aMsg);
    if (_hmi)
        _hmi->log(aMsg);
}

void NgPost::_error(const QString &error) const
{
    if (_hmi)
        _hmi->logError(error);
    else
        _cerr << error << "\n" << flush;
}

void NgPost::_prepareArticles()
{
    int nbPreparedArticlePerConnection = 2;

#ifdef __USE_MUTEX__
    for (int i = 0; i < nbPreparedArticlePerConnection * _nntpConnections.size() ; ++i)
    {
        if (!_prepareNextArticle())
            break;
    }
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


NntpArticle *NgPost::_prepareNextArticle()
{
    NntpArticle *article = _getNextArticle();
    if (article)
        _articles.enqueue(article);
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

NntpArticle *NgPost::_getNextArticle()
{
    if (!_nntpFile)
    {
        _nntpFile = _getNextFile();
        if (!_nntpFile)
        {
            _noMoreFiles = 0x1;
#ifdef __DEBUG__
            emit log("No more file to post...");
#endif
            return nullptr;
        }
    }

    if (!_file)
    {
        _file = new QFile(_nntpFile->path());
        if (_file->open(QIODevice::ReadOnly))
        {
#ifdef __DEBUG__
            emit log(tr("starting processing file %1").arg(_nntpFile->path()));
#endif
            _part = 0;
        }
        else
        {
            emit log(tr("Error: couldn't open file %1").arg(_nntpFile->path()));
            delete _file;
            _file = nullptr;
            _nntpFile = nullptr;
            return _getNextArticle(); // Check if we have more files
        }
    }


    if (_file)
    {
        qint64 pos   = _file->pos();
        qint64 bytes = _file->read(_buffer, articleSize());
        if (bytes > 0)
        {
            _buffer[bytes] = '\0';
#ifdef __DEBUG__
            emit log(tr("we've read %1 bytes from %2 (=> new pos: %3)").arg(bytes).arg(pos).arg(_file->pos()));
#endif
            ++_part;
            NntpArticle *article = new NntpArticle(_nntpFile, _part, _buffer, pos, bytes,
                                                   _from,//_from.empty()?_randomFrom():_from,
                                                   _groups, _obfuscateArticles);

#ifdef __DISP_PROGRESS_BAR__
            connect(article, &NntpArticle::posted, this, &NgPost::onArticlePosted, Qt::QueuedConnection);
            connect(article, &NntpArticle::failed, this, &NgPost::onArticleFailed, Qt::QueuedConnection);
#endif

#ifdef __SAVE_ARTICLES__
            article->dumpToFile("/tmp", _articleIdSignature);
#endif
            return article;
        }
        else
        {
#ifdef __DEBUG__
            emit log(tr("finished processing file %1").arg(_nntpFile->path()));
#endif
            _file->close();
            delete _file;
            _file = nullptr;
            _nntpFile = nullptr;

            if (_noMoreFiles)
            {
                _noMoreArticles = 0x1;
                return nullptr;
            }
        }
    }

    return _getNextArticle(); // if we didn't have an Article, check next file
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
    qDebug() << "args: " << args
             << "=> parsing: " << res << " (error: " << parser.errorText() << ")";

    if (!parser.parse(args))
    {
        _error(QString("Error syntax: %1").arg(parser.errorText()));
        _syntax(argv[0]);
        return false;
    }

    if (parser.isSet(sOptionNames[Opt::HELP]) || parser.isSet(sOptionNames[Opt::VERSION]))
    {
        _syntax(argv[0]);
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
        _groups = parser.value(sOptionNames[Opt::GROUPS]).toStdString();

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

    // check if the server params are given in the command line
    if (parser.isSet(sOptionNames[Opt::HOST]))
    {
        QString host = parser.value(sOptionNames[Opt::HOST]);


        _nntpServers.clear();
        NntpServerParams *server = new NntpServerParams(host);
        _nntpServers.append(server);

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
            _nzbName = fileInfo.fileName(); // The last one will be kept
            if (fileInfo.isFile())
                filesToUpload.append(fileInfo);
            else
            {
                QDir dir(fileInfo.absoluteFilePath());
                for (const QFileInfo &subFile : dir.entryInfoList(QDir::Files, QDir::Name))
                {
                    if (subFile.isReadable())
                        filesToUpload.append(subFile);
                    else
                    {
                        _error(tr("Error: the input file '%1' is not readable...").arg(subFile.absoluteFilePath()));
                        return false;
                    }
                }
            }
        }
    }

    if (parser.isSet("o"))
    {
        QFileInfo nzb(parser.value(sOptionNames[Opt::OUTPUT]));
        _nzbName = nzb.fileName();
        _nzbPath = nzb.absolutePath();
    }


    QStringList grps = QString(_groups.c_str()).split(",");
    _grpList.reserve(grps.size());
    for (const QString &grp : grps)
        _grpList.append(grp);

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
                _nntpServers.append(serverParams);
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
                            _nzbPath = val;
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
                        _groups = val.toStdString();
                    }
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
    QString appVersion = QString("%1_v%2").arg(sAppName).arg(sVersion);
    QString app = QFileInfo(appName).fileName();
    _cout << appVersion << " is a NNTP poster that can:\n"
          << "  - use multiple connections on multiple servers. (for multiple server, you've to use a config file)\n"
          << "  - spread those connections on several threads (by default the number of cores available)\n"
          << "  - post several files or directory (you can put several times the option -i)\n"
          << "  - generate the nzb\n"
          << "  - use SSL encryption if desired\n"
          << "\n"
          << "Syntax: "
          << app << " (options)? (-i <file or directory to upload>)+"
          << "\n";
    for (const QCommandLineOption & opt : sCmdOptions)
    {
        if (opt.valueName() == sOptionNames[Opt::HOST])
            _cout << "\n// without config file, you can provide all the parameters to connect to ONE SINGLE server\n";
        if (opt.names().size() == 1)
            _cout << QString("\t--%1: %2\n").arg(opt.names().first(), -17).arg(opt.description());
        else
            _cout << QString("\t-%1: %2\n").arg(opt.names().join(" or --"), -18).arg(opt.description());
    }

    _cout << "\nExamples:\n"
          << "  - with config file: " << app << " -c ~/.ngPost -m \"password=qwerty42\" -f ngPost@nowhere.com -i /tmp/file1 -i /tmp/file2 -i /tmp/folderToPost1 -i /tmp/folderToPost2\n"
          << "  - with all params:  " << app << " -t 1 -m \"password=qwerty42\" -m \"metaKey=someValue\" -h news.newshosting.com -P 443 -s -u user -p pass -n 30 -f ngPost@nowhere.com \
             -g \"alt.binaries.test,alt.binaries.test2\" -a 64000 -i /tmp/folderToPost -o /tmp/folderToPost.nzb\n"
          << "\nIf you don't provide the output file (nzb file), we will create it in the nzbPath with the name of the last file or folder given in the command line.\n"
          << "so in the first example above, the nzb would be: /tmp/folderToPost2.nzb\n"
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
             << "\nnb Servers: " << _nntpServers.size() << ": " << servers
             << "\nfrom: " << _from.c_str() << ", groups: " << _groups.c_str()
             << "\narticleSize: " << sArticleSize
             << ", obfuscate articles: " << _obfuscateArticles
             << ", display progress bar: " << _dispProgressBar
             << ", display posting files: " << _dispFilesPosting
             << "\n[NgPost::_dumpParams]<<<<<<<<<<<<<<<<<<\n";
}
enum class Opt {HELP = 0, VERSION, CONF,
                INPUT, OUTPUT, THREAD, MSG_ID,
                META, ARTICLE_SIZE, FROM, GROUPS,
                HOST, PORT, SSL, USER, PASS, CONNECTION
               };
#endif
