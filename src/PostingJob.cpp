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

#include "PostingJob.h"

#include <cmath>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QMutex>
#include <QProcess>
#include <QRegularExpression>
#include <QThread>

#include "NgPost.h"
#include "nntp/NntpArticle.h"
#include "nntp/NntpFile.h"
#include "nntp/ServerParams.h"
#include "NntpConnection.h"
#include "Poster.h"
#include "utils/Macros.h" // MB_FLUSH
#include "utils/NgFilePacker.h"
#include "utils/NgTools.h"
#include "utils/UnfinishedJob.h"
#ifdef __USE_HMI__
#  include "hmi/PostingWidget.h"
#endif

QSet<NNTP::File *> PostingJob::nntpFilesNotPosted() const
{
    QSet<NNTP::File *> missingFiles(_filesInProgress);
    for (auto *nntpFile : _filesToUpload)
        missingFiles << nntpFile;
    missingFiles += _filesFailed;
    return missingFiles;
}

QStringList PostingJob::unpostedFilesPath() const
{
    QStringList unpostedFiles;
    if (isNzbCreated())
    {
        // it means that the NntpFiles have been created
        // either the post was started or not
        for (auto *nntpFile : _filesToUpload)
            unpostedFiles << nntpFile->fileAbsolutePath();
        for (auto *nntpFile : _filesInProgress)
            unpostedFiles << nntpFile->fileAbsolutePath();
        for (auto *nntpFile : _filesFailed)
            unpostedFiles << nntpFile->fileAbsolutePath();
        return unpostedFiles;
    }

    // we use directly _files
    for (auto const &fi : _files)
        unpostedFiles << fi.absoluteFilePath();

    return unpostedFiles;
}

PostingJob::PostingJob(NgPost                 &ngPost,
                       PostingWidget          *postWidget,
                       PostingParamsPtr const &params,
                       QObject                *parent)
    : QObject(parent)
    , _ngPost(ngPost)
    , _params(params)
    , _postWidget(postWidget)
    , _tmpPath(params->tmpPath())
    , _packer(nullptr)
    , _files(_params->files())
    , _delFilesAfterPost(_params->delFilesAfterPost() ? 0x1 : 0x0)

    , _filesFailed()
    , _nbFiles(0)
    , _nbPosted(0)

    , _nntpFile(nullptr)
    , _file(nullptr)
    , _part(0)
    , _nzb(nullptr)

    , _totalSize(0)
    , _pauseDuration(0)

    , _nbConnections(0)
    , _nbArticlesUploaded(0)
    , _nbArticlesFailed(0)
    , _uploadedSize(0)
    , _nbArticlesTotal(0)
    , _stopPosting(0x0)
    , _noMoreFiles(0x0)
    , _isPaused(false)
    , _isActiveJob(false)
#ifdef __COMPUTE_IMMEDIATE_SPEED__
    , _immediateSize(0)
    , _immediateSpeed("0 B/s")
    , _useHMI(_ngPost.useHMI())
#endif
    , _state(JOB_STATE::NOT_STARTED)
    , _isResumeJob(false)
    , _dbJobId(0)

{
    _init();
}

PostingJob::PostingJob(NgPost                       &ngPost,
                       QString const                &rarName,
                       QString const                &rarPass,
                       QString const                &nzbFilePath,
                       QFileInfoList const          &files,
                       QList<QString> const         &grpList,
                       std::string const            &from,
                       SharedParams const           &sharedParams,
                       QMap<QString, QString> const &meta,
                       QObject                      *parent)
    : QObject(parent)
    , _ngPost(ngPost)
    , _params(new PostingParams(
              ngPost, rarName, rarPass, nzbFilePath, files, nullptr, grpList, from, sharedParams, meta))
    , _postWidget(nullptr)
    , _tmpPath(sharedParams->tmpPath())
    , _packer(nullptr)

    , _files(files)
    , _delFilesAfterPost(_params->delFilesAfterPost() ? 0x1 : 0x0)

    , _nbFiles(0)
    , _nbPosted(0)

    , _nntpFile(nullptr)
    , _file(nullptr)
    , _part(0)
    , _nzb(nullptr)

    , _totalSize(0)
    , _pauseTimer()
    , _pauseDuration(0)

    , _nbConnections(0)
    , _nbArticlesUploaded(0)
    , _nbArticlesFailed(0)
    , _uploadedSize(0)
    , _nbArticlesTotal(0)
    , _stopPosting(0x0)
    , _noMoreFiles(0x0)
    , _isPaused(false)
    , _isActiveJob(false)
#ifdef __COMPUTE_IMMEDIATE_SPEED__
    , _immediateSize(0)
    , _immediateSpeed("0 B/s")
    , _useHMI(_ngPost.useHMI())
#endif
    , _state(JOB_STATE::NOT_STARTED)
    , _isResumeJob(false)
    , _dbJobId(0)
{
    _init();
}

PostingJob::PostingJob(NgPost              &ngPost,
                       UnfinishedJob const &unfinshedJob,
                       QFileInfoList const &missingFiles,
                       int                  nbTotalFiles,
                       QObject             *parent)
    : QObject(parent)
    , _ngPost(ngPost)
    , _params(new PostingParams(ngPost,
                                "", // rarName
                                "", // rarPass
                                unfinshedJob.nzbFilePath,
                                missingFiles,
                                nullptr,
                                { unfinshedJob.groups },
                                std::string("resumeJob@ngPost.com"), // from
                                ngPost.postingParams()))
    , _postWidget(nullptr)
    , _tmpPath(ngPost.postingParams()->tmpPath())
    , _packingTmpPath(unfinshedJob.packingPath)
    , _packer(nullptr)

    , _files(missingFiles)
    , _delFilesAfterPost(_params->delFilesAfterPost() ? 0x1 : 0x0)

    , _nbFiles(nbTotalFiles)
    , _nbPosted(nbTotalFiles == 0 ? 0 : nbTotalFiles - missingFiles.size())

    , _nntpFile(nullptr)
    , _file(nullptr)
    , _part(0)
    , _nzb(nullptr)

    , _totalSize(0)
    , _pauseDuration(0)

    , _nbConnections(0)
    , _nbArticlesUploaded(0)
    , _nbArticlesFailed(0)
    , _uploadedSize(0)
    , _nbArticlesTotal(0)
    , _stopPosting(0x0)
    , _noMoreFiles(0x0)
    , _isPaused(false)
    , _isActiveJob(false)
#ifdef __COMPUTE_IMMEDIATE_SPEED__
    , _immediateSize(0)
    , _immediateSpeed("0 B/s")
    , _useHMI(_ngPost.useHMI())
#endif
    , _state(static_cast<JOB_STATE>(unfinshedJob.state))
    , _isResumeJob(true)
    , _dbJobId(unfinshedJob.jobIdDB)
{
    _init();
    if (isPacked())
        _params->setPackingDoneParamsForResume(); // will detach
}

PostingJob::~PostingJob()
{
#if defined(__DEBUG__) && defined(LOG_CONSTRUCTORS)
    _log(QString("Destructing PostingJob %1").arg(NgTools::ptrAddrInHex(this)), NgLogger::DebugLevel::Debug);
    qDebug() << "[PostingJob] <<<< Destruction " << NgTools::ptrAddrInHex(this)
             << " in thread : " << QThread::currentThread()->objectName();
#endif
    _log("Deleting PostingJob", NgLogger::DebugLevel::Debug);

    _ngPost.storeJobInDB(*this);
    qApp->processEvents(); //!< makeing sure we display the logs

    // stopThreads so it quits and waits _builderThread and _connectionsThread
    // threads will emit void QThread::finished()
    // that will run QObject::deleteLater for NntpConnections and
    for (Poster *poster : _posters)
        poster->stopThreads();

    if (_packer)
        delete _packer;

    qDeleteAll(_filesFailed);
    qDeleteAll(_filesInProgress);
    qDeleteAll(_filesToUpload);

    qDeleteAll(_posters);

    // no need to delete the NntpConnections as they deleteLater themselves
    // in the Poster::_builderThread where they have been moved

    if (_nzb)
        delete _nzb;
    if (_file)
        delete _file;

    _ngPost.doNzbPostCMD(this);
}

void PostingJob::_log(QString const &aMsg, NgLogger::DebugLevel debugLvl, bool newLine) const
{
    if (_params->quietMode())
        return;
    NgLogger::log(aMsg, newLine, debugLvl);
}

void PostingJob::_init()
{
#ifdef __DEBUG__
    qDebug() << "[PostingJob] >>>> _init " << this;
#endif
    connect(this, &PostingJob::sigStartPosting, this, &PostingJob::onStartPosting, Qt::QueuedConnection);
    connect(this, &PostingJob::sigStopPosting, this, &PostingJob::onStopPosting, Qt::QueuedConnection);
    connect(this, &PostingJob::sigPostingStarted, &_ngPost, &NgPost::onPostingJobStarted, Qt::QueuedConnection);
    connect(this, &PostingJob::sigPackingDone, &_ngPost, &NgPost::onPackingDone, Qt::QueuedConnection);
    connect(this,
            &PostingJob::sigPostingFinished,
            &_ngPost,
            &NgPost::onPostingJobFinished,
            Qt::QueuedConnection);
    connect(this,
            &PostingJob::sigNoMorePostingConnection,
            &_ngPost,
            &NgPost::onNoMorePostingConnection,
            Qt::QueuedConnection);

    //    connect(this, &PostingJob::sigScheduleNextArticle, this, &PostingJob::onPrepareNextArticle,
    //    Qt::QueuedConnection);

#ifdef __USE_HMI__
    if (_postWidget)
    {
        connect(this,
                &PostingJob::sigFilePosted,
                _postWidget,
                &PostingWidget::onFilePosted,
                Qt::QueuedConnection);
        connect(this,
                &PostingJob::sigArchiveFileNames,
                _postWidget,
                &PostingWidget::onArchiveFileNames,
                Qt::QueuedConnection);
        connect(this,
                &PostingJob::sigArticlesNumber,
                _postWidget,
                &PostingWidget::onArticlesNumber,
                Qt::QueuedConnection);
        connect(this,
                &PostingJob::postingFinished,
                _postWidget,
                &PostingWidget::onPostingJobDone,
                Qt::QueuedConnection);
    }
#endif

    connect(&_resumeTimer, &QTimer::timeout, this, &PostingJob::onResumeTriggered);
#ifdef __COMPUTE_IMMEDIATE_SPEED__
    if (_useHMI || _params->dispProgressBar())
        connect(&_immediateSpeedTimer,
                &QTimer::timeout,
                this,
                &PostingJob::onImmediateSpeedComputation,
                Qt::QueuedConnection);
#endif

    _log(NntpConnection::sslSupportInfo(), NgLogger::DebugLevel::Debug);
}

void PostingJob::onStartPosting(bool isActiveJob)
{
    _isActiveJob = isActiveJob; // MB_TODO what is actually _isActiveJob for? not primary Job? no posting
#ifdef __DEBUG__
    qDebug() << "[MB_TRACE][Issue#82][PostingJob::onStartPosting] job: " << this << ", file: " << nzbName()
             << " (isActive: " << isActiveJob << ")";
#endif
#ifdef __USE_HMI__
    if (_postWidget)
        _log(tr("<h3>Start Post #%1: %2</h3>").arg(_postWidget->jobNumber()).arg(nzbName()));
    else
#endif
        _log(QString("[%1] %2: %3").arg(NgTools::timestamp()).arg(tr("Start posting")).arg(nzbName()),
             NgLogger::DebugLevel::Info);

    // 1.: If we need compression we jump in startCompressFiles
    // and when the _extProcess is done we should arrive in onCompressionFinished
    if (_params->doCompress() && !isCompressed()) // for _resumeJob
    {
        if (_isResumeJob)
        {
            bool generate = false;
            if (_params->rarName().isEmpty())
            {
                _params->genRarName();
                generate = true;
            }
            if (_params->rarPass().isEmpty())
            {
                _params->genRarPass();
                generate = true;
            }
            if (generate)
                _log(tr("Resume Job: generating random archive name/pass : %1 / %2")
                             .arg(_params->rarName())
                             .arg(_params->rarPass()),
                     NgLogger::DebugLevel::Info);
        }
#ifdef __USE_TMP_RAM__
        shallWeUseTmpRam(); // we might modify _tmpPath
#endif
#ifdef __DEBUG__
        _log("[PostingJob::onStartPosting] Starting compression...", NgLogger::DebugLevel::Debug);
#endif
        if (!_isResumeJob)
            _packingTmpPath = QString("%1/%2").arg(_tmpPath).arg(nzbName());
        _packer = new NgFilePacker(*this, _packingTmpPath);
        if (!_packer->startCompressFiles())
            emit sigPostingFinished();
        return;
    }

    // 2.: If we need par2 generation without compression we jump in startGenPar2
    // and when the _extProcess is done we should arrive in onGenPar2Finished
    if (_params->doPar2() && !isPacked()) // for _resumeJob
    {
#ifdef __USE_TMP_RAM__
        shallWeUseTmpRam(); // we might modify _tmpPath
#endif
        if (!_isResumeJob)
            _packingTmpPath = QString("%1/%2").arg(_tmpPath).arg(nzbName());
        _packer = new NgFilePacker(*this, _packingTmpPath);
        if (!_packer->startGenPar2())
            emit sigPostingFinished();
        return;
    }

    // Otherwise no packing at all, we will post the original files
    _state = JOB_STATE::PACKING_DONE;
    _files = _params->files();
    _postFiles();
}

void PostingJob::_postFiles()
{
    _state = JOB_STATE::POST_STARTED;

#ifdef __USE_HMI__
    if (_postWidget) // in case we were in Pending mode
        _postWidget->setPosting();
#endif

    _initPosting();

    int nbThreads = _params->nbThreads();
    if (nbThreads > QThread::idealThreadCount())
        nbThreads = QThread::idealThreadCount();

    // Posters hold 2 threads ;)
    //   _builderThread: to prepare the Articles (Yenc encoding)
    //   _connectionsThread: to post them with one or several NntpConnection(s)
    int nbPosters = nbThreads / 2;
    if (nbPosters < 1)
        nbPosters = 1;
    int nbCon = _createNntpConnections();
    if (!nbCon)
    {
        NgLogger::error(tr("Error: there are no NntpConnection..."));
        _finishPosting(); // MB_TODO shall we?
        if (!_ngPost.useHMI())
            qApp->quit();
        return;
    }

    if (!_nzb->open((_isResumeJob ? QIODevice::ReadWrite : QIODevice::WriteOnly) | QIODevice::Text))
    {
        NgLogger::error(tr("Error: Can't create nzb output file: %1").arg(_params->nzbFilePath()));
        _finishPosting(); // MB_TODO shall we?
        return;
    }
    else
    {
        if (_isResumeJob && _nzb->size() != 0)
        {
            // go to the end of file minus last line "</nzb>"
            int nzbSize = _nzb->size();
            if (nzbSize > kSizeNzbFileEndTag)
            {
                _nzb->seek(nzbSize - kSizeNzbFileEndTag);
                QString lastWords = _nzb->read(kSizeNzbFileEndTag);
                if (lastWords == "</nzb>\n")
                    _nzb->seek(_nzb->size() - kSizeNzbFileEndTag);
            }
            else
                _nzb->seek(nzbSize);
            _nzbStream.setDevice(_nzb);
        }
        else
        {
            _nzbStream.setDevice(_nzb);
            _nzbStream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                       << "<!DOCTYPE nzb PUBLIC \"-//newzBin//DTD NZB 1.1//EN\" "
                          "\"http://www.newzbin.com/DTD/nzb/nzb-1.1.dtd\">\n"
                       << "<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n"
                       << kSpace << "<head>\n";
            // add the title in the header (first file name)
            _nzbStream << kSpace << kSpace << "<meta type=\"title\">" << _params->files().front().fileName()
                       << "</meta>\n";
            if (!_params->rarPass().isEmpty()) // lets add the password
                _nzbStream << kSpace << kSpace << "<meta type=\"password\">" << _params->rarPass()
                           << "</meta>\n";

            // add the other meta like the category or whatever...
            if (!_params->meta().isEmpty())
            {
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
                for (auto itMeta = _params->meta().cbegin(); itMeta != _params->meta().cend(); ++itMeta)
                    _nzbStream << kSpace << kSpace << "<meta type=\"" << itMeta.key() << "\">" << itMeta.value()
                               << "</meta>\n";
#else
                for (auto const &[key, val] : _params->meta().asKeyValueRange())
                    _nzbStream << kSpace << kSpace << "<meta type=\"" << key << "\">" << val << "</meta>\n";
#endif
            }
            _nzbStream << kSpace << "</head>\n\n" << MB_FLUSH;

            _state = JOB_STATE::NZB_CREATED;
        }
    }

    _timeStart.start();

    //    QMutexLocker lock(&_secureArticles); // start the connections but they must wait _prepareArticles

    if (nbPosters > nbCon)
        nbPosters = nbCon; // we can't have more thread than available connections

    _posters.reserve(nbPosters);
    int nbConPerPoster = static_cast<int>(std::floor(nbCon / nbPosters));
    int nbExtraCon     = nbCon - nbConPerPoster * nbPosters;
#ifdef __DEBUG__
    qDebug() << "[PostingJob::_postFiles] nbFiles: " << _filesToUpload.size() << ", nbPosters: " << nbPosters
             << ", nbCons: " << nbCon << " => nbCon per Poster: " << nbConPerPoster
             << " (nbExtraCon: " << nbExtraCon << ")";
#endif

    int conIdx = 0;
    for (ushort posterIdx = 0; posterIdx < nbPosters; ++posterIdx)
    {
        Poster *poster = new Poster(this, posterIdx);
        connect(poster,
                &Poster::sigNoMorePostingConnection,
                this,
                &PostingJob::onPosterWithNoMorePostingConnection,
                Qt::QueuedConnection);
        _posters.append(poster);
        poster->lockQueue(); // lock queue so the connection will wait before starting building Articles

        for (int i = 0; i < nbConPerPoster; ++i)
            poster->addConnection(_nntpConnections.at(conIdx++));

        if (nbExtraCon-- > 0)
            poster->addConnection(_nntpConnections.at(conIdx++));

        poster->startThreads();
    }

    // Prepare 2 Articles for each connections
    _preparePostersArticles();

#ifdef __COMPUTE_IMMEDIATE_SPEED__
    if (_useHMI || _params->dispProgressBar())
        _immediateSpeedTimer.start(kImmediateSpeedDurationMs);
#endif

    for (Poster *poster : _posters)
        poster->unlockQueue();

#ifdef __test_ngPost__
    for (Poster *poster : _posters)
        poster->startNntpConnections();
#endif

    emit sigPostingStarted();
}

void PostingJob::onPosterWithNoMorePostingConnection(Poster *poster)
{
    _posters.removeOne(poster);
    poster->deleteLater();
    if (_posters.isEmpty())
    {
        NgLogger::criticalError(tr("No poster has any connection allowing posting..."),
                                NgError::ERR_CODE::NO_POSTING_CONS);
        emit sigNoMorePostingConnection(this);
        _finishPosting();
    }
}

void PostingJob::onStopPosting()
{
#ifdef __MOVETOTHREAD_TRACKING__
    qDebug() << QString("[THREAD] %1 PostingJob %2 onStopPosting")
                        .arg(QThread::currentThread()->objectName())
                        .arg(NgTools::ptrAddrInHex(this));
#endif

    if (_packer)
        _packer->terminateAndWaitExternalProc();

    _finishPosting();
    emit sigPostingFinished();
}

void PostingJob::onDisconnectedConnection(NntpConnection *con)
{
    if (MB_LoadAtomic(_stopPosting))
        return; // we're destructing all the connections

    if (!con->hasNoMoreFiles())
        NgLogger::error(tr("Error: disconnected connection: #%1\n").arg(con->getId()));

    if (_nntpConnections.removeOne(con))
    {
        con->resetErrorCount(); // In case we will resume if we loose all
        _closedConnections.append(con);

        if (_nntpConnections.isEmpty())
        {
            if (con->hasNoMoreFiles())
                _finishPosting();
            else
            {
                NgLogger::error(tr("we lost all the connections..."));
                if (_params->tryResumePostWhenConnectionLost())
                {
                    int sleepDurationInSec = _params->waitDurationBeforeAutoResume();
                    _log(tr("Sleep for %1 sec before trying to reconnect").arg(sleepDurationInSec),
                         NgLogger::DebugLevel::Info);
                    _ngPost.pause();
                    _resumeTimer.start(sleepDurationInSec * 1000);
                }
                else
                    _finishPosting();
            }
        }
    }
}

void PostingJob::onNntpFileStartPosting()
{
    NNTP::File *nntpFile = static_cast<NNTP::File *>(sender());
    if (!_ngPost.useHMI())
        _log(QString("[%1][%2: %3] >>>>> %4")
                     .arg(NgTools::timestamp())
                     .arg(tr("avg. speed"))
                     .arg(avgSpeed())
                     .arg(nntpFile->name()),
             NgLogger::DebugLevel::Info);
}

void PostingJob::onNntpFilePosted()
{
    NNTP::File *nntpFile = static_cast<NNTP::File *>(sender());
    _totalSize += static_cast<quint64>(nntpFile->fileSize());
    ++_nbPosted;
    if (_postWidget)
        emit sigFilePosted(nntpFile->path(), nntpFile->nbArticles(), nntpFile->nbFailedArticles());

    if (_params->dispFilesPosting() && !_ngPost.useHMI())
        _log(QString("[%1][%2: %3] <<<<< %4")
                     .arg(NgTools::timestamp())
                     .arg(tr("avg. speed"))
                     .arg(avgSpeed())
                     .arg(nntpFile->name()),
             NgLogger::DebugLevel::Info);

    nntpFile->writeToNZB(_nzbStream,
                         _params->from(false)); // we don't want empty value if _obfuscateArticles
    _filesInProgress.remove(nntpFile);
    emit nntpFile->sigScheduleDeletion();
    if (_nbPosted == _nbFiles)
    {
#ifdef __DEBUG__
        _log(QString("All files have been posted => closing Job (nb article uploaded: %1, failed: %2)")
                     .arg(_nbArticlesUploaded)
                     .arg(_nbArticlesFailed),
             NgLogger::DebugLevel::Debug);
#endif

        _state = JOB_STATE::POSTED;
        _finishPosting();
    }
}

void PostingJob::onNntpErrorReading()
{
    NNTP::File *nntpFile = static_cast<NNTP::File *>(sender());
    ++_nbPosted;
    if (_postWidget)
        emit sigFilePosted(nntpFile->path(), nntpFile->nbArticles(), nntpFile->nbArticles());

    if (_params->dispFilesPosting() && !_ngPost.useHMI())
        _log(tr("[avg. speed: %1] <<<<< %2").arg(avgSpeed()).arg(nntpFile->name()), NgLogger::DebugLevel::Info);

    _filesInProgress.remove(nntpFile);
    _filesFailed.insert(nntpFile);
    if (_nbPosted == _nbFiles)
    {
#ifdef __DEBUG__
        _log(QString("All files have been posted => closing Job (nb article uploaded: %1, failed: %2)")
                     .arg(_nbArticlesUploaded)
                     .arg(_nbArticlesFailed),
             NgLogger::DebugLevel::Debug);
#endif

        _state = JOB_STATE::POSTED;
        _finishPosting();
    }
}

int PostingJob::_createNntpConnections()
{
    _nbConnections = _params->nbNntpConnections();
    _nntpConnections.reserve(_nbConnections);
    int conIdx = 0;
    for (NNTP::ServerParams *srvParams : _params->nntpServers())
    {
        if (srvParams->enabled)
        {
            for (int k = 0; k < srvParams->nbCons; ++k)
            {
                NntpConnection *nntpCon = new NntpConnection(_ngPost, ++conIdx, *srvParams);
                connect(nntpCon,
                        &NntpConnection::sigDisconnected,
                        this,
                        &PostingJob::onDisconnectedConnection,
                        Qt::QueuedConnection);
                _nntpConnections.append(nntpCon);
            }
        }
    }

    if (_ngPost.useHMI())
        _log(tr("Number of available Nntp Connections: %1").arg(_nbConnections), NgLogger::DebugLevel::Info);
    else
        _log(QString("[%1] %2: %3")
                     .arg(NgTools::timestamp())
                     .arg(tr("Number of available Nntp Connections"))
                     .arg(_nbConnections),
             NgLogger::DebugLevel::Info);

    return _nbConnections;
}

void PostingJob::_preparePostersArticles()
{
    _log("PostingJob::_prepareArticles", NgLogger::DebugLevel::FullDebug);

    for (int i = 0; i < kNbPreparedArticlePerConnection; ++i)
    {
        if (MB_LoadAtomic(_noMoreFiles))
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
    for (QFileInfo const &fi : _params->files())
    {
        QString path = fi.absoluteFilePath();
        _log(tr("Deleting posted %1: %2").arg(fi.isDir() ? tr("folder") : tr("file")).arg(path),
             NgLogger::DebugLevel::Info);
        if (fi.isDir())
        {
            QDir dir(path);
            dir.removeRecursively();
        }
        else
            QFile::remove(path);
    }
}

NNTP::Article *PostingJob::_readNextArticleIntoBufferPtr(QString const &threadName, char **bufferPtr)
{
    // qDebug() << "[PostingJob::readNextArticleIntoBufferPtr] " << threadName;
    if (!_nntpFile)
    {
        _nntpFile = _getNextFile();
        if (!_nntpFile)
        {
            _noMoreFiles = 0x1;
#ifdef __DEBUG__
            _log(tr("[%1] No more file to post...").arg(threadName), NgLogger::DebugLevel::Debug);
#endif
            return nullptr;
        }
    }

    if (!_file)
    {
        _file = new QFile(_nntpFile->path());
        if (_file->open(QIODevice::ReadOnly))
        {
            _log(tr("[%1] starting processing file %2").arg(threadName).arg(_nntpFile->path()),
                 NgLogger::DebugLevel::FullDebug);
            _part = 0;
        }
        else
        {
            if (NgLogger::isDebugMode())
                NgLogger::error(tr("[%1] Error: couldn't open file %2").arg(threadName).arg(_nntpFile->path()));
            else
                NgLogger::error(tr("Error: couldn't open file %1").arg(_nntpFile->path()));
            emit _nntpFile->sigErrorReadingFile();
            delete _file;
            _file     = nullptr;
            _nntpFile = nullptr;
            return _readNextArticleIntoBufferPtr(threadName, bufferPtr); // Check if we have more files
        }
    }

    if (_file)
    {
        qint64 pos       = _file->pos();
        qint64 bytesRead = _file->read(*bufferPtr, NgConf::kArticleSize);
        if (bytesRead > 0)
        {
            (*bufferPtr)[bytesRead] = '\0';
            _log(tr("[%1] we've read %2 bytes from %3 (=> new pos: %4)")
                         .arg(threadName)
                         .arg(bytesRead)
                         .arg(pos)
                         .arg(_file->pos()),
                 NgLogger::DebugLevel::FullDebug);
            ++_part;
            NNTP::Article *article =
                    new NNTP::Article(_nntpFile,
                                      _part,
                                      pos,
                                      bytesRead,
                                      _params->obfuscateArticles() ? nullptr : _params->fromStdPtr(),
                                      _params->obfuscateArticles());
            return article;
        }
        else
        {
            _log(tr("[%1] finished processing file %2").arg(threadName).arg(_nntpFile->path()),
                 NgLogger::DebugLevel::FullDebug);

            _file->close();
            delete _file;
            _file     = nullptr;
            _nntpFile = nullptr;
        }
    }

    return _readNextArticleIntoBufferPtr(threadName, bufferPtr); // if we didn't have an Article, check next file
}

void PostingJob::_initPosting()
{
    qDebug() << "[MB_TRACE][PostingJob::_initPosting] nzbFilePath: " << _params->nzbFilePath()
             << ", overwriteNzb: " << _params->overwriteNzb();

    // initialize buffer and nzb file
    if (!_isResumeJob && !_params->overwriteNzb()) // MB_TODO: for now never overwrite!
        _params->setNzbFilePath(NgTools::substituteExistingFile(_params->nzbFilePath()));
    _nzb = new QFile(_params->nzbFilePath());
    qDebug() << tr("[MB_TRACE ]Creating QFile(%1)").arg(_params->nzbFilePath());
    if (!_isResumeJob)
        _nbFiles = static_cast<uint>(_files.size());
    _params->dumpParams();

    int    numPadding = 1;
    double padding    = static_cast<double>(_nbFiles) / 10.;
    while (padding > 1.)
    {
        ++numPadding;
        padding /= 10.;
    }

    // initialize the NntpFiles (active objects)
    _filesToUpload.reserve(_isResumeJob ? _files.size() : static_cast<int>(_nbFiles));
    uint fileNum = _isResumeJob ? _nbPosted : 0;
    for (QFileInfo const &file : _files)
    {
        NNTP::File *nntpFile =
                new NNTP::File(this, file, ++fileNum, _nbFiles, numPadding, _params->groupsAccordingToPolicy());
        connect(nntpFile,
                &NNTP::File::sigAllArticlesArePosted,
                this,
                &PostingJob::onNntpFilePosted,
                Qt::QueuedConnection);
        connect(nntpFile,
                &NNTP::File::sigErrorReadingFile,
                this,
                &PostingJob::onNntpErrorReading,
                Qt::QueuedConnection);
        if (_params->dispFilesPosting()) //&& NgLogger::isDebugMode())
            connect(nntpFile,
                    &NNTP::File::sigStartPosting,
                    this,
                    &PostingJob::onNntpFileStartPosting,
                    Qt::QueuedConnection);

        _filesToUpload.enqueue(nntpFile);
        _nbArticlesTotal += nntpFile->nbArticles();

#ifdef __test_ngPost__
        if (_testThread)
            nntpFile->moveToThread(_testThread);
#endif
    }
    emit sigArticlesNumber(_nbArticlesTotal);
}

void PostingJob::_finishPosting()
{
#ifdef __DEBUG__
    qDebug() << "[MB_TRACE][PostingJob::_finishPosting]";
#endif
    if (MB_LoadAtomic(_stopPosting))
        return; // already came here

    _stopPosting = 0x1;

    if (_timeStart.isValid() && hasPostFinished())
    {
        _nbArticlesUploaded = _nbArticlesTotal; // we might not have processed the last onArticlePosted
        _uploadedSize       = _totalSize;
    }

    NgLogger::stopProgressBar(true);
    if (NgLogger::isDebugMode())
        _log("Finishing posting...", NgLogger::DebugLevel::Debug);

    // MB_TODO for v5 how to do that better!
    //    _ngPost._finishPosting(); // to update progress bar

    // 1.: print stats
    if (_timeStart.isValid())
        _printStats();

    for (NntpConnection *con : _nntpConnections)
        emit con->sigKillConnection();

    // 3.: close all the connections (they're living in the _threadPool)
    for (Poster *poster : _posters)
        poster->stopThreads();

    qApp->processEvents();

    // 4.: stop and wait for all threads
    // 2.: close nzb file
    _closeNzb();

    if (NgLogger::isDebugMode())
        _log("All posters stopped...", NgLogger::DebugLevel::Debug);

#ifdef __DEBUG__
    _log("All connections are closed...", NgLogger::DebugLevel::Debug);
#endif

    // 5.: print out the list of files that havn't been posted
    // (in case of disconnection)
    int nbPendingFiles = _filesToUpload.size() + _filesInProgress.size() + _filesFailed.size();
    if (nbPendingFiles)
    {
        NgLogger::error(
                tr("ERROR: there were %1 on %2 that havn't been posted:").arg(nbPendingFiles).arg(_nbFiles));
        bool isDebugMode = NgLogger::isDebugMode();
        for (NNTP::File *file : _filesInProgress)
        {
            QString msg = QString("  - %1").arg(file->stats());
            if (isDebugMode)
                msg += QString(" (fInProgress%1)").arg(file->missingArticles());
            NgLogger::error(msg);
        }
        for (NNTP::File *file : _filesToUpload)
        {
            QString msg = QString("  - %1").arg(file->stats());
            if (isDebugMode)
                msg += QString(" (fToUpload%1)").arg(file->missingArticles());
            NgLogger::error(msg);
        }
        for (NNTP::File *file : _filesFailed)
        {
            QString msg = QString("  - %1").arg(file->stats());
            if (isDebugMode)
                msg += QString(" (fFailed%1)").arg(file->missingArticles());
            NgLogger::error(msg);
        }
        NgLogger::error(tr("you can try to repost only those using ngPost --resume"));
    }
    else if (hasPostFinished() && MB_LoadAtomic(_delFilesAfterPost))
        _delOriginalFiles();

    emit sigPostingFinished();
}

void PostingJob::_closeNzb()
{
    if (_nzb)
    {
        if (_nzb->isOpen())
        {
            _nzbStream << kNzbFileEndTag;
            _nzb->close();
        }
        delete _nzb;
        _nzb = nullptr;
    }
}

void PostingJob::_printStats() const
{
    QString size = postSize();

    int    duration = static_cast<int>(_timeStart.elapsed() - _pauseDuration);
    double sec      = duration / 1000;

    QString msgEnd("\n"), ts = QString("[%1] ").arg(NgTools::timestamp());
    if (!_ngPost.useHMI())
        msgEnd += ts;
    msgEnd += tr("%1NZB file: %2\n").arg(_isResumeJob ? "Resumed " : "").arg(_params->nzbFilePath());
    if (!_ngPost.useHMI())
        msgEnd += ts;
    msgEnd += tr("Upload size: %1 in %2 (%3 sec) \
                 => average speed: %4 (%5 connections on %6 threads)\n")
                      .arg(size)
                      .arg(QTime::fromMSecsSinceStartOfDay(duration).toString("hh:mm:ss.zzz"))
                      .arg(sec)
                      .arg(avgSpeed())
                      .arg(_nntpConnections.size() + _closedConnections.size())
                      .arg(_posters.size() * 2);

    if (_nbArticlesFailed > 0)
        msgEnd += tr("%1 / %2 articles FAILED to be uploaded (even with %3 retries)...\n")
                          .arg(_nbArticlesFailed)
                          .arg(_nbArticlesTotal)
                          .arg(NNTP::Article::nbMaxTrySending());

    if (_nzb)
    {
        if (_params->doCompress())
        {
            if (!_ngPost.useHMI())
                msgEnd += ts;
            msgEnd += tr("file: %1, rar name: %2").arg(nzbName()).arg(_params->rarName());
            if (!_params->rarPass().isEmpty())
                msgEnd += tr(", rar pass: %1").arg(_params->rarPass());
        }
        if (!_params->urlNzbUpload())
            msgEnd += "\n";
    }

    _log(msgEnd, NgLogger::DebugLevel::Info);
}

QString PostingJob::nzbName() const { return _ngPost.getNzbName(QFileInfo(_params->nzbFilePath())); }

QString PostingJob::postSize() const { return NgTools::humanSize(static_cast<double>(_totalSize)); }

void PostingJob::pause()
{
    _log("Pause posting...", NgLogger::DebugLevel::Info);
    for (NntpConnection *con : _nntpConnections)
        emit con->sigKillConnection();

    _isPaused = true;
    _pauseTimer.start();
}

void PostingJob::resume()
{
    _log("Resume posting...", NgLogger::DebugLevel::Info);
    for (NntpConnection *con : _nntpConnections)
        emit con->sigStartConnection();

    _isPaused = false;
    _pauseDuration += _pauseTimer.elapsed();
}

QString PostingJob::getFilesPaths() const { return _params->getFilesPaths(); }

QString PostingJob::sslSupportInfo() { return NntpConnection::sslSupportInfo(); }

bool PostingJob::supportsSsl() { return NntpConnection::supportsSsl(); }

void PostingJob::onResumeTriggered()
{
    if (_isPaused)
    {
        _log(tr("Try to resume posting"), NgLogger::DebugLevel::Info);
        _nntpConnections.swap(_closedConnections);
        _ngPost.resume();
    }
}

#ifdef __COMPUTE_IMMEDIATE_SPEED__
void PostingJob::onImmediateSpeedComputation()
{
    QString power                    = " ";
    int     immediateSpeedDurationMs = kImmediateSpeedDurationMs;
    double  bandwidth                = 1000. * _immediateSize / immediateSpeedDurationMs;
    if (bandwidth > 1024)
    {
        bandwidth /= 1024;
        power = "k";
    }
    if (bandwidth > 1024)
    {
        bandwidth /= 1024;
        power = "M";
    }
    if (_useHMI || _params->dispProgressBar())
    {
        _immediateSpeed = QString("%1 %2B/s").arg(bandwidth, 6, 'f', 2).arg(power);
        _immediateSize  = 0;
        _immediateSpeedTimer.start(kImmediateSpeedDurationMs);
    }
}

#endif

#ifdef __USE_TMP_RAM__
void PostingJob::shallWeUseTmpRam()
{
    if (_params->useTmpRam())
    {
        qint64 sourceSize = 0;
        for (QFileInfo const &fi : _params->files())
            sourceSize += NgTools::recursivePathSize(fi);

        double sourceSizeWithRatio = _params->ramRatio() * sourceSize,
               availableSize       = static_cast<double>(_params->ramAvailable());
        if (sourceSizeWithRatio < availableSize)
        {
            _tmpPath = _params->ramPath();
            _log(tr("Using TMP_RAM path as temporary folder. Post size: %1")
                         .arg(NgTools::humanSize(static_cast<double>(sourceSize))),
                 NgLogger::DebugLevel::Info);
        }
        else
        {
            NgLogger::error(tr("Couldn't use TMP_RAM as there is not enough space: %1 available for a Post with "
                               "ratio of %2")
                                    .arg(NgTools::humanSize(availableSize))
                                    .arg(NgTools::humanSize(sourceSizeWithRatio)));
        }
    }
}
#endif
