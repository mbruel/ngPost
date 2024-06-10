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
    _ngPost = new NgPost(argc, argv);
}

void TestResumeJobs::init()
{
    _deletekDBTestFile();
    _db = new NgHistoryDatabase;
    MB_VERIFY(_db->initSQLite(kDBTestFilePath), this);
}

void TestResumeJobs::cleanup() { delete _db; }

void TestResumeJobs::onTestUnfinishedBasicCase()
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
    bool jobOK = QTest::qWaitFor([&jobHandler]() { return jobHandler->isTestDone(); }, 120000); // 2min sec
    MB_VERIFY(jobOK == true, this);
    if (!jobOK)
    {
        qCritical() << "onTestUnfinishedBasicCase FAILED...";
        return;
    }

    // MB_TODO: Syncho issue on prepareArticlesInAdvance: Article queue
    // I suppose as all in same test thread of the PostingJobHandler
    // [Poster #0] [Poster #0] prepareArticlesInAdvance: Article queue size:  5
    // [Poster #0] [Poster #0] prepareArticlesInAdvance: Article queue size:  2

    /*
     * QDEBUG : TestResumeJobs::onTestUnfinishedBasicCase() Creation NNTP::File:
"://db/resume_test/file1.nzb/file1.part3.rar"  size:  293630  article size:  716800  => nbArticles:  1 QDEBUG :
TestResumeJobs::onTestUnfinishedBasicCase() Creation NNTP::File:
"://db/resume_test/file1.nzb/file1.vol00+01.par2"  size:  1049592  article size:  716800  => nbArticles:  2
QDEBUG : TestResumeJobs::onTestUnfinishedBasicCase() Creation NNTP::File:
"://db/resume_test/file1.nzb/file1.vol01+01.par2"  size:  1049592  article size:  716800  => nbArticles:  2
QDEBUG : TestResumeJobs::onTestUnfinishedBasicCase() [PostingJob::_postFiles] nbFiles:  4 , nbPosters:  1 ,
nbCons:  1  => nbCon per Poster:  1  (nbExtraCon:  0 ) QDEBUG : TestResumeJobs::onTestUnfinishedBasicCase()
Construction  ""  in thread:  "" QDEBUG : TestResumeJobs::onTestUnfinishedBasicCase() "[THREAD] Poster #0
created!" [Poster #0] [Poster #0] prepareArticlesInAdvance: Article queue size:  3 onPostingStarted...
[17:29:41.415] Start posting: resumed_file1.nzb
Creation NntpCon #1 with ssl
[17:29:41.415] Number of available Nntp Connections: 1
PostingJob::_prepareArticles
[Poster #0] finished processing file ://db/resume_test/file1.nzb/file1.vol00+01.par2
[Poster #0] starting processing file ://db/resume_test/file1.nzb/file1.vol01+01.par2
[Poster #0] we've read 716800 bytes from 0 (=> new pos: 716800)
[Poster #0] [Poster #0] prepareArticlesInAdvance: Article queue size:  4
[Poster #0] we've read 332792 bytes from 716800 (=> new pos: 1049592)
[Poster #0] [Poster #0] prepareArticlesInAdvance: Article queue size:  1
[Poster #0] finished processing file ://db/resume_test/file1.nzb/file1.vol01+01.par2
[Poster #0] starting processing file ://db/resume_test/file1.nzb/file1.part3.rar
[Poster #0] we've read 293630 bytes from 0 (=> new pos: 293630)
[Poster #0] [Poster #0] prepareArticlesInAdvance: Article queue size:  5
[Poster #0] finished processing file ://db/resume_test/file1.nzb/file1.part3.rar
[Poster #0] starting processing file ://db/resume_test/file1.nzb/file1.vol00+01.par2
[Poster #0] we've read 716800 bytes from 0 (=> new pos: 716800)
[Poster #0] [Poster #0] prepareArticlesInAdvance: Article queue size:  2
[Poster #0] we've read 332792 bytes from 716800 (=> new pos: 1049592)
[Poster #0] [Poster #0] prepareArticlesInAdvance: Article queue size:  6
[Poster #0] finished processing file ://db/resume_test/file1.nzb/file1.vol00+01.par2
[Poster #0] starting processing file ://db/resume_test/file1.nzb/file1.vol01+01.par2
[Poster #0] we've read 716800 bytes from 0 (=> new pos: 716800)
[Poster #0] [Poster #0] prepareArticlesInAdvance: Article queue size:  3
onPostingStarted...
QWARN  : TestResumeJobs::onTestUnfinishedBasicCase() qt.network.ssl: QSslSocket::startClientEncryption: cannot
start handshake on non-plain connection QWARN  : TestResumeJobs::onTestUnfinishedBasicCase() qt.network.ssl:
QSslSocket::startClientEncryption: cannot start handshake on non-plain connection [Poster #0] [Poster #0 {Poster
#0 {NntpCon #1}}] getNextArticle _articles.size() = 3 [Poster #0 {Poster #0 {NntpCon #1}}] start sending article:
[6/6] file1.vol01+01.par2 - Article #2/2 <id: cddc921d4d2e41ec915ea5c9d12114f7, nbTrySend: 0> [Poster #0] we've
read 332792 bytes from 716800 (=> new pos: 1049592) QDEBUG : TestResumeJobs::onTestUnfinishedBasicCase()
[NNTP::File::onArticlePosted]  "[6/6] file1.vol01+01.par2" : posted:  0  /  2  (nb FAILED:  0 )  article part  2
, id:  "cddc921d4d2e41ec915ea5c9d12114f7@ngPost" [Poster #0] [Poster #0 {Poster #0 {NntpCon #1}}] getNextArticle
_articles.size() = 3 [Poster #0 {Poster #0 {NntpCon #1}}] start sending article: [5/6] file1.vol00+01.par2 -
Article #1/2 <id: 02cdeca96131409397ff9772551dd43b, nbTrySend: 0> [Poster #0] finished processing file
://db/resume_test/file1.nzb/file1.vol01+01.par2 [Poster #0] No more file to post... [17:29:43.352][avg. speed:
167.78 kB/s] >>>>> [5/6] file1.vol00+01.par2 QDEBUG : TestResumeJobs::onTestUnfinishedBasicCase()
[NNTP::File::onArticlePosted]  "[5/6] file1.vol00+01.par2" : posted:  0  /  2  (nb FAILED:  0 )  article part  1
, id:  "02cdeca96131409397ff9772551dd43b@ngPost" [Poster #0] [Poster #0 {Poster #0 {NntpCon #1}}] getNextArticle
_articles.size() = 2 [Poster #0 {Poster #0 {NntpCon #1}}] start sending article: [6/6] file1.vol01+01.par2 -
Article #1/2 <id: c04a40e37b7e4721b8f4b3014e64d426, nbTrySend: 0> [Poster #0] No more file to post... QSYSTEM:
TestResumeJobs::onTestUnfinishedBasicCase() Signal caught, closing the application...

[17:29:44.505][avg. speed: 331.82 kB/s] >>>>> [6/6] file1.vol01+01.par2
17:29:44: /home/mb/Documents/github/ngPost/build-test_ngPost-Desktop_Qt_5_15_2_GCC_64bit-Debug/test_ngPost
crashed.
*/

    // MB_TODO: it should be:

    /*
     *
<rarName :  "" >
<rarPass :  "" >
<nzbFilePath :  "/home/mb/Downloads/nzb/file1.nzb" >

[PostingJob::getFilesPaths] "/tmp/file1.nzb/file1.part3.rar"
[PostingJob::getFilesPaths] "/tmp/file1.nzb/file1.vol00+01.par2"
[PostingJob::getFilesPaths] "/tmp/file1.nzb/file1.vol01+01.par2"
<files :  "/tmp/file1.nzb/file1.part3.rar|/tmp/file1.nzb/file1.vol00+01.par2|/tmp/file1.nzb/file1.vol01+01.par2"
>
<_grpList :  "alt.binaries.xylo" >
<_from :  "resumeJob@ngPost.com" >
<rarName :  QMap() >
[PostingParams::_dumpParams]<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<n
[MB_TRACE]filesToUpload:  QList(QFileInfo(/tmp/file1.nzb/file1.part3.rar),
QFileInfo(/tmp/file1.nzb/file1.vol00+01.par2), QFileInfo(/tmp/file1.nzb/file1.vol01+01.par2)) DB already
initialized [MB_TRACE][Issue#82][PostingJob::onStartPosting] job:  PostingJob(0x55c8fbcefdc0) , file: "file1.nzb"
(isActive:  true ) [MB_TRACE][PostingJob::_initPosting] nzbFilePath:  "/home/mb/Downloads/nzb/file1.nzb" ,
overwriteNzb:  false
"[MB_TRACE ]Creating QFile(/home/mb/Downloads/nzb/file1.nzb)"
[PostingParams::_dumpParams]>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

[MainParams::_dumpParams]>>>>>>>>>>>>>>>>>>
 nb Servers:  2 :  "[7con SSL on xsn_66598d1cc90f9:947716832@reader.xsnews.nl:443 enabled:0, nzbCheck:1] [1con
SSL on mbruel:tieumNG#1982@news.newshosting.com:443 enabled:1, nzbCheck:1] "

nbThreads:  8
inputDir:  "/home/mb/Downloads" , autoDelete:  false
packAuto:  false , packAutoKeywords: QList("compress", "gen_pass", "gen_par2") , autoClose:  false
, monitor delay:  1  ignore dir:  false  ext:  QList()
from:   , genFrom:  true , saveFrom:  false , groups:
"alt.binaries.xylo,alt.binaries.superman,alt.binaries.paxer"  policy:  "EACH_POST" articleSize:  716800 ,
obfuscate articles:  false , obfuscate file names:  false , _delFilesAfterPost:  false , _overwriteNzb:  false

compression settings: <tmp_path:  "/tmp" >  <ram_path:  ""  ratio:  1.1 > , <rar_path:  "/usr/bin/rar" > ,
<rar_size:  1 > <par2_pct:  5 > , <par2_path:  "/home/mb/apps/bin/parpar" > , <par2_pathCfg:
"/home/mb/apps/bin/parpar" > , <par2_args:  "-s1M -r1n*0.6 -m2048M -p1l -t7 --progress stdout -q" >  _use7z:
false

compress:  false , doPar2:  false , gen_name:  false , genPass:  false , lengthName:  33 , lengthPass:  25
 _urlNzbUploadStr: ""  _urlNzbUpload : no , _nzbPostCmd:  QList("echo \"NZB_POST_CMD echo: <release: __nzbName__>
<rarName: __rarName__> <rarPass: __rarPass__> <path: __nzbPath__> <groups: __groups__> <size: __sizeInByte__B>
<nbFiles: __nbFiles__> <nbArticles: __nbArticles__> <failed: __nbArticlesFailed__>\"") _preparePacking:  true

[MainParams::_dumpParams]<<<<<<<<<<<<<<<<<<

<rarName :  "" >
<rarPass :  "" >
<nzbFilePath :  "/home/mb/Downloads/nzb/file1.nzb" >

[PostingJob::getFilesPaths] "/tmp/file1.nzb/file1.part3.rar"
[PostingJob::getFilesPaths] "/tmp/file1.nzb/file1.vol00+01.par2"
[PostingJob::getFilesPaths] "/tmp/file1.nzb/file1.vol01+01.par2"
<files :  "/tmp/file1.nzb/file1.part3.rar|/tmp/file1.nzb/file1.vol00+01.par2|/tmp/file1.nzb/file1.vol01+01.par2"
>
<_grpList :  "alt.binaries.xylo" >
<_from :  "resumeJob@ngPost.com" >
<rarName :  QMap() >
[PostingParams::_dumpParams]<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<n
Creation NNTP::File:  "/tmp/file1.nzb/file1.part3.rar"  size:  293630  article size:  716800  => nbArticles:  1
Creation NNTP::File:  "/tmp/file1.nzb/file1.vol00+01.par2"  size:  1049592  article size:  716800  => nbArticles:
2 Creation NNTP::File:  "/tmp/file1.nzb/file1.vol01+01.par2"  size:  1049592  article size:  716800  =>
nbArticles:  2 [PostingJob::_postFiles] nbFiles:  3 , nbPosters:  1 , nbCons:  1  => nbCon per Poster:  1
(nbExtraCon:  0 ) Construction  ""  in thread:  "MainThread"
"[THREAD] ArtBuilder #0 owned by Poster #0 has moved to its thread BuilderThread #0"
"[THREAD] NntpCon #1 has been started, affected to Poster #0, moved to its thread NntpThread #0"
Using default config file: /home/mb/.ngPost
Group Policy EACH_POST: each post on a different group
Generate new random poster for each post
[2024-06-10 17:22:32] ngPost starts logging
Database path: /home/mb/ngPost.sqlite , already exist: yes
There are some unfinished post in the Database: 1
SSL support: yes, build version: OpenSSL 1.1.1k  FIPS 25 Mar 2021, system version: OpenSSL 1.1.1w  11 Sep 2023
Resuming job: file1.nzb
Resume mode: number of unfinished jobs: 1
[17:22:32.139] Start posting: file1.nzb
Creation NntpCon #1 with ssl
[17:22:32.139] Number of available Nntp Connections: 1
PostingJob::_prepareArticles
[BuilderThread #0] starting processing file /tmp/file1.nzb/file1.part3.rar
[BuilderThread #0] we've read 293630 bytes from 0 (=> new pos: 293630)
[Poster #0] [BuilderThread #0] prepareArticlesInAdvance: Article queue size:  1
[BuilderThread #0] finished processing file /tmp/file1.nzb/file1.part3.rar
[BuilderThread #0] starting processing file /tmp/file1.nzb/file1.vol00+01.par2
[BuilderThread #0] we've read 716800 bytes from 0 (=> new pos: 716800)
[Poster #0] [BuilderThread #0] prepareArticlesInAdvance: Article queue size:  2
[BuilderThread #0] we've read 332792 bytes from 716800 (=> new pos: 1049592)
[Poster #0] [BuilderThread #0] prepareArticlesInAdvance: Article queue size:  3
currentBuilt:  416  ( kVersio:  "4.16" , lastRelease from net :  416
[Poster #0] [Poster #0 {NntpCon #1}] getNextArticle _articles.size() = 3
[Poster #0 {NntpCon #1}] start sending article: [4/6] file1.part3.rar - Article #1/1 <id:
c42a405a179441559baa6acd19e2f00d, nbTrySend: 0> [BuilderThread #0] finished processing file
/tmp/file1.nzb/file1.vol00+01.par2 [BuilderThread #0] starting processing file /tmp/file1.nzb/file1.vol01+01.par2
[BuilderThread #0] we've read 716800 bytes from 0 (=> new pos: 716800)
[17:22:32.827][avg. speed:   0.00  B/s] >>>>> [4/6] file1.part3.rar
[Poster #0] [Poster #0 {NntpCon #1}] getNextArticle _articles.size() = 3
[Poster #0 {NntpCon #1}] start sending article: [5/6] file1.vol00+01.par2 - Article #1/2 <id:
6bb147db02c34ea2b4abc4a719aaceae, nbTrySend: 0> [NNTP::File::onArticlePosted]  "[4/6] file1.part3.rar" : posted:
0  /  1  (nb FAILED:  0 )  article part  1 , id:  "c42a405a179441559baa6acd19e2f00d@ngPost" [BuilderThread #0]
we've read 332792 bytes from 716800 (=> new pos: 1049592) [17:22:34.016][avg. speed: 152.85 kB/s] <<<<< [4/6]
file1.part3.rar Destruction NNTP::File # 4  :  "file1.part3.rar"  ( 1  articles)
         - Destruction NNTP::Article # 1
[17:22:34.118][avg. speed: 144.97 kB/s] >>>>> [5/6] file1.vol00+01.par2
[NNTP::File::onArticlePosted]  "[5/6] file1.vol00+01.par2" : posted:  0  /  2  (nb FAILED:  0 )  article part  1
, id:  "6bb147db02c34ea2b4abc4a719aaceae@ngPost" [Poster #0] [Poster #0 {NntpCon #1}] getNextArticle
_articles.size() = 3 [Poster #0 {NntpCon #1}] start sending article: [5/6] file1.vol00+01.par2 - Article #2/2
<id: bd2ee3a938b34078b6feb9395fed2e08, nbTrySend: 0> [BuilderThread #0] finished processing file
/tmp/file1.nzb/file1.vol01+01.par2 [BuilderThread #0] No more file to post... [NNTP::File::onArticlePosted]
"[5/6] file1.vol00+01.par2" : posted:  1  /  2  (nb FAILED:  0 )  article part  2 , id:
"bd2ee3a938b34078b6feb9395fed2e08@ngPost" [Poster #0] [Poster #0 {NntpCon #1}] getNextArticle _articles.size() =
2 [Poster #0 {NntpCon #1}] start sending article: [6/6] file1.vol01+01.par2 - Article #1/2 <id:
01378734460a485487e2df5094e05535, nbTrySend: 0> [BuilderThread #0] No more file to post... [17:22:36.176][avg.
speed: 325.01 kB/s] <<<<< [5/6] file1.vol00+01.par2 Destruction NNTP::File # 5  :  "file1.vol00+01.par2"  ( 2
articles)
         - Destruction NNTP::Article # 1
         - Destruction NNTP::Article # 2
[17:22:36.279][avg. speed: 316.92 kB/s] >>>>> [6/6] file1.vol01+01.par2
[Poster #0] [Poster #0 {NntpCon #1}] getNextArticle _articles.size() = 1
[NNTP::File::onArticlePosted]  "[6/6] file1.vol01+01.par2" : posted:  0  /  2  (nb FAILED:  0 )  article part  1
, id:  "01378734460a485487e2df5094e05535@ngPost" [Poster #0 {NntpCon #1}] start sending article: [6/6]
file1.vol01+01.par2 - Article #2/2 <id: cda9e3b8b01f482b9200fc93fbbc4174, nbTrySend: 0> [BuilderThread #0] No
more file to post... [Poster #0] [Poster #0 {NntpCon #1}] getNextArticle _articles.size() = 0 [Poster #0 {NntpCon
#1}] No more articles [Poster #0 {NntpCon #1}] Closing connection... [NNTP::File::onArticlePosted]  "[6/6]
file1.vol01+01.par2" : posted:  1  /  2  (nb FAILED:  0 )  article part  2 , id:
"cda9e3b8b01f482b9200fc93fbbc4174@ngPost" [MB_TRACE][PostingJob::_finishPosting] Destruction  "ArtBuilder #0" is
thread:  "BuilderThread #0" Destruction  "Poster #0 {NntpCon #1}"  from thread:  "NntpThread #0"
[17:22:38.287][avg. speed: 380.14 kB/s] <<<<< [6/6] file1.vol01+01.par2
All files have been posted => closing Job (nb article uploaded: 5, failed: 0)
Finishing posting...

[17:22:38.287] Resumed NZB file: /home/mb/Downloads/nzb/file1.nzb
[17:22:38.287] Upload size: 2.28 MB in 00:00:06.147 (6 sec)                  => average speed: 380.14 kB/s (1
connections on 2 threads)


[Poster #0] threads: BuilderThread #0, NntpThread #0 are stopped (no more event queue)
[Poster #0 {NntpCon #1}] Destructing connection..
[Poster #0] Connection thread is done: 1
[Poster #0] all threads done: 1
Destruction NNTP::File # 6  :  "file1.vol01+01.par2"  ( 2  articles)
         - Destruction NNTP::Article # 1
         - Destruction NNTP::Article # 2
All posters stopped...
All connections are closed...
[MB_TRACE][Issue#82][NgPost::onPostingJobFinished] job:  PostingJob(0x55c8fbcefdc0) , file:  "file1.nzb"
[PostingJob::getFilesPaths] "/tmp/file1.nzb/file1.part3.rar"
[PostingJob::getFilesPaths] "/tmp/file1.nzb/file1.vol00+01.par2"
[PostingJob::getFilesPaths] "/tmp/file1.nzb/file1.vol01+01.par2"
[MB_TRACE][onPostingJobFinished]getFilesPaths:
"/tmp/file1.nzb/file1.part3.rar|/tmp/file1.nzb/file1.vol00+01.par2|/tmp/file1.nzb/file1.vol01+01.par2"
[PostingJob] <<<< Destruction  "0x000055c8fbcefdc0"  in thread :  "MainThread"
"[MB_TRACE][Database::insertPostingJob][file1.nzb] >>>>>>>>>>>"
"[MB_TRACE][Database::insertPostingJob][file1.nzb] >>>>>>>>>>>"  resume  true
 => closing application
Destructing PostingJob 0x000055c8fbcefdc0
Deleting PostingJob
Destruction Poster # 0
cmd:  "echo" , args:  QList("NZB_POST_CMD echo: <release: file1> <rarName: > <rarPass: > <path:
/home/mb/Downloads/nzb/file1.nzb> <groups: alt.binaries.xylo> <size: 2392814B> <nbFiles: 6> <nbArticles: 5>
<failed: 0>") NZB_POST_CMD echo: <release: file1> <rarName: > <rarPass: > <path:
/home/mb/Downloads/nzb/file1.nzb> <groups: alt.binaries.xylo> <size: 2392814B> <nbFiles: 6> <nbArticles: 5>
<failed: 0> ngPost closed properly! [MB_TRACE][MainParams] destroyed... is it on purpose? (QSharedData:
"0x000055c8fbc38fa0" 17:22:38:
/home/mb/Documents/github/ngPost/build-ngPost-Desktop_Qt_6_4_0_GCC_64bit-Debug/ngPost exited with code 0
*/

    postedFiles = ResumeJobsService::_postedFilesFromNzb(kNzbWritablePath);
    MB_VERIFY(postedFiles.size() == 6, this);
    // more checks with each name?...
}
