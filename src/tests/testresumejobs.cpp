#include "testresumejobs.h"
#include "utils/Macros.h"
#include "utils/PostingJobHandler.h"
#include "utils/TestUtils.h"
#include <QtTest/QtTest>

#include <QDebug>
#include <QFileInfoList>
#include <QSqlQuery>

#include "NgDBConf.h"
#include "NgHistoryDatabase.h"
#include "NgPost.h"
#include "ResumeJobsService.h"

TestResumeJobs::TestResumeJobs(QString const &testName, int argc, char *argv[]) : MacroTest(testName)
{
    qDebug() << "Construction TestResumeJobs => _ngPost...";
    _ngPost                 = new NgPost(argc, argv);
    _sleepAfterEachTestCase = true;
}

void TestResumeJobs::init()
{
    _deletekDBTestFile();
    _db = new NgHistoryDatabase;
    MB_VERIFY(_db->initSQLite(kDBTestFilePath), this);
}

void TestResumeJobs::cleanup() { delete _db; }

void TestResumeJobs::onTestResumeWhenJobStateNZB_CREATED()
{
    NgLogger::setDebug(NgLogger::DebugLevel::FullDebug);

    TestUtils::loadDefaultConf(*_ngPost);
    _doInsertsIfProvided("://db/resume_test/db_inserts.sql");
    UnfinishedJobs jobsToPost = _db->unfinishedJobs();
    MB_VERIFY(jobsToPost.size() == 1, this);

    UnfinishedJob const &job = jobsToPost.front();
    MB_VERIFY(job.jobIdDB == 1, this);

    QStringList missingFiles = _db->missingFiles(job.jobIdDB);
    MB_VERIFY(missingFiles.size() == 3, this);

    // Deploy nzb to it can be modified! in /tmp/<nzbName>
    const QString kNzbResourcePath = "://db/resume_test/nzb/file1.nzb";
    const QString kNzbWritablePath = "/tmp/resumed_file1.nzb";
    if (QFileInfo(kNzbWritablePath).exists())
    {
        qDebug() << "[MB_TRACE] [TestResumeJobs] rm " << kNzbWritablePath;
        MB_VERIFY(QFile::remove(kNzbWritablePath), this);
    }
    bool isNzbCopied = _copyResourceFile(kNzbResourcePath, kNzbWritablePath);
    MB_VERIFY(isNzbCopied, this);

    QStringList postedFiles = ResumeJobsService::_postedFilesFromNzb(kNzbWritablePath);
    MB_VERIFY(postedFiles.size() == 3, this);

    PostingJob *resumeJob = ResumeJobsService::getPostingJobFirstUnfinishedJob(*_ngPost, kNzbWritablePath);
    MB_VERIFY(resumeJob != nullptr, this);

    PostingJobHandler *jobHandler = new PostingJobHandler(resumeJob);
    jobHandler->start();
    bool jobOK = QTest::qWaitFor([&jobHandler]() { return jobHandler->isTestDone(); }, 120000); // 2min
    MB_VERIFY(jobOK == true, this);
    if (!jobOK)
    {
        qCritical() << "onTestUnfinishedBasicCase FAILED...";
        return;
    }
    postedFiles = ResumeJobsService::_postedFilesFromNzb(kNzbWritablePath);
    MB_VERIFY(postedFiles.size() == 6, this);
    // more checks with each name?...
}
