#ifndef NGHISTORYDATABASE_H
#define NGHISTORYDATABASE_H
#include "NgDBConf.h"
#include "utils/Database.h"

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
    static const QRegularExpression kByteSizeRegExp;

public:
    NgHistoryDatabase();

    int insertPostingJob(PostingJob const &job);

    bool initSQLite(QString const &dbPath);

    qint64  postedSizeInMB(DATE_CONDITION since) const;
    QString postedSize(DATE_CONDITION since) const;

    static qint64 byteSize(QString const &humanSize);
    static qint64 megaSize(QString const &humanSize);

private:
    bool _markUnfinishedJobDone(int const dbJobId, bool success);
    int  _insertPost(QString const &date,
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
                     int            done);

    QSqlQuery _postingJobQuery(PostingJob const &job) const;

    UnfinishedJobs          unfinishedJobs();
    QMultiMap<int, QString> missingFiles();
    QStringList             missingFiles(int jobId);
};

#endif // NGHISTORYDATABASE_H
