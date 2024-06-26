#include "NgHistoryDatabase.h"

#include <QDebug>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>

#include "NgDBConf.h"
#include "utils/NgLogger.h"
#include "utils/NgTools.h"

#include "PostingJob.h"

using namespace NgConf;
NgHistoryDatabase::NgHistoryDatabase() { }

NgHistoryDatabase::Stats NgHistoryDatabase::statistics(DATE_CONDITION since) const
{
    QString req(DB::SQL::kSelectStatistics);
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
    if (!query.exec(req) || !query.next())
    {
        NgLogger::error(tr("Error executing statistics query : %1  (query: %2)")
                                .arg(query.lastError().text())
                                .arg(query.lastQuery()));
        return Stats(since, "", 0, 0., 0., 0.);
    }
    return Stats(since,
                 query.value(0).toString(),
                 query.value(1).toUInt(),
                 query.value(2).toDouble(),
                 query.value(3).toDouble(),
                 query.value(4).toDouble());
}

void NgHistoryDatabase::dumpStatistics() const
{
    NgLogger::log(tr("NgPost posting statistics:"), true);
    NgLogger::log(tr("\t- %1").arg(statistics(DATE_CONDITION::ALL).str()), true);
    NgLogger::log(tr("\t- %1").arg(statistics(DATE_CONDITION::YEAR).str()), true);
    NgLogger::log(tr("\t- %1").arg(statistics(DATE_CONDITION::MONTH).str()), true);
    NgLogger::log(tr("\t- %1").arg(statistics(DATE_CONDITION::WEEK).str()), true);
    NgLogger::log(tr("\t- %1").arg(statistics(DATE_CONDITION::DAY).str()), true);
}

QString NgHistoryDatabase::Stats::str() const
{
    return tr("%1: number of posts: %2, total size: %3, avg size: %4, avg speed: %5")
            .arg((since == DATE_CONDITION::ALL) ? tr("total from the %1").arg(firstDate)
                                                : Database::dateCondition(since))
            .arg(nbPosts)
            .arg(NgTools::humanSizeFromMB(sumSizeMB))
            .arg(NgTools::humanSizeFromMB(avgSizeMB))
            .arg(NgTools::humanSpeedFromKbps(avgSpeedKbps));
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
    int dbJobId = jobQuery.lastInsertId().toInt();
    jobQuery.finish();

    auto const missingFiles = job.unpostedFilesPath();
    qDebug() << QString("[MB_TRACE][Database::insertPostingJob][%1] dbJobId = %2 has %3 missing files")
                        .arg(job.nzbName())
                        .arg(dbJobId)
                        .arg(missingFiles.size());
    if (missingFiles.isEmpty())
        return dbJobId; // No missing files

    NgLogger::log(tr("%1 has %2 unposted file(s)").arg(job.nzbName()).arg(missingFiles.size()), true);
    for (QString const &filePath : missingFiles)
    {
        QSqlQuery fileQuery;
        fileQuery.prepare(DB::SQL::kInsertUnfinishedStatement);
        fileQuery.bindValue(":job_id", dbJobId);
        fileQuery.bindValue(":filePath", filePath);
        if (!fileQuery.exec())
        {
            NgLogger::error(tr("Error saving path of unposted file '%1' for post %2 : %3  (query: %4) with "
                               "dbJobId: %5")
                                    .arg(filePath)
                                    .arg(job.nzbName())
                                    .arg(fileQuery.lastError().text())
                                    .arg(fileQuery.lastQuery())
                                    .arg(dbJobId));
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
                                   double         sizeMB,
                                   double         avgSpeedKbps,
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
    query.bindValue(":sizeMB", sizeMB);
    query.bindValue(":avgSpeedKbps", avgSpeedKbps);
    query.bindValue(":archiveName", archiveName);
    query.bindValue(":archivePass", archivePass);
    query.bindValue(":groups", groups);
    query.bindValue(":from", from);
    query.bindValue(":packingPath", packingPath);
    query.bindValue(":nzbFilePath", nzbFilePath);
    query.bindValue(":nbFiles", nbFiles);
    query.bindValue(":state", state);

    qDebug() << tr("[MB_TRACE][insertPost] %1")
                        .arg(QString("<:date : %1><:nzbName : %2><:sizeMB : %3><:avgSpeedKbps : "
                                     "%4><:archiveName: "
                                     "%5><:archivePass %6><:groups "
                                     "%7><:from : %8><:packingPath %9><:nzbFilePath %10><:state %11>")
                                     .arg(date)
                                     .arg(nzbName)
                                     .arg(sizeMB)
                                     .arg(avgSpeedKbps)
                                     .arg(archiveName)
                                     .arg(archivePass)
                                     .arg(groups)
                                     .arg(from)
                                     .arg(packingPath)
                                     .arg(nzbFilePath)
                                     .arg(PostingJob::state(state)));
    if (!query.exec())
    {
        NgLogger::error(tr("Error inserting post to history DB: %1  (query: %2) args: %3")
                                .arg(query.lastError().text())
                                .arg(query.lastQuery())
                                .arg(QString("<:date : %1><:nzbName : %2><:sizeMB: %3><:avgSpeedKbps : "
                                             "%4><:archiveName: %5><:archivePass %6><:groups "
                                             "%7><:from : %8><:packingPath %9><:nzbFilePath %10><:state %11>")
                                             .arg(date)
                                             .arg(nzbName)
                                             .arg(sizeMB)
                                             .arg(avgSpeedKbps)
                                             .arg(archiveName)
                                             .arg(archivePass)
                                             .arg(groups)
                                             .arg(from)
                                             .arg(packingPath)
                                             .arg(PostingJob::state(state))));
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
    query.bindValue(":sizeMB", job.postSizeInMB());
    query.bindValue(":avgSpeedKbps", job.avgSpeedKbps());
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
                               query.value(6).toString(), static_cast<ushort>(query.value(7).toInt()),
                               query.value(8).toString(), query.value(9).toString() };
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
