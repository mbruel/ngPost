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
#include <QCoreApplication>
#include <QThread>
#include <QTextStream>
#include <QCommandLineParser>
#include <QDir>
#include <QDebug>
#ifdef __DISP_PROGRESS_BAR__
#include <iostream>
#endif




const QString NgPost::sAppName = "ngPost";
const QString NgPost::sVersion = "1.0.1";

qint64        NgPost::sArticleSize = sDefaultArticleSize;
const QString NgPost::sSpace       = sDefaultSpace;

const QMap<NgPost::Opt, QString> NgPost::sOptionNames =
{
    {Opt::HELP,         "help"},
    {Opt::VERSION,      "version"},
    {Opt::CONF,         "conf"},

    {Opt::INPUT,        "input"},
    {Opt::OUTPUT,       "output"},
    {Opt::NZB_PATH,     "nzbpath"},
    {Opt::THREAD,       "thread"},

    {Opt::REAL_NAME,    "real_name"},
    {Opt::MSG_ID,       "msg_id"},
    {Opt::META,         "meta"},
    {Opt::ARTICLE_SIZE, "article_size"},
    {Opt::FROM,         "from"},
    {Opt::GROUPS,       "groups"},

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
    {{"c", sOptionNames[Opt::CONF]},          tr( "use configuration file"), "file"},

    {{"i", sOptionNames[Opt::INPUT]},         tr("input file to upload (single file"), "file"},
    {{"o", sOptionNames[Opt::OUTPUT]},        tr("output file path (nzb)"), "file"},
    {{"t", sOptionNames[Opt::THREAD]},        tr("number of Threads (the connections will be distributed amongs them)"), "nb"},
    {{"r", sOptionNames[Opt::REAL_NAME]},     tr("real yEnc name in Article Headers (no obfuscation)")},

    {{"g", sOptionNames[Opt::GROUPS]},        tr("newsgroups where to post the files (coma separated without space)"), "str"},
    {{"m", sOptionNames[Opt::META]},          tr("extra meta data in header (typically \"password=qwerty42\")"), "key=val"},
    {{"f", sOptionNames[Opt::FROM]},          tr("uploader (in nzb file, article one is random)"), "email"},
    {{"a", sOptionNames[Opt::ARTICLE_SIZE]},  tr("article size (default one: %1)").arg(sDefaultArticleSize), "nb"},
    {{"z", sOptionNames[Opt::MSG_ID]},        tr("msg id signature, after the @ (default one: %1)").arg(sDefaultMsgIdSignature), "str"},

    // for a single server...
    {{"h", sOptionNames[Opt::HOST]},          tr("NNTP server hostname (or IP)"), sOptionNames[Opt::HOST]},
    {{"P", sOptionNames[Opt::PORT]},          tr("NNTP server port"), "nb"},
    {{"s", sOptionNames[Opt::SSL]},           tr("use SSL")},
    {{"u", sOptionNames[Opt::USER]},          tr("NNTP server username"), "str"},
    {{"p", sOptionNames[Opt::PASS]},          tr("NNTP server password"), "str"},
    {{"n", sOptionNames[Opt::CONNECTION]},    tr("number of NNTP connections"), "nb"},

};



NgPost::NgPost():
    QObject (),
    _nzbName(),
    _filesToUpload(), _filesInProgress(),
    _nbFiles(0), _nbPosted(0),
    _threadPool(), _nntpConnections(), _nntpServers(),
    _obfuscation(true), _from(),
    _groups(sDefaultGroups),
    _articleIdSignature(sDefaultMsgIdSignature),
    _nzb(nullptr), _nzbStream(),
    _nntpFile(nullptr), _file(nullptr), _buffer(nullptr), _part(0),
    _articles(),
    _timeStart(), _uploadSize(0),
    _meta(), _grpList(),
    _nbConnections(sDefaultNumberOfConnections), _nbThreads(QThread::idealThreadCount()),
    _socketTimeOut(sDefaultSocketTimeOut), _nzbPath(sDefaultNzbPath)
  #ifdef __DISP_PROGRESS_BAR__
  , _nbArticlesUploaded(0), _nbArticlesTotal(0), _progressTimer(), _refreshRate(200)
  #endif
{
    QThread::currentThread()->setObjectName("NgPost");
    connect(this, &NgPost::scheduleNextArticle, this, &NgPost::onPrepareNextArticle);

    // in case we want to generate random uploader (_from not provided)
    std::srand(static_cast<uint>(QDateTime::currentMSecsSinceEpoch()));
}




NgPost::~NgPost()
{
#ifdef __DISP_PROGRESS_BAR__
    disconnect(&_progressTimer, &QTimer::timeout, this, &NgPost::onRefreshProgressBar);
    if (!_timeStart.isNull())
    {
        onRefreshProgressBar();
        std::cout << std::endl;
    }
#endif

    _log("Destuction NgPost...");

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
        _log(tr("Stopping thread %1").arg(th->objectName()));
        th->quit();
        th->wait();
    }


    _log("All connections are closed...");

    _log(tr("prepared articles queue size: %1").arg(_articles.size()));


    // 5.: print out the list of files that havn't been posted
    // (in case of disconnection)
    int nbPendingFiles = _filesToUpload.size() + _filesInProgress.size();
    if (nbPendingFiles)
    {
        QTextStream cerr(stderr);
        cerr << "ERROR: there were " << nbPendingFiles << " on " << _nbFiles << " that havn't been posted:\n";
        for (NntpFile *file : _filesInProgress)
            cerr << "\t- " << file->path() << "\n";
        for (NntpFile *file : _filesToUpload)
            cerr << "\t- " << file->path() << "\n";
        cerr << "you can try to repost only those and concatenate the nzb with the current one ;)\n" << flush;
    }

    // 6.: free all resources
    qDeleteAll(_nntpServers);
    qDeleteAll(_filesInProgress);
    qDeleteAll(_filesToUpload);
    qDeleteAll(_nntpConnections);
    qDeleteAll(_threadPool);
}

bool NgPost::startPosting()
{
    if (!_nzb->open(QIODevice::WriteOnly))
    {
        qCritical() << "Error: Can't create nzb output file: " << QString("%1/%2.nzb").arg(nzbPath()).arg(_nzbName);
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
    connect(&_progressTimer, &QTimer::timeout, this, &NgPost::onRefreshProgressBar, Qt::DirectConnection);
    _progressTimer.start(_refreshRate);
#endif

    return true;
}

NntpArticle *NgPost::getNextArticle()
{
#ifdef __USE_MUTEX__
    QMutexLocker lock(&_mutex);
#endif
    NntpArticle *article = nullptr;
    if (_articles.size())
        article = _articles.dequeue();
    else
        article = _prepareNextArticle();

    if (article)
        emit scheduleNextArticle();

    return article;
}

void NgPost::onNntpFilePosted(NntpFile *nntpFile)
{
    _uploadSize += nntpFile->fileSize();
    ++_nbPosted;
    nntpFile->writeToNZB(_nzbStream);
    _filesInProgress.remove(nntpFile);
    delete nntpFile;
    if (_nbPosted == _nbFiles)
    {
        _log("All files have been posted => closing application");
        qApp->quit();
    }
}

void NgPost::onLog(QString msg)
{
    qDebug() << msg;
}

void NgPost::onDisconnectedConnection(NntpConnection *con)
{
    QTextStream cerr(stderr);
    cerr << tr("Error: disconnected connection: #%1\n").arg(con->getId());
    _nntpConnections.removeOne(con);
    delete con;
    if (_nntpConnections.isEmpty())
    {

        cerr << "we lost all the connections... => closing application\n";
        qApp->quit();
    }
    cerr << flush;
}

void NgPost::onPrepareNextArticle()
{
    _prepareNextArticle();
}

void NgPost::onErrorConnecting(QString err)
{
    QTextStream cerr(stderr);
    cerr << err << "\n" << flush;
}

#ifdef __DISP_PROGRESS_BAR__
void NgPost::onArticlePosted(NntpArticle *article)
{
    Q_UNUSED(article)
    ++_nbArticlesUploaded;
}

#include <iostream>
void NgPost:: onRefreshProgressBar()
{
    int barWidth = 70;
    float progress = (float)_nbArticlesUploaded / _nbArticlesTotal;

    qDebug() << "[NgPost::onRefreshProgressBar] uploaded: " << _nbArticlesUploaded
             << " / " << _nbArticlesTotal
             << " => progress: " << progress << "\n";

    std::cout << "\r[";
    int pos = barWidth * progress;
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << int(progress * 100.0) << " %"
              << " (" << _nbArticlesUploaded << " / " << _nbArticlesTotal << ")";
    std::cout.flush();
    if (_nbArticlesUploaded < _nbArticlesTotal)
        _progressTimer.start(_refreshRate);
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
    }
}

void NgPost::_printStats() const
{
    QString unitSize = "B";
    double uploadSize = _uploadSize;
    if (uploadSize > 1024)
    {
        uploadSize /= 1024;
        unitSize = "kB";
    }
    if (uploadSize > 1024)
    {
        uploadSize /= 1024;
        unitSize = "MB";
    }

    int duration = _timeStart.elapsed();
    double sec = duration/1000;
    double bandwidth = _uploadSize / sec;
    QString unitSpeed = "B/s";

    if (bandwidth > 1024)
    {
        bandwidth /= 1024;
        unitSpeed = "kB/s";
    }
    if (bandwidth > 1024)
    {
        bandwidth /= 1024;
        unitSpeed = "MB/s";
    }

    QString msgEnd = tr("\nUpload size: %1 %2 in %3 (%4 sec) => average speed: %5 %6 (%7 connections on %8 threads)\n").arg(
                uploadSize).arg(unitSize).arg(
                QTime::fromMSecsSinceStartOfDay(duration).toString("hh:mm:ss.zzz")).arg(sec).arg(
                bandwidth).arg(unitSpeed).arg(_nntpConnections.size()).arg(_threadPool.size());


    QTextStream cout(stdout);
    cout << msgEnd;
    if (_nzb)
        cout << QString("nzb file: %1/%2.nzb\n").arg(nzbPath()).arg(_nzbName);
    cout << "\n" << flush;
}

void NgPost::_log(const QString &aMsg) const
{
    qDebug() << QString("[%1] %2").arg(QThread::currentThread()->objectName()).arg(aMsg);

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

NntpArticle *NgPost::_getNextArticle()
{
    if (!_nntpFile)
    {
        _nntpFile = _getNextFile();
        if (!_nntpFile)
        {
#ifdef __DEBUG__
            _log("No more file to post...");
#endif
            return nullptr;
        }
    }

    if (!_file)
    {
        _file = new QFile(_nntpFile->path());
        if (_file->open(QIODevice::ReadOnly))
        {
            _log(tr("starting processing file %1").arg(_nntpFile->path()));
            _part = 0;
        }
        else
        {
            _log(tr("Error: couldn't open file %1").arg(_nntpFile->path()));
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
            _log(tr("we've read %1 bytes from %2 (=> new pos: %3)").arg(bytes).arg(pos).arg(_file->pos()));
#endif
            ++_part;
            NntpArticle *article = new NntpArticle(_nntpFile, _part, _buffer, pos, bytes,
                                                   _from.empty()?_randomFrom():_from,
                                                   _groups, _obfuscation);

#ifdef __DISP_PROGRESS_BAR__
            connect(article, &NntpArticle::posted, this, &NgPost::onArticlePosted, Qt::QueuedConnection);
#endif

#ifdef __SAVE_ARTICLES__
            article->dumpToFile("/tmp");
#endif
            return article;
        }
        else
        {
            _log(tr("finished processing file %1").arg(_nntpFile->path()));
            _file->close();
            delete _file;
            _file = nullptr;
            _nntpFile = nullptr;
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
        QTextStream cerr(stderr);
        cerr << "Error syntax: " << parser.errorText() << "\n" << flush;
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
        QTextStream cerr(stderr);
        cerr << "Error syntax: you should provide at least one input file or directory using the option -i\n" << flush;
        return false;
    }

    if (parser.isSet(sOptionNames[Opt::CONF]))
    {
        QString err = _parseConfig(parser.value(sOptionNames[Opt::CONF]));
        if (!err.isEmpty())
        {
            QTextStream cerr(stderr);
            cerr << err << "\n" << flush;
            return false;
        }
    }
    else
    {
        QString conf = QString("%1/%2").arg(getenv("HOME")).arg(sDefaultConfig);
        QFileInfo defaultConf(conf);
        if (defaultConf.exists() && defaultConf.isFile())
        {
            qCritical() << "Using default config file: " << conf;
            QString err = _parseConfig(conf);
            if (!err.isEmpty())
            {
                QTextStream cerr(stderr);
                cerr << err << "\n" << flush;
                return false;
            }
        }
    }

    if (parser.isSet(sOptionNames[Opt::REAL_NAME]))
    {
        QTextStream cout(stdout);
        cout << "Real yEnc name for the subject of the Articles (no obfuscation...)\n" << flush;
        _obfuscation = false;
    }

    if (parser.isSet(sOptionNames[Opt::THREAD]))
    {
        bool ok;
        _nbThreads = parser.value(sOptionNames[Opt::THREAD]).toInt(&ok);
        if (_nbThreads < 1)
            _nbThreads = 1;
        if (!ok)
        {
            QTextStream cerr(stderr);
            cerr << tr("You should give an integer for the number of threads (option -t)\n") << flush;
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

    if (parser.isSet(sOptionNames[Opt::MSG_ID]))
        _articleIdSignature = escapeXML(sOptionNames[Opt::MSG_ID]).toStdString();

    if (parser.isSet(sOptionNames[Opt::ARTICLE_SIZE]))
    {
        bool ok;
        int size = parser.value(sOptionNames[Opt::ARTICLE_SIZE]).toInt(&ok);
        if (ok)
            sArticleSize = size;
        else
        {
            QTextStream cerr(stderr);
            cerr << tr("You should give an integer for the article size (option -a)\n") << flush;
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
            server->useSSL = true;

        if (parser.isSet(sOptionNames[Opt::PORT]))
        {
            bool ok;
            ushort port = parser.value(sOptionNames[Opt::PORT]).toUShort(&ok);
            if (ok)
                server->port = port;
            else
            {
                QTextStream cerr(stderr);
                cerr << tr("You should give an integer for the port (option -P)\n") << flush;
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
                QTextStream cerr(stderr);
                cerr << tr("You should give an integer for the number of connections (option -n)\n") << flush;
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
            QTextStream cerr(stderr);
            cerr << tr("Error: the input file '%1' is not readable...\n").arg(parser.value("input")) << flush;
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
                        QTextStream cerr(stderr);
                        cerr << tr("Error: the input file '%1' is not readable...\n").arg(subFile.absoluteFilePath()) << flush;
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

    // initialize buffer and nzb file
    _buffer  = new char[articleSize()+1];
    _nzb     = new QFile(QString("%1/%2.nzb").arg(nzbPath()).arg(_nzbName));
    _nbFiles = filesToUpload.size();

    // initialize the NntpFiles (active objects)
    _filesToUpload.reserve(filesToUpload.size());
    int fileNum = 0;
    for (const QFileInfo &file : filesToUpload)
    {
        NntpFile *nntpFile = new NntpFile(file, ++fileNum, _nbFiles, _grpList);
        connect(nntpFile, &NntpFile::allArticlesArePosted, this, &NgPost::onNntpFilePosted);
        _filesToUpload.enqueue(nntpFile);
#ifdef __DISP_PROGRESS_BAR__
        _nbArticlesTotal += nntpFile->nbArticles();
#endif
    }

#ifdef __DEBUG__
    _dumpParams();
#endif

    return false;
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
                    else if (opt == sOptionNames[Opt::REAL_NAME])
                    {
                        _obfuscation = false;
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
                    else if (opt == sOptionNames[Opt::FROM])
                    {
                        if (!val.contains('@'))
                            val += "@ngPost";
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
                            serverParams->useSSL = true;
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
    QTextStream cout(stdout);
    cout << appVersion << " is a NNTP poster that can:\n"
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
            cout << "\n// without config file, you can provide all the parameters to connect to ONE SINGLE server\n";
        cout << QString("\t-%1: %2\n").arg(opt.names().join(" or -"), -18).arg(opt.description());
    }

    cout << "\nExamples:\n"
         << "  - with config file: " << app << " -c ~/.ngPost -m \"password=qwerty42\" -f ngPost@nowhere.com -i /tmp/file1 -i /tmp/file2 -i /tmp/folderToPost1 -i /tmp/folderToPost2\n"
         << "  - with all params:  " << app << " -t 1 -m \"password=qwerty42\" -m \"metaKey=someValue\" -h news.newshosting.com -P 443 -s -u user -p pass -n 30 -f ngPost@nowhere.com \
            -g \"alt.binaries.test,alt.binaries.test2\" -a 64000 -i /tmp/folderToPost -o /tmp/folderToPost.nzb\n"
         << "\nIf you don't provide the output file (nzb file), we will create it in the nzbPath with the name of the last file or folder given in the command line.\n"
         << "so in the first example above, the nzb would be: /tmp/folderToPost2.nzb\n"
         << flush;
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
             << "\n[NgPost::_dumpParams]<<<<<<<<<<<<<<<<<<\n";
}
enum class Opt {HELP = 0, VERSION, CONF,
                INPUT, OUTPUT, THREAD, MSG_ID,
                META, ARTICLE_SIZE, FROM, GROUPS,
                HOST, PORT, SSL, USER, PASS, CONNECTION
               };
#endif
