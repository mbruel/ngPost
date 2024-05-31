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
#include "nntp/NntpServerParams.h"
#include "NntpConnection.h"
#include "Poster.h"
#include "utils/Macros.h" // MB_FLUSH
#include "utils/NgTools.h"
#ifdef __USE_HMI__
#  include "hmi/PostingWidget.h"
#endif

PostingJob::PostingJob(NgPost                 &ngPost,
                       PostingWidget          *postWidget,
                       PostingParamsPtr const &params,
                       QObject                *parent)
    : QObject(parent)
    , _ngPost(ngPost)
    , _params(params)
    , _postWidget(postWidget)
    , _tmpPath(params->tmpPath())
    , _files(_params->files())
    , _delFilesAfterPost(_params->delFilesAfterPost() ? 0x1 : 0x0)

    , _extProc(nullptr)
    , _compressDir(nullptr)
    , _limitProcDisplay(false)
    , _nbProcDisp(42)

    , _nntpConnections()
    , _closedConnections()

    , _filesToUpload()
    , _filesInProgress()
    , _filesFailed()
    , _nbFiles(0)
    , _nbPosted(0)

    , _nzb(nullptr)
    , _nzbStream()
    , _nntpFile(nullptr)
    , _file(nullptr)
    , _part(0)
    , _timeStart()
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
    , _postStarted(false)
    , _packed(false)
    , _postFinished(false)
    , _secureDiskAccess()
    , _posters()
    , _isPaused(false)
    , _resumeTimer()
    , _isActiveJob(false)
#ifdef __COMPUTE_IMMEDIATE_SPEED__
    , _immediateSize(0)
    , _immediateSpeedTimer()
    , _immediateSpeed("0 B/s")
    , _useHMI(_ngPost.useHMI())
#endif
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
    , _files(files)
    , _delFilesAfterPost(_params->delFilesAfterPost() ? 0x1 : 0x0)

    , _extProc(nullptr)
    , _compressDir(nullptr)
    , _limitProcDisplay(false)
    , _nbProcDisp(42)

    , _nntpConnections()
    , _closedConnections()

    , _filesToUpload()
    , _filesInProgress()
    , _filesFailed()
    , _nbFiles(0)
    , _nbPosted(0)

    , _nzb(nullptr)
    , _nzbStream()
    , _nntpFile(nullptr)
    , _file(nullptr)
    , _part(0)
    , _timeStart()
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
    , _postStarted(false)
    , _packed(false)
    , _postFinished(false)
    , _secureDiskAccess()
    , _posters()
    , _isPaused(false)
    , _resumeTimer()
    , _isActiveJob(false)
#ifdef __COMPUTE_IMMEDIATE_SPEED__
    , _immediateSize(0)
    , _immediateSpeedTimer()
    , _immediateSpeed("0 B/s")
    , _useHMI(_ngPost.useHMI())
#endif
{
    _init();
}

PostingJob::~PostingJob()
{
#ifdef __DEBUG__
    _log(QString("Destructing PostingJob %1").arg(NgTools::ptrAddrInHex(this)), NgLogger::DebugLevel::Debug);
    qDebug() << "[PostingJob] <<<< Destruction " << NgTools::ptrAddrInHex(this)
             << " in thread : " << QThread::currentThread()->objectName();
#endif

    _log("Deleting PostingJob", NgLogger::DebugLevel::Debug);

    // stopThreads so it quits and waits _builderThread and _connectionsThread
    // threads will emit void QThread::finished()
    // that will run QObject::deleteLater for NntpConnections and
    for (Poster *poster : _posters)
        poster->stopThreads();

    if (_compressDir)
    {
        if (hasPostFinishedSuccessfully())
            _cleanCompressDir();
        delete _compressDir;
        _compressDir = nullptr;
    }

    if (_extProc)
        _cleanExtProc();

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
    //    NgLogger::log(QString("aMsg = %1, quiestMode: %2").arg(aMsg).arg(_params->quietMode()), newLine,
    //    debugLvl);
    if (_params->quietMode())
        return;
    NgLogger::log(aMsg, newLine, debugLvl);
}

void PostingJob::_init()
{
#ifdef __DEBUG__
    qDebug() << "[PostingJob] >>>> _init " << this;
#endif
    connect(this, &PostingJob::startPosting, this, &PostingJob::onStartPosting, Qt::QueuedConnection);
    connect(this, &PostingJob::stopPosting, this, &PostingJob::onStopPosting, Qt::QueuedConnection);
    connect(this, &PostingJob::postingStarted, &_ngPost, &NgPost::onPostingJobStarted, Qt::QueuedConnection);
    connect(this, &PostingJob::packingDone, &_ngPost, &NgPost::onPackingDone, Qt::QueuedConnection);
    connect(this, &PostingJob::postingFinished, &_ngPost, &NgPost::onPostingJobFinished, Qt::QueuedConnection);

    //    connect(this, &PostingJob::scheduleNextArticle, this, &PostingJob::onPrepareNextArticle,
    //    Qt::QueuedConnection);

#ifdef __USE_HMI__
    if (_postWidget)
    {
        connect(this, &PostingJob::filePosted, _postWidget, &PostingWidget::onFilePosted, Qt::QueuedConnection);
        connect(this,
                &PostingJob::archiveFileNames,
                _postWidget,
                &PostingWidget::onArchiveFileNames,
                Qt::QueuedConnection);
        connect(this,
                &PostingJob::articlesNumber,
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
        _log(QString("[%1] %2: %3").arg(timestamp()).arg(tr("Start posting")).arg(nzbName()),
             NgLogger::DebugLevel::None);

    // 1.: If we need compression we jump in startCompressFiles
    // and when the _extProcess is done we should arrive in onCompressionFinished
    if (_params->doCompress())
    {
#ifdef __USE_TMP_RAM__
        shallWeUseTmpRam(); // we might modify _tmpPath
#endif
#ifdef __DEBUG__
        _log("[PostingJob::onStartPosting] Starting compression...", NgLogger::DebugLevel::Debug);
#endif
        if (!startCompressFiles())
            emit postingFinished();
        return;
    }

    // 2.: If we need par2 generation without compression we jump in startGenPar2
    // and when the _extProcess is done we should arrive in onGenPar2Finished
    if (_params->doPar2())
    {
#ifdef __USE_TMP_RAM__
        shallWeUseTmpRam(); // we might modify _tmpPath
#endif
        if (!startGenPar2())
            emit postingFinished();
        return;
    }

    // Otherwise no packing at all, we will post the original files
    _packed = false;
    _files  = _params->files();
    _postFiles();
}

bool PostingJob::startCompressFiles()
{
    if (!_params->canCompress())
        return false;

    // 1.: create archive temporary folder
    _archiveTmpFolder = _createArchiveFolder(_tmpPath, _params->rarName());
    if (_archiveTmpFolder.isEmpty())
        return false;

    _extProc = new QProcess(this);
    connect(_extProc,
            &QProcess::readyReadStandardOutput,
            this,
            &PostingJob::onExtProcReadyReadStandardOutput,
            Qt::DirectConnection);
    connect(_extProc,
            &QProcess::readyReadStandardError,
            this,
            &PostingJob::onExtProcReadyReadStandardError,
            Qt::DirectConnection);
    connect(_extProc,
            static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this,
            &PostingJob::onCompressionFinished,
            Qt::QueuedConnection);

    QStringList args = _params->buildCompressionCommandArgumentsList();

#if defined(Q_OS_WIN)
    if (_archiveTmpFolder.startsWith("//"))
        _archiveTmpFolder.replace(QRegularExpression("^//"), "\\\\");
#endif
    args << QString("%1/%2.%3").arg(_archiveTmpFolder, _params->rarName(), _params->use7z() ? "7z" : "rar");

    // With the option obfuscateFileName users want to rename the files
    // before puting them in the archive
    // so we keep the map _obfuscatedFileNames to reverse the renaming later
    // and populate _files with the new random names...
    if (_params->obfuscateFileName())
    {
        _files.clear();
        for (QFileInfo const &fileInfo : _params->files())
        {
            QString obfuscatedName = QString("%1/%2").arg(fileInfo.absolutePath(),
                                                          NgTools::randomFileName(_params->lengthName())),
                    fileName       = fileInfo.absoluteFilePath();

            if (QFile::rename(fileName, obfuscatedName))
            {
                _obfuscatedFileNames.insert(obfuscatedName, fileName);
                _files << QFileInfo(obfuscatedName);
            }
            else
            {
                _files << fileInfo;
                NgLogger::error(tr("Couldn't rename file %1").arg(fileName));
            }
        }
    }

    // Trick with RAR_NO_ROOT_FOLDER if only one dir file to not double it when extracting...
    // + Windows shitty path replacement.. should switch to C++17 std::filesystem... one day?!?
    bool hasDir = false;
    if (_params->files().size() == 1)
    { // Only remove rar root folder if there is only ONE folder AND RAR_NO_ROOT_FOLDER is set
        QFileInfo const &fileInfo = _files.first();
        QString          path     = fileInfo.absoluteFilePath();
#if defined(Q_OS_WIN)
        if (path.startsWith("//"))
            path.replace(QRegularExpression("^//"), "\\\\");
#endif
        if (fileInfo.isDir())
        {
            hasDir = true;
            if (_params->removeRarRootFolder())
                path += "/";
        }
        args << path;
    }
    else
    {
        for (QFileInfo const &fileInfo : _files)
        {
            QString path = fileInfo.absoluteFilePath();
#if defined(Q_OS_WIN)
            if (path.startsWith("//"))
                path.replace(QRegularExpression("^//"), "\\\\");
#endif
            if (fileInfo.isDir())
                hasDir = true;
            args << path;
        }
    }

    // if we've a folder, add the recursive option!
    if (hasDir && !args.contains("-r"))
        args << "-r";

    // launch the compression! and ends up just under in onCompressionFinished if all ok...
    if (NgLogger::isDebugMode())
        _log(QString("[%1] %2: %3 %4\n")
                     .arg(timestamp())
                     .arg(tr("Compressing files"))
                     .arg(_params->rarPath())
                     .arg(args.join(" ")),
             NgLogger::DebugLevel::None);
    else
        _log(QString("%1...").arg(tr("Compressing files")), NgLogger::DebugLevel::None);
    _limitProcDisplay = false;
    _extProc->start(_params->rarPath(), args);

#ifdef __DEBUG__
    _log("[PostingJob::_compressFiles] compression started...", NgLogger::DebugLevel::Debug);
#endif

    return true;
}

void PostingJob::onCompressionFinished(int exitCode)
{
    if (NgLogger::isDebugMode())
        _log(tr("=> rar exit code: %1\n").arg(exitCode), NgLogger::DebugLevel::Debug);
    else
        _log("", NgLogger::DebugLevel::None);

#ifdef __DEBUG__
    _log("[PostingJob::_compressFiles] compression finished...", NgLogger::DebugLevel::Debug);
#endif

    // if we've done the obfuscateFileName, we can revert the renaming
    // except if we will delete the files when the posting is done ;
    if (_params->obfuscateFileName() && !MB_LoadAtomic(_delFilesAfterPost))
    {
        for (auto it = _obfuscatedFileNames.cbegin(), itEnd = _obfuscatedFileNames.cend(); it != itEnd; ++it)
            QFile::rename(it.key(), it.value());
    }

    // if the compression process failed we stop here...
    if (exitCode != 0)
    {
        NgLogger::error(tr("Error during compression: %1").arg(exitCode));
        _cleanCompressDir();
        emit postingFinished();
        return;
    }

    // we need to update _files with the generated archives
    _updateFilesListFromCompressDir();

    // Shall we continue with par2? quite straight forward...
    // we jump under in startGenPar2 then in onGenPar2Finished
    if (_params->doPar2())
    {
        disconnect(_extProc,
                   static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                   this,
                   &PostingJob::onCompressionFinished);
        if (!startGenPar2())
        {
            _cleanCompressDir();
            // MB_TODO 2024.05 shall we _cleanExtProc() ?
            emit postingFinished();
        }
        return;
    }

    // No par2 to be done so yeah we clean the extProc
    // and only start posting if we're the active job
    // as we may have starting packing as soon as the previous one started posting
    // we wait it finishes to not handling sharing NntpConnection
    // that would not benefit in any way...
    // Rq 2024.05: seems the _isActiveJob is useless no?
    _packed = true;
    _cleanExtProc();
    if (this == _ngPost._activeJob)
        _postFiles();

    emit packingDone(); //!< no par2 generation so next job can start packing ;)
}

bool PostingJob::startGenPar2()
{
    if (!_params->canGenPar2())
        return false;

    QStringList args;
    if (_params->par2Args().isEmpty())
        args << "c"
             << "-l"
             << "-m1024" << QString("-r%1").arg(_params->par2Pct());
    else
        args << _params->par2Args().split(kCmdArgsSeparator);

    bool useParPar = _params->useParPar();

    // we've already compressed => we gen par2 for the files in the archive folder
    if (_extProc)
    {
        if (useParPar && args.last().trimmed() != "-o")
            args << "-o";
        args << QString("%1/%2.par2").arg(_archiveTmpFolder, _params->rarName());
        if (useParPar)
            args << "-R" << _archiveTmpFolder;
        else
        {
            if (_params->use7z())
                args << QString("%1/%2.7z%3")
                                .arg(_archiveTmpFolder, _params->rarName(), _params->splitArchive() ? "*" : "");
            else
                args << QString("%1/%2*rar").arg(_archiveTmpFolder, _params->rarName());

            if (_params->par2Args().isEmpty() && (NgLogger::isDebugMode() || !_postWidget))
                args << "-q"; // remove the progressbar bar
        }
    }
    else
    { // par2 generation only => can't use folders or files from different drive (Windows)
        QString basePath = _files.first().absolutePath();
        if (useParPar)
        {
            args << "-f"
                 << "basename";
            if (args.last().trimmed() != "-o")
                args << "-o";
            args << QString("%1/%2.par2").arg(_archiveTmpFolder, _params->rarName());
        }
        else
        {
#if defined(Q_OS_WIN)
            QString basePathWin(basePath);
            basePathWin.replace("/", "\\");
            if (_params->useMultiPar())
                args << QString("/d%1").arg(basePathWin);
            else
                args << "-B" << basePathWin;
            QString par2File = QString("%1/%2.par2").arg(_archiveTmpFolder, _params->rarName());
            par2File.replace("/", "\\");
            args << par2File;
#else
            args << "-B" << basePath;
            args << QString("%1/%2.par2").arg(_archiveTmpFolder, _params->rarName());
#endif
        }

        for (QFileInfo const &fileInfo : _files)
        {
            if (fileInfo.isDir())
            {
                NgLogger::error(tr("you can't post folders without compression..."));
                return false;
            }
            QString path = fileInfo.absoluteFilePath();
#if defined(Q_OS_WIN)
            if (!useParPar && basePath != fileInfo.absolutePath())
            {
                //                NgLogger::error(tr("Due to par2 software limitation and to avoid unnecessary
                //                copies, all files must be in the same folder..."));
                NgLogger::error(
                        tr("only ParPar allows to generate par2 for files from different folders... you should "
                           "consider using it ;)"));
                return false;
            }
            path.replace("/", "\\");
#endif
            args << path;
        }

        //        QString archiveTmpFolder = _createArchiveFolder(_tmpPath, _params->rarName());
        if (_archiveTmpFolder.isEmpty())
            return false;

        _extProc = new QProcess(this);
        connect(_extProc,
                &QProcess::readyReadStandardOutput,
                this,
                &PostingJob::onExtProcReadyReadStandardOutput,
                Qt::DirectConnection);
        connect(_extProc,
                &QProcess::readyReadStandardError,
                this,
                &PostingJob::onExtProcReadyReadStandardError,
                Qt::DirectConnection);
    }

    connect(_extProc,
            static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this,
            &PostingJob::onGenPar2Finished,
            Qt::QueuedConnection);

    if (NgLogger::isDebugMode())
        _log(QString("[%1] %2: %3 %4")
                     .arg(timestamp())
                     .arg(tr("Generating par2"))
                     .arg(_params->par2Path())
                     .arg(args.join(" ")),
             NgLogger::DebugLevel::None);
    else
        _log(QString("%1...").arg(tr("Generating par2")), NgLogger::DebugLevel::None);

    if (!_params->useParPar())
        _limitProcDisplay = true;
    _nbProcDisp = 0;
    _extProc->start(_params->par2Path(), args);

    return true;
}

void PostingJob::onGenPar2Finished(int exitCode)
{
    if (NgLogger::isDebugMode())
        _log(tr("=> par2 exit code: %1\n").arg(exitCode), NgLogger::DebugLevel::Debug);
    else
        _log("", NgLogger::DebugLevel::None);

    _cleanExtProc();

    if (exitCode != 0)
    {
        NgLogger::error(tr("Error during par2 generation: %1").arg(exitCode));
        _cleanCompressDir();
        emit postingFinished();
        return;
    }

    // we need to update _files with the generated par2 files
    _updateFilesListFromCompressDir();

    _packed = true;
    if (this == _ngPost._activeJob)
        _postFiles();

    emit packingDone(); //!< no par2 generation so next job can start packing ;)
}

void PostingJob::_postFiles()
{
    _postStarted = true;

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
        return;
    }

    if (!_nzb->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        NgLogger::error(tr("Error: Can't create nzb output file: %1").arg(_params->nzbFilePath()));
        _finishPosting(); // MB_TODO shall we?
        return;
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
            _nzbStream << kSpace << kSpace << "<meta type=\"password\">" << _params->rarPass() << "</meta>\n";

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

    emit postingStarted();
}

void PostingJob::onStopPosting()
{
#ifdef __MOVETOTHREAD_TRACKING__
    qDebug() << QString("[THREAD] %1 PostingJob %2 onStopPosting")
                        .arg(QThread::currentThread()->objectName())
                        .arg(NgTools::ptrAddrInHex(this));
#endif

    if (_extProc)
    {
        _log(tr("killing external process..."), NgLogger::DebugLevel::None);
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
                         NgLogger::DebugLevel::None);
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
    NntpFile *nntpFile = static_cast<NntpFile *>(sender());
    if (!_ngPost.useHMI())
        _log(QString("[%1][%2: %3] >>>>> %4")
                     .arg(timestamp())
                     .arg(tr("avg. speed"))
                     .arg(avgSpeed())
                     .arg(nntpFile->name()),
             NgLogger::DebugLevel::None);
}

void PostingJob::onNntpFilePosted()
{
    NntpFile *nntpFile = static_cast<NntpFile *>(sender());
    _totalSize += static_cast<quint64>(nntpFile->fileSize());
    ++_nbPosted;
    if (_postWidget)
        emit filePosted(nntpFile->path(), nntpFile->nbArticles(), nntpFile->nbFailedArticles());

    if (_params->dispFilesPosting() && !_ngPost.useHMI())
        _log(QString("[%1][%2: %3] <<<<< %4")
                     .arg(timestamp())
                     .arg(tr("avg. speed"))
                     .arg(avgSpeed())
                     .arg(nntpFile->name()),
             NgLogger::DebugLevel::None);

    nntpFile->writeToNZB(_nzbStream,
                         _params->from(false)); // we don't want empty value if _obfuscateArticles
    _filesInProgress.remove(nntpFile);
    emit nntpFile->scheduleDeletion();
    if (_nbPosted == _nbFiles)
    {
#ifdef __DEBUG__
        _log(QString("All files have been posted => closing Job (nb article uploaded: %1, failed: %2)")
                     .arg(_nbArticlesUploaded)
                     .arg(_nbArticlesFailed),
             NgLogger::DebugLevel::Debug);
#endif

        _postFinished = true;
        _finishPosting();
    }
}

void PostingJob::onNntpErrorReading()
{
    NntpFile *nntpFile = static_cast<NntpFile *>(sender());
    ++_nbPosted;
    if (_postWidget)
        emit filePosted(nntpFile->path(), nntpFile->nbArticles(), nntpFile->nbArticles());

    if (_params->dispFilesPosting() && !_ngPost.useHMI())
        _log(tr("[avg. speed: %1] <<<<< %2").arg(avgSpeed()).arg(nntpFile->name()), NgLogger::DebugLevel::None);

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

        _postFinished = true;
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
                        &NntpConnection::disconnected,
                        this,
                        &PostingJob::onDisconnectedConnection,
                        Qt::QueuedConnection);
                _nntpConnections.append(nntpCon);
            }
        }
    }

    if (_ngPost.useHMI())
        _log(tr("Number of available Nntp Connections: %1").arg(_nbConnections), NgLogger::DebugLevel::None);
    else
        _log(QString("[%1] %2: %3")
                     .arg(timestamp())
                     .arg(tr("Number of available Nntp Connections"))
                     .arg(_nbConnections),
             NgLogger::DebugLevel::None);

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
             NgLogger::DebugLevel::None);
        if (fi.isDir())
        {
            QDir dir(path);
            dir.removeRecursively();
        }
        else
            QFile::remove(path);
    }
}

NntpArticle *PostingJob::_readNextArticleIntoBufferPtr(QString const &threadName, char **bufferPtr)
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
            emit _nntpFile->errorReadingFile();
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
            NntpArticle *article =
                    new NntpArticle(_nntpFile,
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
    if (!_params->overwriteNzb()) // MB_TODO: for now never overwrite!
        _params->setNzbFilePath(NgTools::substituteExistingFile(_params->nzbFilePath()));
    _nzb = new QFile(_params->nzbFilePath());
    qDebug() << tr("[MB_TRACE ]Creating QFile(%1)").arg(_params->nzbFilePath());
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
    _filesToUpload.reserve(static_cast<int>(_nbFiles));
    uint fileNum = 0;
    for (QFileInfo const &file : _files)
    {
        NntpFile *nntpFile =
                new NntpFile(this, file, ++fileNum, _nbFiles, numPadding, _params->groupsAccordingToPolicy());
        connect(nntpFile,
                &NntpFile::allArticlesArePosted,
                this,
                &PostingJob::onNntpFilePosted,
                Qt::QueuedConnection);
        connect(nntpFile,
                &NntpFile::errorReadingFile,
                this,
                &PostingJob::onNntpErrorReading,
                Qt::QueuedConnection);
        if (_params->dispFilesPosting()) //&& NgLogger::isDebugMode())
            connect(nntpFile,
                    &NntpFile::startPosting,
                    this,
                    &PostingJob::onNntpFileStartPosting,
                    Qt::QueuedConnection);

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
    if (MB_LoadAtomic(_stopPosting))
        return; // already came here

    _stopPosting = 0x1;

    if (_timeStart.isValid() && _postFinished)
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
        emit con->killConnection();

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
        for (NntpFile *file : _filesInProgress)
        {
            QString msg = QString("  - %1").arg(file->stats());
            if (isDebugMode)
                msg += QString(" (fInProgress%1)").arg(file->missingArticles());
            NgLogger::error(msg);
        }
        for (NntpFile *file : _filesToUpload)
        {
            QString msg = QString("  - %1").arg(file->stats());
            if (isDebugMode)
                msg += QString(" (fToUpload%1)").arg(file->missingArticles());
            NgLogger::error(msg);
        }
        for (NntpFile *file : _filesFailed)
        {
            QString msg = QString("  - %1").arg(file->stats());
            if (isDebugMode)
                msg += QString(" (fFailed%1)").arg(file->missingArticles());
            NgLogger::error(msg);
        }
        NgLogger::error(tr("you can try to repost only those and concatenate the nzb with the current one ;)"));
    }
    else if (_postFinished && MB_LoadAtomic(_delFilesAfterPost))
        _delOriginalFiles();

    emit postingFinished();
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

    int    duration = static_cast<int>(_timeStart.elapsed() - _pauseDuration);
    double sec      = duration / 1000;

    QString msgEnd("\n"), ts = QString("[%1] ").arg(timestamp());
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
                          .arg(NntpArticle::nbMaxTrySending());

    if (_nzb)
    {
        if (!_ngPost.useHMI())
            msgEnd += ts;
        msgEnd += tr("nzb file: %1\n").arg(_params->nzbFilePath());
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

    _log(msgEnd, NgLogger::DebugLevel::None);
}

void PostingJob::_cleanExtProc()
{
    delete _extProc;
    _extProc = nullptr;
    _log(tr("External process deleted."), NgLogger::DebugLevel::FullDebug);
}

void PostingJob::_cleanCompressDir()
{
    if (!_params->keepRar())
        _compressDir->removeRecursively();
    if (NgLogger::isDebugMode())
        _log(tr("Compressed files deleted."), NgLogger::DebugLevel::Debug);
}

QStringList PostingJob::_updateFilesListFromCompressDir()
{
    _files.clear();
    QStringList archiveNames;
    for (QFileInfo const &file : _compressDir->entryInfoList(QDir::Files, QDir::Name))
    {
        _files << file;
        archiveNames << file.absoluteFilePath();
        NgLogger::log(QString("  - %1").arg(file.fileName()), true, NgLogger::DebugLevel::Debug);
    }
    emit archiveFileNames(archiveNames); // to update the content of _postWidget
    return archiveNames;
}

QString PostingJob::_createArchiveFolder(QString const &tmpFolder, QString const &archiveName)
{
    QString archiveTmpFolder = QString("%1/%2").arg(tmpFolder).arg(archiveName);
    _compressDir             = new QDir(archiveTmpFolder);
    if (_compressDir->exists())
    {
        NgLogger::log(tr("The temporary directory '%1' already exists...").arg(archiveTmpFolder), true);
        archiveTmpFolder = NgTools::substituteExistingFile(archiveTmpFolder, false, false);
        delete _compressDir;
        _compressDir = new QDir(archiveTmpFolder);
        if (_compressDir->exists())
        {
            delete _compressDir;
            _compressDir = nullptr;
            return QString();
        }
        else
            NgLogger::error(tr("We're using another temporary folder : %1").arg(archiveTmpFolder));
    }

    if (!_compressDir->mkpath("."))
    {
        NgLogger::error(tr("Couldn't create the temporary folder: '%1'...").arg(_compressDir->absolutePath()));
        delete _compressDir;
        _compressDir = nullptr;
        return QString();
    }

    return archiveTmpFolder;
}

void PostingJob::onExtProcReadyReadStandardOutput()
{
    if (NgLogger::isDebugMode())
        _log(_extProc->readAllStandardOutput(), NgLogger::DebugLevel::Debug, false);
    else if (_isActiveJob)
    {
        if (!_limitProcDisplay || ++_nbProcDisp % 42 == 0)
            _log("*", NgLogger::DebugLevel::None, false);
    }
}

void PostingJob::onExtProcReadyReadStandardError() { NgLogger::error(_extProc->readAllStandardError()); }

QString PostingJob::nzbName() const { return _ngPost.getNzbName(QFileInfo(_params->nzbFilePath())); }

QString PostingJob::postSize() const { return NgTools::humanSize(static_cast<double>(_totalSize)); }

void PostingJob::pause()
{
    _log("Pause posting...", NgLogger::DebugLevel::None);
    for (NntpConnection *con : _nntpConnections)
        emit con->killConnection();

    _isPaused = true;
    _pauseTimer.start();
}

void PostingJob::resume()
{
    _log("Resume posting...", NgLogger::DebugLevel::None);
    for (NntpConnection *con : _nntpConnections)
        emit con->startConnection();

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
        _log(tr("Try to resume posting"), NgLogger::DebugLevel::None);
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
                 NgLogger::DebugLevel::None);
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
