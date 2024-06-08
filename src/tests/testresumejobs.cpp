#include "testresumejobs.h"
#include "utils/Macros.h"
#include <QtTest/QtTest>

#include <QDebug>
#include <QFileInfoList>
#include <QSqlQuery>

#include "NgDBConf.h"
#include "NgHistoryDatabase.h"
#include "NgPost.h"
#include "ResumeJobQueue.h"

TestResumeJobs::TestResumeJobs(QString const &testName, int argc, char *argv[]) : MacroTest(testName)
{
    qDebug() << "Construction TestResumeJobs => _ngPost...";
    _ngPost = new NgPost(argc, argv);
}

void TestResumeJobs::init()
{
    _deletekDBTestFile();
    _db = new NgHistoryDatabase;
    MB_VERIFY(_db->initSQLite(kDBTestFilePath), this);
}

void TestResumeJobs::cleanup() { delete _db; }

void TestResumeJobs::onTestUnpostedBasicCase()
{
    _doInsertsIfProvided("://db/resume_test/db_inserts.sql");
    UnfinishedJobs jobsToPost = _db->unfinishedJobs();
    MB_VERIFY(jobsToPost.size() == 1, this);

    MB_VERIFY(_db->missingFiles().size() == 3, this);
    UnfinishedJob const &job = jobsToPost.front();
    MB_VERIFY(job.jobIdDB == 1, this);

    QStringList missingFiles = _db->missingFiles(job.jobIdDB);
    MB_VERIFY(missingFiles.size() == 3, this);

    // Deploy nzb to it can be modified! in /tmp/<nzbName>
    const QString kNzbResourcePath = "://db/resume_test/nzb/file1.nzb";
    const QString kNzbWritablePath = "/tmp/expected_file1.nzb";
    if (QFileInfo(kNzbWritablePath).exists())
    {
        qDebug() << "[MB_TRACE] [TestResumeJobs] rm " << kNzbWritablePath;
        MB_VERIFY(QFile::remove(kNzbWritablePath), this);
    }
    bool isNzbCopied = _copyResourceFile(kNzbResourcePath, kNzbWritablePath);
    MB_VERIFY(isNzbCopied, this);

    PostingJob *resumeJob = ResumeJobQueue::getPostingJobFirstUnfinishedJob(*_ngPost, kNzbWritablePath);
    MB_VERIFY(resumeJob != nullptr, this);

    // MB_TODO: needs to run in an handler with a thread...
    // with a wait condition
    // PostingJobHandler like ConnectionHandler!
}
