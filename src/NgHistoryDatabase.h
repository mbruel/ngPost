#ifndef NGHISTORYDATABASE_H
#define NGHISTORYDATABASE_H

#include "utils/Database.h"
#include "utils/UnfinishedJob.h"

class PostingJob;

class NgHistoryDatabase : public Database
{
    friend class NgMigration;
    friend class ResumeJobsService;
#ifdef __test_ngPost__
    friend class MacroTest;
    friend class TestNgHistoryDatabase;
    friend class TestResumeJobs;
#endif

public:
    struct Stats
    {
        Stats(DATE_CONDITION c, QString const &d, uint n, double s, double aSize, double aSpeed)
            : since(c), firstDate(d), nbPosts(n), sumSizeMB(s), avgSizeMB(aSize), avgSpeedKbps(aSpeed)
        {
        }
        DATE_CONDITION since;
        QString        firstDate;
        uint           nbPosts;
        double         sumSizeMB;
        double         avgSizeMB;
        double         avgSpeedKbps;

        QString str() const;
    };
    NgHistoryDatabase();

    int insertPostingJob(PostingJob const &job);

    bool initSQLite(QString const &dbPath);

    Stats statistics(DATE_CONDITION since) const;

    void dumpStatistics() const;

private:
    bool _markUnfinishedJobDone(int const dbJobId, bool success);
    int  _insertPost(QString const &date,
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
                     int            state);

    QSqlQuery _postingJobQuery(PostingJob const &job) const;

    UnfinishedJobs          unfinishedJobs();
    QMultiMap<int, QString> missingFiles();
    QStringList             missingFiles(int jobId);
};

#endif // NGHISTORYDATABASE_H
