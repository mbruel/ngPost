//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
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

#include "PostingJob.h"
#include "NgPost.h"
#include "NntpConnection.h"
#include "nntp/NntpServerParams.h"
#include "nntp/NntpFile.h"
#include "nntp/NntpArticle.h"
#include "hmi/PostingWidget.h"
#include <cmath>
#include <QDebug>
#include <QProcess>
#include <QThread>
#include <QMutex>
#include <QCoreApplication>
#include <QDir>

PostingJob::PostingJob(NgPost *ngPost,
                       const QString &nzbFilePath,
                       const QFileInfoList &files,
                       PostingWidget *postWidget,
                       bool obfuscateArticles,
                       const QString &tmpPath,
                       const QString &rarPath,
                       uint rarSize,
                       uint par2Pct,
                       bool doCompress,
                       bool doPar2,
                       const QString &rarName,
                       const QString &rarPass,
                       QObject *parent) :
    QObject (parent),
    _ngPost(ngPost), _files(files), _postWidget(postWidget),

    _extProc(nullptr), _compressDir(nullptr), _limitProcDisplay(false), _nbProcDisp(42),

    _tmpPath(tmpPath), _rarPath(rarPath), _rarSize(rarSize), _par2Pct(par2Pct),
    _doCompress(doCompress), _doPar2(doPar2), _rarName(rarName), _rarPass(rarPass),

    _threadPool(), _nntpConnections(),

    _nzbName(QFileInfo(nzbFilePath).fileName()),
    _filesToUpload(), _filesInProgress(),
    _nbFiles(), _nbPosted(),


    _nzbFilePath(nzbFilePath), _nzb(nullptr), _nzbStream(),
    _nntpFile(nullptr), _file(nullptr), _buffer(nullptr), _part(0),
    _secureArticles(), _articles(),
    _timeStart(), _totalSize(0),

    _nbConnections(0), _nbThreads(QThread::idealThreadCount()),
    _nbArticlesUploaded(0), _nbArticlesFailed(0),
    _uploadedSize(0), _nbArticlesTotal(0),
    _stopPosting(0x0), _noMoreFiles(0x0),
    _postSucceed(false),
    _obfuscateArticles(obfuscateArticles)
{
#ifdef __DEBUG__
    qDebug() << "[PostingJob] >>>> Construct " << this;
#endif
    connect(this, &PostingJob::startPosting,     this,    &PostingJob::onStartPosting,   Qt::QueuedConnection);
    connect(this, &PostingJob::stopPosting,      this,    &PostingJob::onStopPosting,    Qt::QueuedConnection);
    connect(this, &PostingJob::postingStarted,   _ngPost, &NgPost::onPostingJobStarted,  Qt::QueuedConnection);
    connect(this, &PostingJob::postingFinished,  _ngPost, &NgPost::onPostingJobFinished, Qt::QueuedConnection);
    connect(this, &PostingJob::noMoreConnection, _ngPost, &NgPost::onPostingJobFinished, Qt::QueuedConnection);

    connect(this, &PostingJob::scheduleNextArticle, this, &PostingJob::onPrepareNextArticle, Qt::QueuedConnection);

    if (_postWidget)
    {
        connect(this, &PostingJob::filePosted,       _postWidget, &PostingWidget::onFilePosted,        Qt::QueuedConnection);
        connect(this, &PostingJob::archiveFileNames, _postWidget, &PostingWidget::onArchiveFileNames,  Qt::QueuedConnection);
        connect(this, &PostingJob::articlesNumber,   _postWidget, &PostingWidget::onArticlesNumber,    Qt::QueuedConnection);
        connect(this, &PostingJob::postingFinished,  _postWidget, &PostingWidget::onPostingJobDone,    Qt::QueuedConnection);
    }
}

PostingJob::~PostingJob()
{
#ifdef __DEBUG__
    qDebug() << "[PostingJob] <<<< Destruction " << this;
#endif

    if (_compressDir)
        _cleanCompressDir();

    if (_extProc)
        _cleanExtProc();

    qDeleteAll(_filesInProgress);
    qDeleteAll(_filesToUpload);
    qDeleteAll(_nntpConnections);
    qDeleteAll(_threadPool);

    if (_buffer)
        delete _buffer;
    if (_nzb)
        delete _nzb;
    if (_file)
        delete _file;
}

NntpArticle *PostingJob::getNextArticle(const QString &threadName)
{
    if (_stopPosting.load())
        return nullptr;

    QMutexLocker lock(&_secureArticles); // thread safety (coming from a posting thread)
    if (_ngPost->debugMode())
        _log(tr("[%1][PostingJob::getNextArticle] _articles.size() = %2").arg(threadName).arg(_articles.size()));

    NntpArticle *article = nullptr;
    if (_articles.size())
        article = _articles.dequeue();    
    else
    {
        if (!_noMoreFiles.load())
        {
            // we should never come here as the goal is to have articles prepared in advance in the queue
            if (_ngPost->debugMode())
                _log(tr("[%1][PostingJob::getNextArticle] no article prepared...").arg(threadName));

            article = _prepareNextArticle(threadName, false);
        }
    }

    if (article)
        emit scheduleNextArticle(); // schedule the preparation of another Article in main thread

    return article;
}


void PostingJob::onStartPosting()
{
    if (_postWidget)
        _log(QString("<h3>Start Post #%1: %2</h3>").arg(_postWidget->jobNumber()).arg(_nzbName));
    else
        _log(QString("\n\nStart posting: %1").arg(_nzbName));

    if (_doCompress)
    {
#ifdef __DEBUG__
        _log("[PostingJob::onStartPosting] Starting compression...");
#endif
        if (!startCompressFiles(_rarPath, _tmpPath, _rarName, _rarPass, _rarSize))
            emit postingFinished();
    }
    else
        _postFiles();

}


void PostingJob::_postFiles()
{
    if (_postWidget) // in case we were in Pending mode
        _postWidget->setPosting();

    if (_doCompress)
    {
        QStringList archiveNames;
        _files.clear();
        for (const QFileInfo & file : _compressDir->entryInfoList(QDir::Files, QDir::Name))
        {
            _files << file;
            archiveNames << file.absoluteFilePath();
            if (_ngPost->debugMode())
                _ngPost->_log(QString("  - %1").arg(file.fileName()));
        }
        emit archiveFileNames(archiveNames);
    }

    _initPosting();

    if (_nbThreads < 1)
        _nbThreads = 1;
    else if (_nbThreads > QThread::idealThreadCount())
        _nbThreads = QThread::idealThreadCount();


    if (!_nzb->open(QIODevice::WriteOnly))
    {
        _error(tr("Error: Can't create nzb output file: %1").arg(_nzbFilePath));
        emit postingFinished();
        return ;
    }
    else
    {
        QString tab = _ngPost->space();
        _nzbStream.setDevice(_nzb);
        _nzbStream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   << "<!DOCTYPE nzb PUBLIC \"-//newzBin//DTD NZB 1.1//EN\" \"http://www.newzbin.com/DTD/nzb/nzb-1.1.dtd\">\n"
                   << "<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n";

        if (_ngPost->_meta.size())
        {
            _nzbStream << tab << "<head>\n";
            for (auto itMeta = _ngPost->_meta.cbegin(); itMeta != _ngPost->_meta.cend() ; ++itMeta)
                _nzbStream << tab << tab << "<meta type=\"" << itMeta.key() << "\">" << itMeta.value() << "</meta>\n";
            _nzbStream << tab << "</head>\n\n";
        }
        _nzbStream << flush;
    }

    int  nbTh = _nbThreads, nbCon = _createNntpConnections();
    if (!nbCon)
    {
        _error("Error: there are no NntpConnection...");
        emit postingFinished();
        return;
    }

    _timeStart.start();

    QMutexLocker lock(&_secureArticles); // start the connections but they must wait _prepareArticles

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

    emit postingStarted();
}


void PostingJob::onStopPosting()
{
    if (_extProc)
    {
        _log("killing external process...");
        _extProc->terminate();
    }
    else
    {
        _finishPosting();
        emit postingFinished();
    }
}


void PostingJob::onDisconnectedConnection(NntpConnection *con)
{
    if (_stopPosting.load())
        return; // we're destructing all the connections

    _error(tr("Error: disconnected connection: #%1\n").arg(con->getId()));
    _nntpConnections.removeOne(con);
    delete con;
    if (_nntpConnections.isEmpty())
    {
        _error(tr("we lost all the connections..."));
        _finishPosting();
        emit noMoreConnection();
    }
}

void PostingJob::onPrepareNextArticle()
{
    QMutexLocker lock(&_secureArticles); // thread safety (coming from PostingJob)
    _prepareNextArticle(_ngPost->sMainThreadName);
}

void PostingJob::onNntpFileStartPosting()
{
    NntpFile *nntpFile = static_cast<NntpFile*>(sender());
    _log(QString("[avg. speed: %1] >>>>> %2").arg(avgSpeed()).arg(nntpFile->name()));
}

void PostingJob::onNntpFilePosted()
{
    NntpFile *nntpFile = static_cast<NntpFile*>(sender());
    _totalSize += static_cast<quint64>(nntpFile->fileSize());
    ++_nbPosted;
    if (_postWidget)
        emit filePosted(nntpFile->path(), nntpFile->nbArticles(), nntpFile->nbFailedArticles());

    if (_ngPost->_dispFilesPosting)
        _log(QString("[avg. speed: %1] <<<<< %2").arg(avgSpeed()).arg(nntpFile->name()));

    nntpFile->writeToNZB(_nzbStream, _ngPost->_articleIdSignature.c_str());
    _filesInProgress.remove(nntpFile);
    emit nntpFile->scheduleDeletion();
    if (_nbPosted == _nbFiles)
    {
#ifdef __DEBUG__
        _log(QString("All files have been posted => closing Job (nb article uploaded: %1, failed: %2)").arg(
                 _nbArticlesUploaded).arg(_nbArticlesFailed));
#endif

        _postSucceed = true;
        _finishPosting();

        emit postingFinished();
    }
}

void PostingJob::_log(const QString &aMsg, bool newline) const
{
    emit _ngPost->log(aMsg, newline);
}

void PostingJob::_error(const QString &error) const
{
    emit _ngPost->error(error);
}

int PostingJob::_createNntpConnections()
{
    _nbConnections = 0;
    for (NntpServerParams *srvParams : _ngPost->_nntpServers)
        _nbConnections += srvParams->nbCons;

    _nntpConnections.reserve(_nbConnections);
    int conIdx = 0;
    for (NntpServerParams *srvParams : _ngPost->_nntpServers)
    {
        for (int k = 0 ; k < srvParams->nbCons ; ++k)
        {
            NntpConnection *nntpCon = new NntpConnection(_ngPost, ++conIdx, *srvParams);
            connect(nntpCon, &NntpConnection::log,   _ngPost, &NgPost::onLog, Qt::QueuedConnection);
            connect(nntpCon, &NntpConnection::error, _ngPost, &NgPost::onError, Qt::QueuedConnection);
            connect(nntpCon, &NntpConnection::errorConnecting, _ngPost, &NgPost::onErrorConnecting, Qt::QueuedConnection);
            connect(nntpCon, &NntpConnection::disconnected, this, &PostingJob::onDisconnectedConnection, Qt::QueuedConnection);
            _nntpConnections.append(nntpCon);
        }
    }

    _log(tr("Number of available Nntp Connections: %1").arg(_nbConnections));
    return _nbConnections;
}

void PostingJob::_startConnectionInThread(int conIdx, QThread *thread, const QString &threadName)
{
    NntpConnection *con = _nntpConnections.at(conIdx);
    con->setThreadName(threadName);
    con->moveToThread(thread);
    emit con->startConnection();
}

void PostingJob::_prepareArticles()
{

    int nbArticlesToPrepare = _ngPost->sNbPreparedArticlePerConnection * _nntpConnections.size();
  #ifdef __DEBUG__
    _log(QString("[PostingJob::_prepareArticles] >>>> preparing %1 articles for each connections. Nb cons = %2 => should prepare %3 articles!").arg(
             _ngPost->sNbPreparedArticlePerConnection).arg(_nntpConnections.size()).arg(nbArticlesToPrepare));
  #endif
    for (int i = 0; i < nbArticlesToPrepare ; ++i)
    {
        if (!_prepareNextArticle(_ngPost->sMainThreadName))
        {
  #ifdef __DEBUG__
            _log(QString("[PostingJob::_prepareArticles] No more Articles to produce after i = %1").arg(i));
  #endif
            break;
        }
    }
  #ifdef __DEBUG__
    _log(QString("[NgPost::_prepareArticles] <<<<< finish preparing, current article queue size:  %1").arg(_articles.size()));
  #endif
}


NntpArticle *PostingJob::_getNextArticle(const QString &threadName)
{
    if (!_nntpFile)
    {
        _nntpFile = _getNextFile();
        if (!_nntpFile)
        {
            _noMoreFiles = 0x1;
#ifdef __DEBUG__
            _log(tr("[%1] No more file to post...").arg(threadName));
#endif
            return nullptr;
        }
    }

    if (!_file)
    {
        _file = new QFile(_nntpFile->path());
        if (_file->open(QIODevice::ReadOnly))
        {
            if (_ngPost->debugMode())
                _log(tr("[%1] starting processing file %2").arg(threadName).arg(_nntpFile->path()));
            _part = 0;
        }
        else
        {
            if (_ngPost->debugMode())
                _error(tr("[%1] Error: couldn't open file %2").arg(threadName).arg(_nntpFile->path()));
            else
                _error(tr("Error: couldn't open file %1").arg(_nntpFile->path()));
            delete _file;
            _file = nullptr;
            _nntpFile = nullptr;
            return _getNextArticle(threadName); // Check if we have more files
        }
    }


    if (_file)
    {
        qint64 pos   = _file->pos();
        qint64 bytes = _file->read(_buffer, _ngPost->articleSize());
        if (bytes > 0)
        {
            _buffer[bytes] = '\0';
            if (_ngPost->debugMode())
                _log(tr("[%1] we've read %2 bytes from %3 (=> new pos: %4)").arg(threadName).arg(bytes).arg(pos).arg(_file->pos()));
            ++_part;
            NntpArticle *article = new NntpArticle(_nntpFile, _part, _buffer, pos, bytes,
                                                   _obfuscateArticles ? _ngPost->_randomFrom() : _ngPost->_from,
                                                   _ngPost->_groups, _obfuscateArticles);

#ifdef __SAVE_ARTICLES__
            article->dumpToFile("/tmp", _articleIdSignature);
#endif
            return article;
        }
        else
        {
            if (_ngPost->debugMode())
                _log(tr("[%1] finished processing file %2").arg(threadName).arg(_nntpFile->path()));

            _file->close();
            delete _file;
            _file = nullptr;
            _nntpFile = nullptr;
        }
    }

    return _getNextArticle(threadName); // if we didn't have an Article, check next file
}

NntpArticle *PostingJob::_prepareNextArticle(const QString &threadName, bool fillQueue)
{
    NntpArticle *article = _getNextArticle(threadName);
    if (article && fillQueue)
        _articles.enqueue(article);   
    return article;
}

void PostingJob::_initPosting()
{
    // initialize buffer and nzb file
    _buffer  = new char[static_cast<quint64>(_ngPost->articleSize())+1];
    _nzb     = new QFile(_nzbFilePath);
    _nbFiles = static_cast<uint>(_files.size());

    // initialize the NntpFiles (active objects)
    _filesToUpload.reserve(static_cast<int>(_nbFiles));
    uint fileNum = 0;
    for (const QFileInfo &file : _files)
    {
        NntpFile *nntpFile = new NntpFile(this, file, ++fileNum, _nbFiles, _ngPost->_grpList);
        connect(nntpFile, &NntpFile::allArticlesArePosted, this, &PostingJob::onNntpFilePosted, Qt::QueuedConnection);
        if (_ngPost->_dispFilesPosting && _ngPost->debugMode())
            connect(nntpFile, &NntpFile::startPosting, this, &PostingJob::onNntpFileStartPosting, Qt::QueuedConnection);

        _filesToUpload.enqueue(nntpFile);
        _nbArticlesTotal += nntpFile->nbArticles();
    }
    emit articlesNumber(_nbArticlesTotal);
}

void PostingJob::_finishPosting()
{
    _stopPosting = 0x1;

    if (!_timeStart.isNull())
    {
        _nbArticlesUploaded = _nbArticlesTotal; // we might not have processed the last onArticlePosted
        _uploadedSize       = _totalSize;
    }

#ifdef __DEBUG__
    _log("Finishing posting...");
#endif


    // 1.: print stats
    if (!_timeStart.isNull())
        _printStats();


    for (NntpConnection *con : _nntpConnections)
        emit con->killConnection();

    qApp->processEvents();


    // 4.: stop and wait for all threads
    // 2.: close nzb file
    _closeNzb();


    // 3.: close all the connections (they're living in the _threadPool)
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
}

void PostingJob::_closeNzb()
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

void PostingJob::_printStats() const
{
    QString size = postSize();

    int duration = _timeStart.elapsed();
    double sec = duration/1000;

    QString msgEnd = tr("\nUpload size: %1 in %2 (%3 sec) \
=> average speed: %4 (%5 connections on %6 threads)\n").arg(size).arg(
                QTime::fromMSecsSinceStartOfDay(duration).toString("hh:mm:ss.zzz")).arg(sec).arg(
                avgSpeed()).arg(_nntpConnections.size()).arg(_threadPool.size());

    if (_nbArticlesFailed > 0)
        msgEnd += tr("%1 / %2 articles FAILED to be uploaded (even with %3 retries)...\n").arg(
                      _nbArticlesFailed).arg(_nbArticlesTotal).arg(NntpArticle::nbMaxTrySending());

    if (_nzb)
    {
        msgEnd += QString("nzb file: %1\n").arg(_nzbFilePath);
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




//int PostingJob::compressFiles()
//{
//    if (!_canCompress() || (_doPar2 && !_canGenPar2()))
//        return -1;

//    // 1.: Compress
//    int exitCode = _compressFiles(_rarPath, _tmpPath, _rarName, _rarPass, _rarSize);


//    if (exitCode == 0 && _doPar2)
//        exitCode = _genPar2(_tmpPath, _rarName, _par2Pct);

//    if (exitCode == 0)
//        _log("Ready to post!");

//    return exitCode;
//}

bool PostingJob::startCompressFiles(const QString &cmdRar,
                                   const QString &tmpFolder,
                                   const QString &archiveName,
                                   const QString &pass,
                                   uint volSize)
{
    if (!_canCompress())
        return false;

    // 1.: create archive temporary folder
    QString archiveTmpFolder = _createArchiveFolder(tmpFolder, archiveName);
    if (archiveTmpFolder.isEmpty())
        return false;

    _extProc = new QProcess(this);
    connect(_extProc, &QProcess::readyReadStandardOutput, this, &PostingJob::onExtProcReadyReadStandardOutput, Qt::DirectConnection);
    connect(_extProc, &QProcess::readyReadStandardError,  this, &PostingJob::onExtProcReadyReadStandardError,  Qt::DirectConnection);
    connect(_extProc, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &PostingJob::onCompressionFinished);

    bool is7z = false;
    if (_rarPath.contains("7z"))
    {
        is7z = true;
        if (_rarArgs.isEmpty())
            _rarArgs = _ngPost->sDefault7zOptions;
    }
    else
    {
        if (_rarArgs.isEmpty())
            _rarArgs = _ngPost->sDefaultRarExtraOptions;
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

    for (const QFileInfo &fileInfo : _files)
        args << fileInfo.absoluteFilePath();

    // 3.: launch rar
    if (_ngPost->debugMode() || !_postWidget)
        _log(tr("Compressing files: %1 %2\n").arg(cmdRar).arg(args.join(" ")));
    else
        _log("Compressing files...\n");
    _limitProcDisplay = false;
    _extProc->start(cmdRar, args);

    _log("[PostingJob::_compressFiles] compression started...");

    return true;
}


void PostingJob::onCompressionFinished(int exitCode)
{
    if (_ngPost->debugMode())
        _log(tr("=> rar exit code: %1\n").arg(exitCode));
    else
        _log("");



    _log("[PostingJob::_compressFiles] compression finished...");

    if (exitCode != 0)
    {
        _error(tr("Error during compression: %1").arg(exitCode));
        _cleanCompressDir();
        emit postingFinished();
    }
    else
    {
        if (_doPar2)
        {
            disconnect(_extProc, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                       this, &PostingJob::onCompressionFinished);
            if (!startGenPar2(_tmpPath, _rarName, _par2Pct))
            {
                _cleanCompressDir();
                emit postingFinished();
            }
        }
        else
        {
            _cleanExtProc();
            _postFiles();
        }
    }
}

bool PostingJob::startGenPar2(const QString &tmpFolder,
                     const QString &archiveName,
                     uint redundancy)
{
    if (!_canGenPar2())
        return false;

    QString archiveTmpFolder;
    QStringList args;
    if (_ngPost->_par2Args.isEmpty())
        args << "c" << "-l" << "-m1024" << QString("-r%1").arg(redundancy);
    else
        args << _ngPost->_par2Args.split(" ");

    // we've already compressed => we gen par2 for the files in the archive folder
    if (_extProc)
    {
        archiveTmpFolder = QString("%1/%2").arg(tmpFolder).arg(archiveName);
        if (_ngPost->_par2Path.toLower().contains("parpar"))
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
            if (_ngPost->_par2Args.isEmpty() && (_ngPost->debugMode() || !_postWidget))
                args << "-q"; // remove the progressbar bar

        }
    }
    else
    {
        // par2 on Linux wants to have all files in the same folder and generate the par2 there
        // otherwise we get this error: Ignoring out of basepath source file
        // exitcode: 3
        _error("ngPost can't generate par2 without compression cause we'd have to copy all the files in a temporary folder...");
        return false;

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

    connect(_extProc, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &PostingJob::onGenPar2Finished);

    if (_ngPost->debugMode() || !_postWidget)
        _log(tr("Generating par2: %1 %2\n").arg(_ngPost->_par2Path).arg(args.join(" ")));
    else
        _log("Generating par2...\n");
    _limitProcDisplay = true;
    _nbProcDisp = 0;
    _extProc->start(_ngPost->_par2Path, args);    

    return true;
}

void PostingJob::onGenPar2Finished(int exitCode)
{
    if (_ngPost->debugMode())
        _log(tr("=> par2 exit code: %1\n").arg(exitCode));
    else
        _log("");

    _cleanExtProc();

    if (exitCode != 0)
    {
        _error(tr("Error during par2 generation: %1").arg(exitCode));
        _cleanCompressDir();
        emit postingFinished();
    }
    else
        _postFiles();
}


void PostingJob::_cleanExtProc()
{
    delete _extProc;
    _extProc = nullptr;
    if (_ngPost->debugMode())
        _log("External process deleted.");
}

void PostingJob::_cleanCompressDir()
{
    _compressDir->removeRecursively();
    delete _compressDir;
    _compressDir = nullptr;
    if (_ngPost->debugMode())
        _log("Compressed files deleted.");
}

QString PostingJob::_createArchiveFolder(const QString &tmpFolder, const QString &archiveName)
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


void PostingJob::onExtProcReadyReadStandardOutput()
{
    if (_ngPost->debugMode())
        _log(_extProc->readAllStandardOutput(), false);
    else
    {
        if (!_limitProcDisplay || ++_nbProcDisp%42 == 0)
            _log("*", false);
    }
//    qApp->processEvents();
}

void PostingJob::onExtProcReadyReadStandardError()
{
    _error(_extProc->readAllStandardError());
//    qApp->processEvents();
}


bool PostingJob::_checkTmpFolder() const
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


bool PostingJob::_canCompress() const
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

bool PostingJob::_canGenPar2() const
{
    //1.: the _tmp_folder must be writable
    if (!_checkTmpFolder())
        return false;

    //2.: check _ is executable
    QFileInfo fi(_ngPost->_par2Path);
    if (!fi.exists() || !fi.isFile() || !fi.isExecutable())
    {
        _error(tr("ERROR: par2 is not available..."));
        return false;
    }

    return true;
}
