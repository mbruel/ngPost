#include "NgHistoryDatabase.h"

#include <QDebug>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>

#include "nntp/NntpFile.h"
#include "utils/NgLogger.h"
#include "utils/NgTools.h"

#include "PostingJob.h"

using namespace NgConf;
NgHistoryDatabase::NgHistoryDatabase() { }

qint64 NgHistoryDatabase::postedSizeInMB(DATE_CONDITION since) const
{
    qint64  megas = 0;
    QString req(DB::SQL::kSelectSizes);
    if (since != DATE_CONDITION::ALL)
    {
        req.append(" WHERE ");
        switch (since)
        {
        case DATE_CONDITION::YEAR:
            req.append(DB::SQL::kYearCondition);
            break;
        case DATE_CONDITION::MONTH:
            req.append(DB::SQL::kMonthCondition);
            break;
        case DATE_CONDITION::WEEK:
            req.append(DB::SQL::kWeekCondition);
            break;
        case DATE_CONDITION::DAY:
            req.append(DB::SQL::kDayCondition);
            break;
        default:
            break;
        }
    }
    QSqlQuery query;
    if (!query.exec(req))
    {
        NgLogger::error(tr("Error executing postedSize query : %1  (query: %2)")
                                .arg(query.lastError().text())
                                .arg(query.lastQuery()));
        return 0;
    }
    while (query.next())
        megas += megaSize(query.value(0).toString());

    query.finish();
    return megas;
}

QString NgHistoryDatabase::postedSize(DATE_CONDITION since) const
{
    double  size = static_cast<double>(postedSizeInMB(since));
    QString unit = "MB";
    if (size > 1024)
    {
        size /= 1024;
        unit = "GB";
    }
    if (size > 1024)
    {
        size /= 1024;
        unit = "TB";
    }
    return QString("%1 %2").arg(size, 0, 'f', 2).arg(unit);
}

int NgHistoryDatabase::insertPostingJob(PostingJob const &job)
{
    qDebug() << QString("[MB_TRACE][Database::insertPostingJob][%1] >>>>>>>>>>>").arg(job.nzbName());
    if (job.isResumeJob())
    {
        qDebug() << QString("[MB_TRACE][Database::insertPostingJob][%1] >>>>>>>>>>>").arg(job.nzbName())
                 << " resume " << job.areAllNntpFilesPosted();

        bool res = _markUnfinishedJobDone(job.dbJobId(), job.areAllNntpFilesPosted());
        qApp->processEvents(); // As it happens at PostingJob destruction, make sure the log event is
                               // processed
        return res ? 1 : 0;
    }

    QSqlQuery jobQuery = _postingJobQuery(job);
    if (!jobQuery.exec())
    {
        NgLogger::error(tr("Error inserting post %1 to history DB: %2  (query: %3)")
                                .arg(job.nzbName())
                                .arg(jobQuery.lastError().text())
                                .arg(jobQuery.lastQuery()));
        qDebug() << QString("[MB_TRACE][Database::insertPostingJob][%1] <<<<<<<<<<< s1").arg(job.nzbName());
        return 0;
    }

    if (job.hasPostFinished())
    {
        qDebug() << QString("[MB_TRACE][Database::insertPostingJob][%1] <<<<<<<<<<< s2").arg(job.nzbName());

        return jobQuery.lastInsertId().toInt();
    }

    // so post is not finished!
    int dbJobId = jobQuery.lastInsertId().toInt();
    qDebug() << QString("[MB_TRACE][Database::insertPostingJob][%1] dbJobId = %2")
                        .arg(job.nzbName())
                        .arg(dbJobId);

    auto const missingFiles = job.nntpFilesNotPosted();
    if (missingFiles.isEmpty())
    {
        NgLogger::error(tr("we've %1 missing file(s) although the job is supposed to be complete...")
                                .arg(missingFiles.size()));
        return dbJobId;
    }

    jobQuery.finish();
    for (auto *nntpFile : missingFiles)
    {
        QSqlQuery fileQuery;
        fileQuery.prepare(DB::SQL::kInsertUnfinishedStatement);
        fileQuery.bindValue(":job_id", dbJobId);
        fileQuery.bindValue(":filePath", nntpFile->fileInfo().absoluteFilePath());
        if (!fileQuery.exec())
        {
            NgLogger::error(tr("Error saving path of unposted file '%1' for post %2 : %3  (query: %4) with "
                               "values: %5, %6")
                                    .arg(nntpFile->fileInfo().absoluteFilePath())
                                    .arg(job.nzbName())
                                    .arg(fileQuery.lastError().text())
                                    .arg(fileQuery.lastQuery())
                                    .arg(dbJobId)
                                    .arg(nntpFile->fileInfo().absoluteFilePath()));

            qDebug() << QString("[MB_TRACE][Database::insertPostingJob][%1] <<<<<<<<<<< s3").arg(job.nzbName());

            return dbJobId;
        }
        fileQuery.clear();
    }
    qDebug() << QString("[MB_TRACE][Database::insertPostingJob][%1] <<<<<<<<<<< s4").arg(job.nzbName());

    return dbJobId;
}

int NgHistoryDatabase::_insertPost(QString const &date,
                                   QString const &nzbName,
                                   QString const &size,
                                   QString const &avgSpeed,
                                   QString const &archiveName,
                                   QString const &archivePass,
                                   QString const &groups,
                                   QString const &from,
                                   QString const &packingPath,
                                   QString const &nzbFilePath,
                                   int            nbFiles,
                                   int            state)
{
    if (!isDbInitialized())
    {
        NgLogger::error(tr("Trying to insert in Database not initialized..."));
        return 0;
    }
    QSqlQuery query;
    query.prepare(DB::SQL::kInsertJob);
    query.bindValue(":date", date);
    query.bindValue(":nzbName", nzbName);
    query.bindValue(":size", size);
    query.bindValue(":avgSpeed", avgSpeed);
    query.bindValue(":archiveName", archiveName);
    query.bindValue(":archivePass", archivePass);
    query.bindValue(":groups", groups);
    query.bindValue(":from", from);
    query.bindValue(":packingPath", packingPath);
    query.bindValue(":nzbFilePath", nzbFilePath);
    query.bindValue(":nbFiles", nbFiles);
    query.bindValue(":state", state);

    qDebug() << tr("[MB_TRACE][insertPost] %1")
                        .arg(QString("<:date : %1><:nzbName : %2><:size: %3><:avgSpeed : %4><:archiveName: "
                                     "%5><:archivePass %6><:groups "
                                     "%7><:from : %8><:packingPath %9><:nzbFilePath %10><:state %11>")
                                     .arg(date)
                                     .arg(nzbName)
                                     .arg(size)
                                     .arg(avgSpeed)
                                     .arg(archiveName)
                                     .arg(archivePass)
                                     .arg(groups)
                                     .arg(from)
                                     .arg(packingPath)
                                     .arg(nzbFilePath)
                                     .arg(state ? "yes" : "no"));
    if (!query.exec())
    {
        NgLogger::error(tr("Error inserting post to history DB: %1  (query: %2) args: %3")
                                .arg(query.lastError().text())
                                .arg(query.lastQuery())
                                .arg(QString("<:date : %1><:nzbName : %2><:size: %3><:avgSpeed : "
                                             "%4><:archiveName: %5><:archivePass %6><:groups "
                                             "%7><:from : %8><:packingPath %9><:nzbFilePath %10><:state %11>")
                                             .arg(date)
                                             .arg(nzbName)
                                             .arg(size)
                                             .arg(avgSpeed)
                                             .arg(archiveName)
                                             .arg(archivePass)
                                             .arg(groups)
                                             .arg(from)
                                             .arg(packingPath)
                                             .arg(state ? "yes" : "no")));
        qApp->processEvents(); // As it happens at PostingJob destruction, make sure the log event is
                               // processed
        return 0;
    }

    //    int postId = query.lastInsertId().toInt();
    //    qDebug() << "Post ID for " << nzbName << " : " << postId;
    return query.lastInsertId().toInt();
}

QSqlQuery NgHistoryDatabase::_postingJobQuery(PostingJob const &job) const
{
    QSqlQuery query;
    query.prepare(DB::SQL::kInsertJob);
    query.bindValue(":date", NgTools::currentDateTime());
    query.bindValue(":nzbName", job.nzbName());
    query.bindValue(":size", job.postSize());
    query.bindValue(":avgSpeed", job.avgSpeed());
    query.bindValue(":files", "files1|foiles2");
    query.bindValue(":archiveName", job.hasCompressed() ? job.rarName() : QString());
    query.bindValue(":archivePass", job.hasCompressed() ? job.rarPass() : QString());
    query.bindValue(":groups", job.groups());
    query.bindValue(":from", job.from(false));
    query.bindValue(":packingPath", job.packingTmpPath());
    query.bindValue(":nzbFilePath", job.nzbFilePath());
    query.bindValue(":nbFiles", job.nbFiles());
    query.bindValue(":state", job.state());
    return query;
}

const QRegularExpression NgHistoryDatabase::kByteSizeRegExp = QRegularExpression("(\\d+\\.\\d{2}) ([kMG]?B)");
qint64                   NgHistoryDatabase::byteSize(QString const &humanSize)
{
    qint64                  bytes = 0;
    QRegularExpressionMatch match = kByteSizeRegExp.match(humanSize);
    if (match.hasMatch())
    {
        double  size = match.captured(1).toDouble();
        QString unit = match.captured(2);
        if (unit == "GB")
            size *= 1024 * 1024 * 1024;
        else if (unit == "MB")
            size *= 1024 * 1024;
        else if (unit == "kB")
            size *= 1024;
        bytes = static_cast<long>(size);
    }
    return bytes;
}

qint64 NgHistoryDatabase::megaSize(QString const &humanSize)
{
    qint64                  megas = 0;
    QRegularExpressionMatch match = kByteSizeRegExp.match(humanSize);
    if (match.hasMatch())
    {
        double  size = match.captured(1).toDouble();
        QString unit = match.captured(2);
        if (unit == "GB")
            size *= 1024;
        else if (unit.isEmpty() || unit == "kB")
            size = 0; // ignore bytes and kB
        megas = static_cast<long>(size);
    }
    return megas;
}

bool NgHistoryDatabase::_markUnfinishedJobDone(int const dbJobId, bool success)
{
    // Sqlite support transactions but we make sure...
    if (!transaction())
    {
        NgLogger::criticalError(tr("Error DB not supporting transactions... dbJobId: %1").arg(dbJobId),
                                NgError::ERR_CODE::DB_ERR_UPDATE_UNFINISHED);
        return false;
    }

    QSqlQuery query;
    query.prepare(DB::SQL::kSqlUpdateHistoryDoneUnfinishedStatement);
    query.bindValue(":value", success ? PostingJob::JOB_STATE::POSTED : PostingJob::JOB_STATE::ERROR_RESUMING);
    query.bindValue(":job_id", dbJobId);
    if (!query.exec())
    {
        NgLogger::criticalError(
                tr("Error DB updating tHistory... dbJobId: %1 (%2)").arg(dbJobId).arg(query.lastError().text()),
                NgError::ERR_CODE::DB_ERR_UPDATE_UNFINISHED);
        return false;
    }

    query.prepare(DB::SQL::kSqlDeleteUnfinisheFilesdStatement);
    query.bindValue(":job_id", dbJobId);
    if (query.exec())
        commit();
    else
    {
        rollback();
        NgLogger::criticalError(tr("Error DB updating tUnfinishedFiles... dbJobId: %1 (%2)")
                                        .arg(dbJobId)
                                        .arg(query.lastError().text()),
                                NgError::ERR_CODE::DB_ERR_UPDATE_UNFINISHED);
        return false;
    }
    return true;
}

bool NgHistoryDatabase::initSQLite(QString const &dbPath)
{
    return Database::initSQLite(dbPath, DB::SQL::kPragmas);
}

UnfinishedJobs NgHistoryDatabase::unfinishedJobs()
{
    qDebug() << "[MB_TRACE][Database::unfinishedJobs] >>>>>>>>>>>>";
    UnfinishedJobs jobs;
    QSqlQuery      query;
    query.prepare(DB::SQL::kSqlSelectUnfinishedStatement);
    query.bindValue(":posted_sate", PostingJob::JOB_STATE::POSTED);
    if (!query.exec())
    {
        NgLogger::criticalError(
                tr("Error DB trying to get the unfinished post... (%1)").arg(query.lastError().text()),
                NgError::ERR_CODE::DB_ERR_SELECT_UNFINISHED);
        qDebug() << "[MB_TRACE][Database::unfinishedJobs] <<<<<<<<<< s1";
        return UnfinishedJobs();
    }

    while (query.next())
    {
        qDebug() << "[MB_TRACE][Database::unfinishedJobs]  " << query.value(4).toString() << " ?";
        UnfinishedJob uJob = { query.value(0).toInt(),    query.value(1).toDateTime(),
                               query.value(2).toString(), query.value(3).toString(),
                               query.value(4).toString(), query.value(5).toLongLong(),
                               query.value(6).toString(), query.value(7).toInt() };
        jobs.push_back(uJob);
    }

    qDebug() << "[MB_TRACE][Database::unfinishedJobs] <<<<<<<<<< nbJobs: " << jobs.size();

    return jobs;
}

QStringList NgHistoryDatabase::missingFiles(int jobId)
{
    QStringList files;
    QSqlQuery   query;
    query.prepare(DB::SQL::kMissingFilesForJob);
    query.bindValue(":job_id", jobId);

    if (!query.exec())
    {
        NgLogger::error(tr("Error listing missing files for job_id: %1 (query: %2) args: %3")
                                .arg(jobId)
                                .arg(query.lastError().text())
                                .arg(query.lastQuery()));
        qApp->processEvents(); // As it happens at PostingJob destruction, make sure the log event is
                               // processed
        return QStringList();
    }

    while (query.next())
    {
        qDebug() << "[MB_TRACE][Database::missingFiles]  " << query.value(0).toString();
        files << query.value(0).toString();
    }

    return files;
}

QMultiMap<int, QString> NgHistoryDatabase::missingFiles()
{
    QMultiMap<int, QString> missingFilesPerJob;

    QSqlQuery query;
    if (!query.exec("select * from tUnfinishedFiles"))
    {
        NgLogger::criticalError(tr("Error DB trying to get the unposted files..."),
                                NgError::ERR_CODE::DB_ERR_SELECT_UNFINISHED);
        qDebug() << "[MB_TRACE][Database::missingFiles] <<<<<<<<<< s1";
        return missingFilesPerJob;
    }

    while (query.next())
    {
        qDebug() << "[MB_TRACE][Database::missingFiles]  " << query.value(1).toString();
        missingFilesPerJob.insert(query.value(0).toInt(), query.value(1).toString());
    }

    qDebug() << "[MB_TRACE][Database::missingFiles] <<<<<<<<<< nbMissing: " << missingFilesPerJob.size();

    return missingFilesPerJob;
}
