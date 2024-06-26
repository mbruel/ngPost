#include "UnfinishedJob.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "PostingJob.h"
#include "utils/NgTools.h"

UnfinishedJob::UnfinishedJob(qint64           jobId,
                             QDateTime const &dt,
                             QString const   &tmp,
                             QString const   &nzbFile,
                             QString const   &nzbName,
                             qint64           s,
                             QString const   &grps,
                             ushort           state_,
                             QString const   &aName,
                             QString const   &aPass)
    : _logPrefix(QString("[uJob #%1 %2]").arg(jobId).arg(nzbName))
    , jobIdDB(jobId)
    , date(dt)
    , packingPath(tmp)
    , nzbFilePath(nzbFile)
    , nzbName(nzbName)
    , size(s)
    , groups(grps)
    , state(state_)
    , archiveName(aName)
    , archivePass(aPass)
{
}
QString UnfinishedJob::str() const
{
    QString s = QString("%1 <date: %2> <packing: %3> <nzb: %4> <archive name: %5> <archive pass: %6> <size: %7> "
                        "<state: %8> <grps: %9>")
                        .arg(_logPrefix)
                        .arg(date.toString(NgConf::kDateTimeFormat)
                                     .arg(packingPath)
                                     .arg(nzbFilePath)
                                     .arg(archiveName)
                                     .arg(archivePass)
                                     .arg(size)
                                     .arg(PostingJob::state(state))
                                     .arg(groups));
    for (auto const &f : filesToPost)
        s += QString("\n\t- %1").arg(f);
    return s;
}

bool UnfinishedJob::areFilesToPostAvailable() const
{
    for (auto const &filePath : filesToPost)
    {
        if (!QFile::exists(filePath))
        {
            NgLogger::error(tr("%1 file not available anymore: %2").arg(_logPrefix, filePath));
            return false;
        }
    }
    return true;
}

void UnfinishedJob::_removePar2FromFilesToPost() const
{
    auto it = filesToPost.begin();
    while (it != filesToPost.end())
    {
        if (it->endsWith(".par2"))
        {
            it = filesToPost.erase(it);
            continue;
        }
        ++it;
    }
}
ushort UnfinishedJob::_deleteExistingPar2() const
{
    QDir packingDir(packingPath);
    if (!packingDir.exists())
    {
        NgLogger::log(tr("%1 Packing dir '%2' doesn't exist...").arg(_logPrefix, packingPath),
                      true,
                      NgLogger::DebugLevel::Debug);
        return 0;
    }
    ushort nbDeletedPar2 = 0;
    for (QFileInfo const &fi : packingDir.entryInfoList({ "*.par2" }, QDir::Files, QDir::Name))
    {
        if (QFile::remove(fi.absoluteFilePath()))
        {
            ++nbDeletedPar2;
            NgLogger::log(tr("%1 Existing par2 removed: %2").arg(_logPrefix, fi.fileName()),
                          true,
                          NgLogger::DebugLevel::Debug);
        }
        else
            NgLogger::error(tr("%1 Error removing existing par2: %2").arg(_logPrefix, fi.fileName()));
    }

    if (nbDeletedPar2 != 0)
        NgLogger::log(tr("%1 %2 existing par2 have been deleted").arg(_logPrefix).arg(nbDeletedPar2),
                      true,
                      NgLogger::DebugLevel::Debug);

    return nbDeletedPar2;
}

bool UnfinishedJob::isCompressed() const { return state >= PostingJob::JOB_STATE::COMPRESSION_DONE; }
bool UnfinishedJob::packingDone() const { return state >= PostingJob::JOB_STATE::PACKING_DONE; }
bool UnfinishedJob::nzbStarted() const { return state >= PostingJob::JOB_STATE::NZB_CREATED; }

bool UnfinishedJob::hasEmptyPackingPath() const
{
    QFileInfo fi(packingPath);
    if (!fi.exists() || !fi.isDir() || QDir(packingPath).isEmpty())
        return false;
    return true;
}

bool UnfinishedJob::canBeResumed() const
{
    bool res = false;
    switch (state)
    {
    case PostingJob::JOB_STATE::NOT_STARTED:
        if (!hasFilesToPost())
        {
            NgLogger::error(tr("%1 has no file to post...").arg(_logPrefix));
            return false;
        }
        return areFilesToPostAvailable();

    case PostingJob::JOB_STATE::COMPRESSION_DONE:
        _deleteExistingPar2(); // par2 may have started but wasn't finished
        _removePar2FromFilesToPost();
        return _checkPackingExistAndNotEmpty();

    case PostingJob::JOB_STATE::PACKING_DONE: // Should nearly NEVER happen... (bad luck)
        if (!_checkPackingExistAndNotEmpty())
            return false;

    case PostingJob::JOB_STATE::NZB_CREATED:
        if (!_checkPackingExistAndNotEmpty() || !_checkNzbExistsAndHasReadWritePermission())
            return false;

    default:
        break;
    }

    return res;
}

bool UnfinishedJob::_checkNzbExistsAndHasReadWritePermission() const
{
    // first check if we can read/write the nzb
    QFileInfo fiNzb(nzbFilePath);
    if (!fiNzb.exists() || !fiNzb.isFile() || !fiNzb.isReadable() || !fiNzb.isWritable())
    {
        NgLogger::error(tr("\tCan't open nzb file for resume: %1").arg(nzbFilePath));
        return false;
    }

    return true;
}

bool UnfinishedJob::_checkPackingExistAndNotEmpty() const
{
    QFileInfo fiPackingPath(packingPath);
    if (!fiPackingPath.exists() || !fiPackingPath.isDir())
    {
        NgLogger::error(tr("\tPacking Path not available: %1").arg(packingPath));
        return false;
    }
    if (QDir(packingPath).isEmpty())
    {
        NgLogger::error(tr("\tPacking Path is empty: %1").arg(packingPath));
        return false;
    }
    return true;
}
