#include "testdatabase.h"
#include <QtTest/QtTest>

#include <QDebug>
#include <QFileInfoList>
#include <QSqlQuery>

#include "NgDBConf.h"
#include "NgHistoryDatabase.h"
#include "NgPost.h"

TestNgHistoryDatabase::TestNgHistoryDatabase(QString const &testName, int argc, char *argv[]) : MacroTest(testName)
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
void TestNgHistoryDatabase::onListTablesAndRows()
{
    //     we create the DB and the inserts (dump) and use a .sql
    //                                                    that prepare a tmp db in tmp ;)
    // => on est sûr d'avoir nos putain de rawss!
    // const QString kPathDBTestUnpostedBasicCase = ":/db/resume_test/ngPost.sqlite";
    // MB_VERIFY(_db.initSQLite("~/ngPost.sqlite"), this);
    // MB_VERIFY(_db.isOpen(), this);

    // // Get the number of tables
    // QSqlQuery tables_query(_db.qSqlDatabase());
    // tables_query.exec("SELECT name FROM sqlite_master WHERE type='table';");

    // while (tables_query.next())
    // {
    //     QString table_name = tables_query.value(0).toString();
    //     qDebug() << "Table: " << table_name;

    //     // Get the number of rows in the table
    //     QSqlQuery rows_query;
    //     rows_query.prepare(QString("SELECT COUNT(*) FROM %1;").arg(table_name));
    //     rows_query.exec();

    //     if (rows_query.next())
    //     {
    //         int row_count = rows_query.value(0).toInt();
    //         qDebug() << "  Rows: " << row_count;
    //     }
    // }
}

void TestNgHistoryDatabase::onTestUnpostedBasicCase()
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

/*
 * Destuction NgPost...
Finishing posting...

[23:28:00.136] Upload size: 9.00 MB in 00:00:18.199 (18 sec)                  => average speed: 506.49 kB/s
(1 connections on 2 threads) [23:28:00.136] nzb file: /home/mb/Downloads/nzb/file1.nzb [23:28:00.136] file:
file1.nzb, rar name: file1, rar pass: Pl_1%#gR2%K]qSR-z^a1WfgyD

[Poster #0] threads: BuilderThread #0, NntpThread #0 are stopped (no more event queue)
[NntpThread #0 {NntpCon #1}] Killing connection..
[NntpThread #0 {NntpCon #1}] Destructing connection..
[Poster #0] Connection thread is done: 1
[Poster #0] all threads done: 1
[PostingJob] <<<< Destruction  "0x00005560ea1b5020"  in thread :  "MainThread"
"[MB_TRACE][Database::insertPostingJob][file1.nzb] >>>>>>>>>>>"
"[MB_TRACE][Database::insertPostingJob][file1.nzb] dbJobId = 1"
"[MB_TRACE][Database::insertPostingJob][file1.nzb] <<<<<<<<<<< s4"
All posters stopped...
All connections are closed...
ERROR: there were 4 on 8 that havn't been posted:
  - [0 ok / 5] /tmp/file1/file1.part4.rar (fInProgress missing Articles: 2 5 4 1 3 )
  - [0 ok / 3] /tmp/file1/file1.part5.rar (fToUpload missing Articles: 2 1 3 )
  - [0 ok / 2] /tmp/file1/file1.vol00+01.par2 (fToUpload missing Articles: 2 1 )
  - [0 ok / 3] /tmp/file1/file1.vol01+02.par2 (fToUpload missing Articles: 2 1 3 )
you can try to repost only those and concatenate the nzb with the current one ;)
[MB_TRACE][Issue#82][NgPost::onPostingJobFinished] job:  PostingJob(0x5560ea1b5020) , file:  "file1.nzb"
[PostingJob::getFilesPaths] "/home/mb/Downloads/testNgPost/file1.bin"
[PostingJob::getFilesPaths] "/home/mb/Downloads/testNgPost/file2.bin"
[MB_TRACE][onPostingJobFinished]getFilesPaths:
"/home/mb/Downloads/testNgPost/file1.bin|/home/mb/Downloads/testNgPost/file2.bin"
[MB_TRACE][Issue#82][NgPost::onPostingJobFinished] job:  PostingJob(0x5560ea1b5020) , file:  "file1.nzb"
Destructing PostingJob 0x00005560ea1b5020
Deleting PostingJob
Destruction NNTP::File # 5  :  "file1.part4.rar"  ( 5  articles)
     - Destruction NNTP::Article # 1
     - Destruction NNTP::Article # 2
     - Destruction NNTP::Article # 3
     - Destruction NNTP::Article # 4
Destruction NNTP::File # 6  :  "file1.part5.rar"  ( 3  articles)
Destruction NNTP::File # 7  :  "file1.vol00+01.par2"  ( 2  articles)
Destruction NNTP::File # 8  :  "file1.vol01+02.par2"  ( 3  articles)
Destruction Poster # 0
cmd:  "echo" , args:  QList("NZB_POST_CMD echo: <release: file1> <rarName: file1> <rarPass:
Pl_1%#gR2%K]qSR-z^a1WfgyD> <path: /home/mb/Downloads/nzb/file1.nzb> <groups: alt.binaries.paxer> <size:
9438836B> <nbFiles: 8> <nbArticles: 29> <failed: 0>") NZB_POST_CMD echo: <release: file1> <rarName: file1>
<rarPass: Pl_1%#gR2%K]qSR-z^a1WfgyD> <path: /home/mb/Downloads/nzb/file1.nzb> <groups: alt.binaries.paxer>
<size: 9438836B> <nbFiles: 8> <nbArticles: 29> <failed: 0> [MB_TRACE][MainParams] destroyed... is it on
purpose? (QSharedData)
*/

bool TestNgHistoryDatabase::_doInsertsIfProvided(QString const &sqlScript)
{
    qDebug() << "_doInsertsIfProvided" << sqlScript;
    if (_db->_execSqlFile(sqlScript) != 0)
        return false;
    return true;
}
