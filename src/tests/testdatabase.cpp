#include "testdatabase.h"
#include <QtTest/QtTest>

#include <QDebug>
#include <QFileInfoList>
#include <QSqlQuery>

#include "NgDBConf.h"
#include "NgHistoryDatabase.h"
#include "NgPost.h"

TestNgHistoryDatabase::TestNgHistoryDatabase(QString const &testName, int argc, char *argv[])
    : MacroTest(testName)
{
    qDebug() << "Construction TestNgHistoryDatabase => _ngPost...";
    // _ngPost = new NgPost(argc, argv);
}

void TestNgHistoryDatabase::init()
{
    _deletekDBTestFile();
    _db = new NgHistoryDatabase;
    MB_VERIFY(_db->initSQLite(kDBTestFilePath), this);
}

void TestNgHistoryDatabase::cleanup() { delete _db; }

void TestNgHistoryDatabase::onTestInitDB()
{
    MB_VERIFY(_db->initSQLite(kDBTestFilePath), this);
    QFileInfo fileInfo(kDBTestFilePath);
    MB_VERIFY(fileInfo.exists() && fileInfo.isWritable(), this);
}

// MB_TODO: MOVE IN Database switch on type. code here for sqlite only
void TestNgHistoryDatabase::onListTablesAndRows() { MB_VERIFY(_db->dumpTablesNamesAndRows(), this); }

void TestNgHistoryDatabase::onTestUnfinishedBasicCase()
{
    _doInsertsIfProvided("://db/resume_test/db_inserts.sql");
    UnfinishedJobs jobsToPost = _db->unfinishedJobs();
    MB_VERIFY(jobsToPost.size() == 1, this);

    MB_VERIFY(_db->missingFiles().size() == 3, this);
    UnfinishedJob const &job = jobsToPost.front();
    MB_VERIFY(job.jobIdDB == 1, this);

    QStringList missingFiles = _db->missingFiles(job.jobIdDB);
    MB_VERIFY(missingFiles.size() == 3, this);
}
