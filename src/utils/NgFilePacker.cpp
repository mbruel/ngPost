#include "NgFilePacker.h"

#include <QDir>
#include <QProcess>

#include "NgLogger.h"
#include "PostingJob.h"
#include "utils/NgTools.h"

NgFilePacker::NgFilePacker(PostingJob &job, QString const &packingTmpPath, QObject *parent)
    : QObject(parent)
    , _job(job)
    , _files(job._files)
    , _params(job._params)
    , _packingTmpPath(packingTmpPath)
    , _dispStdout(job._isActiveJob)
    , _quietMode(_params->quietMode())
    , _extProc(nullptr)
    , _packingTmpDir(nullptr)
    , _limitProcDisplay(false)
    , _nbProcDisp(0)

{
}

NgFilePacker::~NgFilePacker()
{
    if (_extProc)
        _cleanExtProc();

    if (_packingTmpDir)
    {
        if (_job.hasPostFinishedSuccessfully())
            _cleanCompressDir();
        delete _packingTmpDir;
        _packingTmpDir = nullptr;
    }
    else if (_job._isResumeJob && _job.hasPostFinishedWithAllFiles())
    {
        // _packingTmpDir only created in PostingJob::_createArchiveFolder
        // for resume job that has finished we need to clean it
        _packingTmpDir = new QDir(_packingTmpPath);
        _cleanCompressDir();
        delete _packingTmpDir;
    }
}

bool NgFilePacker::startCompressFiles()
{
    if (!_params->canCompress())
        return false;

    // 1.: create archive temporary folder
    _createArchiveFolder();
    if (_packingTmpPath.isEmpty())
        return false;

    _createExtProcAndConnectStreams();
    connect(_extProc,
            static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this,
            &NgFilePacker::onCompressionFinished,
            Qt::QueuedConnection);

    QStringList args = _params->buildCompressionCommandArgumentsList();

#if defined(Q_OS_WIN)
    if (_packingTmpPath.startsWith("//"))
        _packingTmpPath.replace(QRegularExpression("^//"), "\\\\");
#endif
    args << QString("%1/%2.%3").arg(_packingTmpPath, _params->rarName(), _params->use7z() ? "7z" : "rar");

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
                     .arg(NgTools::timestamp())
                     .arg(tr("Compressing files"))
                     .arg(_params->rarPath())
                     .arg(args.join(" ")),
             NgLogger::DebugLevel::Info);
    else
        _log(QString("%1...").arg(tr("Compressing files")), NgLogger::DebugLevel::Info);
    _limitProcDisplay = false;
    _extProc->start(_params->rarPath(), args);

#ifdef __DEBUG__
    _log("[PostingJob::_compressFiles] compression started...", NgLogger::DebugLevel::Debug);
#endif

    return true;
}

bool NgFilePacker::startGenPar2()
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
        args << QString("%1/%2.par2").arg(_packingTmpPath, _params->rarName());
        if (useParPar)
            args << "-R" << _packingTmpPath;
        else
        {
            if (_params->use7z())
                args << QString("%1/%2.7z%3")
                                .arg(_packingTmpPath, _params->rarName(), _params->splitArchive() ? "*" : "");
            else
                args << QString("%1/%2*rar").arg(_packingTmpPath, _params->rarName());

            if (_params->par2Args().isEmpty() && (NgLogger::isDebugMode() || !_job._postWidget))
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
            args << QString("%1/%2.par2").arg(_packingTmpPath, _params->rarName());
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
            QString par2File = QString("%1/%2.par2").arg(_packingTmpPath, _params->rarName());
            par2File.replace("/", "\\");
            args << par2File;
#else
            args << "-B" << basePath;
            args << QString("%1/%2.par2").arg(_packingTmpPath, _params->rarName());
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
        if (_packingTmpPath.isEmpty())
            return false;

        _createExtProcAndConnectStreams();
    }

    connect(_extProc,
            static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this,
            &NgFilePacker::onGenPar2Finished,
            Qt::QueuedConnection);

    if (NgLogger::isDebugMode())
        _log(QString("[%1] %2: %3 %4")
                     .arg(NgTools::timestamp())
                     .arg(tr("Generating par2"))
                     .arg(_params->par2Path())
                     .arg(args.join(" ")),
             NgLogger::DebugLevel::Info);
    else
        _log(QString("%1...").arg(tr("Generating par2")), NgLogger::DebugLevel::Info);

    if (!_params->useParPar())
        _limitProcDisplay = true;
    _nbProcDisp = 0;
    _extProc->start(_params->par2Path(), args);

    return true;
}

void NgFilePacker::terminateAndWaitExternalProc()
{
    if (_extProc)
    {
        _log(tr("killing external process..."), NgLogger::DebugLevel::Info);
        _extProc->terminate();
        _extProc->waitForFinished();
    }
}

void NgFilePacker::onCompressionFinished(int exitCode)
{
    if (NgLogger::isDebugMode())
        _log(tr("Compression done with exit code: %1\n").arg(exitCode), NgLogger::DebugLevel::Debug);
    else
        _log("", NgLogger::DebugLevel::Info); // new line...

#ifdef __DEBUG__
    _log("[PostingJob::_compressFiles] compression finished...", NgLogger::DebugLevel::Debug);
#endif

    // if we've done the obfuscateFileName, we can revert the renaming
    // except if we will delete the files when the posting is done ;
    if (_params->obfuscateFileName() && !MB_LoadAtomic(_job._delFilesAfterPost))
    {
        for (auto it = _obfuscatedFileNames.cbegin(), itEnd = _obfuscatedFileNames.cend(); it != itEnd; ++it)
            QFile::rename(it.key(), it.value());
    }

    // if the compression process failed we stop here...
    if (exitCode == 0)
        _job._state = PostingJob::JOB_STATE::COMPRESSION_DONE;
    else
    {
        NgLogger::error(tr("Error during compression: %1").arg(exitCode));
        _cleanCompressDir();
        emit _job.sigPostingFinished();
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
                   &NgFilePacker::onCompressionFinished);
        if (!startGenPar2())
        {
            _cleanCompressDir();
            // MB_TODO 2024.05 shall we _cleanExtProc() ?
            emit _job.sigPostingFinished();
        }
        return;
    }

    // No par2 to be done so yeah we clean the extProc
    // and only start posting if we're the active job
    // as we may have starting packing as soon as the previous one started posting
    // we wait it finishes to not handling sharing NntpConnection
    // that would not benefit in any way...
    // Rq 2024.05: seems the _isActiveJob is useless no?
    _job._state = PostingJob::JOB_STATE::PACKING_DONE;
    _cleanExtProc();
    if (_job._isActiveJob)
        _job._postFiles();

    emit _job.sigPackingDone(); //!< no par2 generation so next job can start packing ;)
}

void NgFilePacker::onGenPar2Finished(int exitCode)
{
    if (NgLogger::isDebugMode())
        _log(tr("Done with exit code: %1\n").arg(exitCode), NgLogger::DebugLevel::Debug);
    else
        _log("", NgLogger::DebugLevel::Info);

    _cleanExtProc();

    if (exitCode == 0)
        _job._state = PostingJob::JOB_STATE::PACKING_DONE;
    else
    {
        NgLogger::error(tr("Error during par2 generation: %1").arg(exitCode));
        _cleanCompressDir();
        emit _job.sigPostingFinished();
        return;
    }

    // we need to update _files with the generated par2 files
    _updateFilesListFromCompressDir();

    _job._state = PostingJob::JOB_STATE::PACKING_DONE;
    if (_job._isActiveJob)
        _job._postFiles();

    emit _job.sigPackingDone(); //!< no par2 generation so next job can start packing ;)
}

void NgFilePacker::onExtProcReadyReadStandardOutput()
{
    if (!_dispStdout) // MB_TODO: to be initialized with _isActiveJob to remove next if!
        return;       // MB_TODO this can also be removed as it is used to connect or not extProc stdout
    if (NgLogger::isFullDebug())
        _log(_extProc->readAllStandardOutput(), NgLogger::DebugLevel::FullDebug, false);
    else if (!_limitProcDisplay || ++_nbProcDisp % kProcDisplayLimit == 0)
        _log("*", NgLogger::DebugLevel::Info, false);
}

void NgFilePacker::onExtProcReadyReadStandardError() { NgLogger::error(_extProc->readAllStandardError()); }

void NgFilePacker::_createArchiveFolder()
{
    _packingTmpDir = new QDir(_packingTmpPath);
    if (_job._isResumeJob)
        return;

    if (_packingTmpDir->exists())
    {
        NgLogger::log(tr("The temporary directory '%1' already exists...").arg(_packingTmpPath), true);
        _packingTmpPath = NgTools::substituteExistingFile(_packingTmpPath, false, false);
        delete _packingTmpDir;
        _packingTmpDir = new QDir(_packingTmpPath);
        if (_packingTmpDir->exists())
        {
            delete _packingTmpDir;
            _packingTmpDir = nullptr;
            return;
        }
        else
            NgLogger::error(tr("We're using another temporary folder : %1").arg(_packingTmpPath));
    }

    if (!_packingTmpDir->mkpath("."))
    {
        NgLogger::error(tr("Couldn't create the temporary folder: '%1'...").arg(_packingTmpDir->absolutePath()));
        delete _packingTmpDir;
        _packingTmpDir = nullptr;
        return;
    }

    return;
}

void NgFilePacker::_createExtProcAndConnectStreams()
{
    _extProc = new QProcess(this);
    connect(_extProc,
            &QProcess::readyReadStandardError,
            this,
            &NgFilePacker::onExtProcReadyReadStandardError,
            Qt::DirectConnection);
    if (_dispStdout)
        connect(_extProc,
                &QProcess::readyReadStandardOutput,
                this,
                &NgFilePacker::onExtProcReadyReadStandardOutput,
                Qt::DirectConnection);
}

void NgFilePacker::_cleanExtProc()
{
    delete _extProc;
    _extProc = nullptr;
    _log(tr("External process deleted."), NgLogger::DebugLevel::FullDebug);
}

void NgFilePacker::_cleanCompressDir()
{
    if (!_params->keepRar())
        _packingTmpDir->removeRecursively();
    _log(tr("Compressed files deleted."), NgLogger::DebugLevel::Debug);
}

void NgFilePacker::_updateFilesListFromCompressDir()
{
    _files.clear();
    QStringList archiveNames;
    for (QFileInfo const &file : _packingTmpDir->entryInfoList(QDir::Files, QDir::Name))
    {
        _files << file;
        archiveNames << file.absoluteFilePath();
        NgLogger::log(QString("  - %1").arg(file.fileName()), true, NgLogger::DebugLevel::Debug);
    }
    emit _job.sigArchiveFileNames(archiveNames); // to update the content of _postWidget
}
