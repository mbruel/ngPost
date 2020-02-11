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
                       const QList<QString> &grpList,
                       const std::string    &groups,
                       const std::string    &from,
                       bool obfuscateArticles,
                       bool obfuscateFileName,
                       const QString &tmpPath,
                       const QString &rarPath,
                       uint rarSize,
                       bool useRarMax,
                       uint par2Pct,
                       bool doCompress,
                       bool doPar2,
                       const QString &rarName,
                       const QString &rarPass,
                       bool keepRar,
                       bool delFilesAfterPost,
                       bool overwriteNzb,
                       QObject *parent) :
    QObject (parent),
    _ngPost(ngPost), _files(files), _postWidget(postWidget),

    _extProc(nullptr), _compressDir(nullptr), _limitProcDisplay(false), _nbProcDisp(42),

    _tmpPath(tmpPath), _rarPath(rarPath), _rarSize(rarSize), _useRarMax(useRarMax), _par2Pct(par2Pct),
    _doCompress(doCompress), _doPar2(doPar2), _rarName(rarName), _rarPass(rarPass), _keepRar(keepRar),

    _nntpConnections(),

    _nzbName(QFileInfo(nzbFilePath).fileName()),
    _filesToUpload(), _filesInProgress(), _filesFailed(),
    _nbFiles(), _nbPosted(),


    _nzbFilePath(nzbFilePath), _nzb(nullptr), _nzbStream(),
    _nntpFile(nullptr), _file(nullptr), _part(0),
    _timeStart(), _totalSize(0),

    _nbConnections(0), _nbThreads(_ngPost->_nbThreads),
    _nbArticlesUploaded(0), _nbArticlesFailed(0),
    _uploadedSize(0), _nbArticlesTotal(0),
    _stopPosting(0x0), _noMoreFiles(0x0),
    _postFinished(false),
    _obfuscateArticles(obfuscateArticles), _obfuscateFileName(obfuscateFileName),
    _delFilesAfterPost(delFilesAfterPost ? 0x1 : 0x0),
    _originalFiles(!postWidget || delFilesAfterPost  || obfuscateFileName ? files : QFileInfoList()),
    _secureDiskAccess(), _posters(),
    _overwriteNzb(overwriteNzb),
    _grpList(grpList), _groups(groups), _from(from)
{
#ifdef __DEBUG__
    qDebug() << "[PostingJob] >>>> Construct " << this;
#endif
    connect(this, &PostingJob::startPosting,     this,    &PostingJob::onStartPosting,   Qt::QueuedConnection);
    connect(this, &PostingJob::stopPosting,      this,    &PostingJob::onStopPosting,    Qt::QueuedConnection);
    connect(this, &PostingJob::postingStarted,   _ngPost, &NgPost::onPostingJobStarted,  Qt::QueuedConnection);
    connect(this, &PostingJob::postingFinished,  _ngPost, &NgPost::onPostingJobFinished, Qt::QueuedConnection);
    connect(this, &PostingJob::noMoreConnection, _ngPost, &NgPost::onPostingJobFinished, Qt::QueuedConnection);

//    connect(this, &PostingJob::scheduleNextArticle, this, &PostingJob::onPrepareNextArticle, Qt::QueuedConnection);

    if (_postWidget)
    {
        connect(this, &PostingJob::filePosted,       _postWidget, &PostingWidget::onFilePosted,        Qt::QueuedConnection);
        connect(this, &PostingJob::archiveFileNames, _postWidget, &PostingWidget::onArchiveFileNames,  Qt::QueuedConnection);
        connect(this, &PostingJob::articlesNumber,   _postWidget, &PostingWidget::onArticlesNumber,    Qt::QueuedConnection);
        connect(this, &PostingJob::postingFinished,  _postWidget, &PostingWidget::onPostingJobDone,    Qt::QueuedConnection);
        connect(this, &PostingJob::noMoreConnection, _postWidget, &PostingWidget::onPostingJobDone,    Qt::QueuedConnection);
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

    qDeleteAll(_filesFailed);
    qDeleteAll(_filesInProgress);
    qDeleteAll(_filesToUpload);
    qDeleteAll(_nntpConnections);
    qDeleteAll(_posters);

    if (_nzb)
        delete _nzb;
    if (_file)
        delete _file;
}


void PostingJob::onStartPosting()
{
    if (_postWidget)
        _log(tr("<h3>Start Post #%1: %2</h3>").arg(_postWidget->jobNumber()).arg(_nzbName));
    else
        _log(tr("\n\nStart posting: %1").arg(_nzbName));

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


#include "Poster.h"
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

    if (_nbThreads > QThread::idealThreadCount())
        _nbThreads = QThread::idealThreadCount();

    int  nbPosters = _nbThreads/2, nbCon = _createNntpConnections();
    if (nbPosters < 1)
        nbPosters = 1;
    if (!nbCon)
    {
        _error(tr("Error: there are no NntpConnection..."));
        emit postingFinished();
        return;
    }

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

        if (!_rarPass.isEmpty() || _ngPost->_meta.size())
        {
            _nzbStream << tab << "<head>\n";
            for (auto itMeta = _ngPost->_meta.cbegin(); itMeta != _ngPost->_meta.cend() ; ++itMeta)
                _nzbStream << tab << tab << "<meta type=\"" << itMeta.key() << "\">" << itMeta.value() << "</meta>\n";
            if (!_rarPass.isEmpty())
                _nzbStream << tab << tab << "<meta type=\"password\">" << _rarPass << "</meta>\n";
            _nzbStream << tab << "</head>\n\n";
        }
        _nzbStream << flush;
    }



    _timeStart.start();

//    QMutexLocker lock(&_secureArticles); // start the connections but they must wait _prepareArticles

    if (nbPosters > nbCon)
        nbPosters = nbCon; // we can't have more thread than available connections


    _posters.reserve(nbPosters);
    int nbConPerPoster = static_cast<int>(std::floor(nbCon / nbPosters));
    int nbExtraCon     = nbCon - nbConPerPoster * nbPosters;
#ifdef __DEBUG__
    qDebug() << "[PostingJob::_postFiles] nbFiles: " << _filesToUpload.size()
             << ", nbPosters: " << nbPosters
             << ", nbCons: " << nbCon
             << " => nbCon per Poster: " << nbConPerPoster
             << " (nbExtraCon: " << nbExtraCon << ")";
#endif


    int conIdx = 0;
    for (ushort posterIdx = 0; posterIdx < nbPosters ; ++posterIdx)
    {
        Poster *poster = new Poster(this, posterIdx);
        _posters.append(poster);
        poster->lockQueue(); // lock queue so the connection will wait before starting building Articles

        for (int i = 0 ; i < nbConPerPoster ; ++i)
            poster->addConnection(_nntpConnections.at(conIdx++));

        if (nbExtraCon-- > 0)
            poster->addConnection(_nntpConnections.at(conIdx++));

        poster->startThreads();
    }

    // Prepare 2 Articles for each connections
    _preparePostersArticles();

    for (Poster *poster : _posters)
        poster->unlockQueue();

    emit postingStarted();
}



void PostingJob::onStopPosting()
{
    if (_extProc)
    {
        _log(tr("killing external process..."));
        _extProc->terminate();
        _extProc->waitForFinished();
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

void PostingJob::onNntpFileStartPosting()
{
    NntpFile *nntpFile = static_cast<NntpFile*>(sender());
    _log(tr("[avg. speed: %1] >>>>> %2").arg(avgSpeed()).arg(nntpFile->name()));
}

void PostingJob::onNntpFilePosted()
{
    NntpFile *nntpFile = static_cast<NntpFile*>(sender());
    _totalSize += static_cast<quint64>(nntpFile->fileSize());
    ++_nbPosted;
    if (_postWidget)
        emit filePosted(nntpFile->path(), nntpFile->nbArticles(), nntpFile->nbFailedArticles());

    if (_ngPost->_dispFilesPosting)
        _log(tr("[avg. speed: %1] <<<<< %2").arg(avgSpeed()).arg(nntpFile->name()));

    nntpFile->writeToNZB(_nzbStream, _ngPost->_articleIdSignature.c_str());
    _filesInProgress.remove(nntpFile);
    emit nntpFile->scheduleDeletion();
    if (_nbPosted == _nbFiles)
    {
#ifdef __DEBUG__
        _log(QString("All files have been posted => closing Job (nb article uploaded: %1, failed: %2)").arg(
                 _nbArticlesUploaded).arg(_nbArticlesFailed));
#endif

        _postFinished = true;
        _finishPosting();

        emit postingFinished();
    }
}

void PostingJob::onNntpErrorReading()
{
    NntpFile *nntpFile = static_cast<NntpFile*>(sender());
    ++_nbPosted;
    if (_postWidget)
        emit filePosted(nntpFile->path(), nntpFile->nbArticles(), nntpFile->nbArticles());

    if (_ngPost->_dispFilesPosting)
        _log(tr("[avg. speed: %1] <<<<< %2").arg(avgSpeed()).arg(nntpFile->name()));

    _filesInProgress.remove(nntpFile);
    _filesFailed.insert(nntpFile);
    if (_nbPosted == _nbFiles)
    {
#ifdef __DEBUG__
        _log(QString("All files have been posted => closing Job (nb article uploaded: %1, failed: %2)").arg(
                 _nbArticlesUploaded).arg(_nbArticlesFailed));
#endif

        _postFinished = true;
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

void PostingJob::_preparePostersArticles()
{
    if (_ngPost->debugMode())
        _log("PostingJob::_prepareArticles");

    for (int i = 0 ; i < _ngPost->sNbPreparedArticlePerConnection ; ++i)
    {
        if (_noMoreFiles.load())
            break;
        for (Poster *poster : _posters)
        {
            if (!poster->prepareArticlesInAdvance())
                break;
        }
    }
}

void PostingJob::_delOriginalFiles()
{
    for (const QFileInfo &fi : _originalFiles)
    {
        QString path = fi.absoluteFilePath();
        _log(tr("Deleting posted %1: %2").arg(fi.isDir()?tr("folder"):tr("file")).arg(path));
        if (fi.isDir())
        {
            QDir dir(path);
            dir.removeRecursively();
        }
        else
            QFile::remove(path);
    }
}



NntpArticle *PostingJob::_readNextArticleIntoBufferPtr(const QString &threadName, char **bufferPtr)
{
//qDebug() << "[PostingJob::readNextArticleIntoBufferPtr] " << threadName;
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
            emit _nntpFile->errorReadingFile();
            delete _file;
            _file = nullptr;
            _nntpFile = nullptr;
            return _readNextArticleIntoBufferPtr(threadName, bufferPtr); // Check if we have more files
        }
    }


    if (_file)
    {
        qint64 pos       = _file->pos();
        qint64 bytesRead = _file->read(*bufferPtr, _ngPost->articleSize());
        if (bytesRead > 0)
        {
            (*bufferPtr)[bytesRead] = '\0';
            if (_ngPost->debugMode())
                _log(tr("[%1] we've read %2 bytes from %3 (=> new pos: %4)").arg(threadName).arg(bytesRead).arg(pos).arg(_file->pos()));
            ++_part;
            NntpArticle *article = new NntpArticle(_nntpFile, _part, pos, bytesRead,
                                                   _obfuscateArticles ? _ngPost->_randomFrom() : _from,
                                                   _groups, _obfuscateArticles);
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

    return _readNextArticleIntoBufferPtr(threadName, bufferPtr); // if we didn't have an Article, check next file
}


void PostingJob::_initPosting()
{
    // initialize buffer and nzb file
    if (!_overwriteNzb)
    {
        QFileInfo fi(_nzbFilePath);
        ushort nbDuplicates = 1;
        while(fi.exists()){
            _nzbFilePath = QString("%1/%2_%3.nzb").arg(fi.absolutePath()).arg(fi.completeBaseName()).arg(++nbDuplicates);
            fi = QFileInfo(_nzbFilePath);
        }
    }
    _nzb     = new QFile(_nzbFilePath);
    _nbFiles = static_cast<uint>(_files.size());

    // initialize the NntpFiles (active objects)
    _filesToUpload.reserve(static_cast<int>(_nbFiles));
    uint fileNum = 0;
    for (const QFileInfo &file : _files)
    {
        NntpFile *nntpFile = new NntpFile(this, file, ++fileNum, _nbFiles, _grpList);
        connect(nntpFile, &NntpFile::allArticlesArePosted, this, &PostingJob::onNntpFilePosted, Qt::QueuedConnection);
        connect(nntpFile, &NntpFile::errorReadingFile,     this, &PostingJob::onNntpErrorReading, Qt::QueuedConnection);
        if (_ngPost->_dispFilesPosting && _ngPost->debugMode())
            connect(nntpFile, &NntpFile::startPosting, this, &PostingJob::onNntpFileStartPosting, Qt::QueuedConnection);

        _filesToUpload.enqueue(nntpFile);
        _nbArticlesTotal += nntpFile->nbArticles();
    }
    emit articlesNumber(_nbArticlesTotal);
}

void PostingJob::_finishPosting()
{
#ifdef __DEBUG__
qDebug() << "[MB_TRACE][PostingJob::_finishPosting]";
#endif
    _stopPosting = 0x1;

    if (!_timeStart.isNull() && _postFinished)
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
    for (Poster *poster : _posters)
        poster->stopThreads();

#ifdef __DEBUG__
    _log("All connections are closed...");
#endif

    // 5.: print out the list of files that havn't been posted
    // (in case of disconnection)
    int nbPendingFiles = _filesToUpload.size() + _filesInProgress.size() + _filesFailed.size();
    if (nbPendingFiles)
    {
        _error(tr("ERROR: there were %1 on %2 that havn't been posted:").arg(
                   nbPendingFiles).arg(_nbFiles));
        for (NntpFile *file : _filesInProgress)
            _error(QString("  - %1").arg(file->path()));
        for (NntpFile *file : _filesToUpload)
            _error(QString("  - %1").arg(file->path()));
        for (NntpFile *file : _filesFailed)
            _error(QString("  - %1").arg(file->path()));

        _error(tr("you can try to repost only those and concatenate the nzb with the current one ;)"));
    }
    else if (_postFinished && _delFilesAfterPost.load())
        _delOriginalFiles();
}

void PostingJob::_closeNzb()
{
    if (_nzb)
    {
        if (_nzb->isOpen())
        {
            _nzbStream << "</nzb>\n";
            _nzb->close();

            _ngPost->uploadNzb(_nzbFilePath);
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
                avgSpeed()).arg(_nntpConnections.size()).arg(_posters.size()*2);

    if (_nbArticlesFailed > 0)
        msgEnd += tr("%1 / %2 articles FAILED to be uploaded (even with %3 retries)...\n").arg(
                      _nbArticlesFailed).arg(_nbArticlesTotal).arg(NntpArticle::nbMaxTrySending());

    if (_nzb)
    {
        msgEnd += tr("nzb file: %1\n").arg(_nzbFilePath);
        if (_doCompress)
        {
            msgEnd += tr("file: %1, rar name: %2").arg(_nzbName).arg(_rarName);
            if (!_rarPass.isEmpty())
                msgEnd += tr(", rar pass: %1").arg(_rarPass);
        }
        if (!_ngPost->_urlNzbUpload)
            msgEnd += "\n";
    }

    _log(msgEnd);
}



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
    if (volSize > 0 || _useRarMax)
    {
        if (_useRarMax)
        {
            qint64 postSize = 0;
            for (const QFileInfo &fileInfo : _files)
            {
                if (fileInfo.isDir())
                    postSize += _dirSize(fileInfo.absoluteFilePath());
                else
                    postSize += fileInfo.size();
            }
            postSize /= 1024*1024; // to get it in MB
            if (volSize > 0)
            {
                if (postSize / volSize > _ngPost->_rarMax)
                    volSize = static_cast<uint>(postSize / _ngPost->_rarMax) + 1;
            }
            else
                volSize = static_cast<uint>(postSize / _ngPost->_rarMax) + 1;

#ifdef __DEBUG__
            qDebug() << tr("postSize: %1 MB => volSize: %2").arg(postSize).arg(volSize);
#endif
            if (_ngPost->debugMode())
                _log(tr("postSize: %1 MB => volSize: %2").arg(postSize).arg(volSize));
        }
        args << QString("-v%1m").arg(volSize);
    }

#if defined( Q_OS_WIN )
    if (archiveTmpFolder.startsWith("//"))
        archiveTmpFolder.replace(QRegExp("^//"), "\\\\");
#endif
    args << QString("%1/%2.%3").arg(archiveTmpFolder).arg(archiveName).arg(is7z ? "7z" : "rar");


    if (_obfuscateFileName)
    {
        _files.clear();
        for (const QFileInfo &fileInfo : _originalFiles)
        {
            QString obfuscatedName = QString("%1/%2").arg(fileInfo.absolutePath()).arg(
                        _ngPost->randomPass(_ngPost->_lengthName)),
                    fileName = fileInfo.absoluteFilePath();

            if (QFile::rename(fileName, obfuscatedName))
            {
                _obfuscatedFileNames.insert(obfuscatedName, fileName);
                _files << QFileInfo(obfuscatedName);
            }
            else
            {
                _files << fileInfo;
                _error(tr("Couldn't rename file %1").arg(fileName));
            }
        }
    }


    bool hasDir = false;
    for (const QFileInfo &fileInfo : _files)
    {
        if (fileInfo.isDir())
        {
            hasDir = true;
#if defined( Q_OS_WIN )
            QString path = QString("%1/").arg(fileInfo.absoluteFilePath());
            if (path.startsWith("//"))
                path.replace(QRegExp("^//"), "\\\\");
            args << path;
#else
            args << QString("%1/").arg(fileInfo.absoluteFilePath());
#endif
        }
        else
#if defined( Q_OS_WIN )
        {

            QString path = fileInfo.absoluteFilePath();
            if (path.startsWith("//"))
                path.replace(QRegExp("^//"), "\\\\");
            args << path;
        }
#else
            args << fileInfo.absoluteFilePath();
#endif
    }

    if (hasDir && !args.contains("-r"))
        args << "-r";

    // 3.: launch rar
    if (_ngPost->debugMode() || !_postWidget)
        _log(tr("Compressing files: %1 %2\n").arg(cmdRar).arg(args.join(" ")));
    else
        _log(tr("Compressing files...\n"));
    _limitProcDisplay = false;
    _extProc->start(cmdRar, args);

#ifdef __DEBUG__
    _log("[PostingJob::_compressFiles] compression started...");
#endif

    return true;
}


void PostingJob::onCompressionFinished(int exitCode)
{
    if (_ngPost->debugMode())
        _log(tr("=> rar exit code: %1\n").arg(exitCode));
    else
        _log("\n");


#ifdef __DEBUG__
    _log("[PostingJob::_compressFiles] compression finished...");
#endif

    if (_obfuscateFileName && !_delFilesAfterPost.load())
    {
        for (auto it = _obfuscatedFileNames.cbegin(), itEnd = _obfuscatedFileNames.cend(); it != itEnd; ++it)
            QFile::rename(it.key(), it.value());
    }


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
        _log(tr("Generating par2...\n"));
    if (!_ngPost->_par2Path.toLower().contains("parpar"))
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
        _log("\n");

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
        _log(tr("External process deleted."));
}

void PostingJob::_cleanCompressDir()
{
    if (!_keepRar)
        _compressDir->removeRecursively();
    delete _compressDir;
    _compressDir = nullptr;
    if (_ngPost->debugMode())
        _log(tr("Compressed files deleted."));
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

qint64 PostingJob::_dirSize(const QString &path)
{
    qint64 size = 0;
    QDir dir(path);
    for(const QFileInfo &fi: dir.entryInfoList(QDir::Files|QDir::Hidden|QDir::System|QDir::Dirs|QDir::NoDotAndDotDot)) {
        if (fi.isDir())
            size += _dirSize(fi.absoluteFilePath());
        else
            size+= fi.size();
    }
    return size;
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
