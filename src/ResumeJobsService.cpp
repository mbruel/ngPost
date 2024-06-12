//========================================================================
//
// Copyright (C) 2024 Matthieu Bruel <Matthieu.Bruel@gmail.com>
// This file is a part of ngPost : https://github.com/mbruel/ngPost
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3..
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>
//
//========================================================================

#include "ResumeJobsService.h"
#include <QCoreApplication>
#include <QDebug> //MB_TODO: use NgLogger instead!
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QXmlStreamReader>

#include "NgDBConf.h"
#include "NgHistoryDatabase.h"
#include "NgPost.h"
#include "PostingJob.h"
#include "utils/NgLogger.h"
#include "utils/NgTools.h"

#ifdef __test_ngPost__
PostingJob *ResumeJobsService::getPostingJobFirstUnfinishedJob(NgPost        &ngPost,
                                                               QString const &nzbWritableFilePath)
{
    NgHistoryDatabase *db                  = ngPost.historyDatabase();
    UnfinishedJobs     jobsToPost          = db->unfinishedJobs();
    auto               missingFilesPerJobs = db->missingFiles(); // all in once (1 sql req)

    // Test we do only the first one!
    auto &unfinishedJob       = jobsToPost.front();
    unfinishedJob.nzbFilePath = nzbWritableFilePath; // modify the nzb path!

    if (!unfinishedJob.couldBeResumed())
    {
        NgLogger::error(tr("couldn't resume unfinished job: %1").arg(unfinishedJob.nzbName));
        return nullptr;
    }
    QStringList   missingFilesInDB  = missingFilesPerJobs.values(unfinishedJob.jobIdDB);
    QStringList   postedFiles       = _postedFilesFromNzb(unfinishedJob.nzbFilePath);
    QFileInfoList filesInPackingDir = _filesInPackingPath(unfinishedJob.packingPath);
    int           nbTotalFiles      = filesInPackingDir.size();

    if (!_doFilesChecks(unfinishedJob, missingFilesInDB, postedFiles, filesInPackingDir))
        return nullptr;

    PostingJob *pendingJob = _jobsToResume(ngPost, unfinishedJob, filesInPackingDir, nbTotalFiles);

    NgLogger::log(tr("Job to resume job: %1").arg(unfinishedJob.nzbName), true);
    return pendingJob;
}
#endif

uint ResumeJobsService::resumeUnfinihedJobs(NgPost &ngPost)
{
    NgHistoryDatabase *db = ngPost.historyDatabase();
    if (!ngPost.initHistoryDatabase())
        return 0;
    UnfinishedJobs jobsToPost          = db->unfinishedJobs();
    auto           missingFilesPerJobs = db->missingFiles(); // all in once (1 sql req)
    ushort         nbPendingJobs       = 0;
    _logNbUnfinshedJobs(jobsToPost.size());

    for (auto const &unfinishedJob : jobsToPost)
    {
        if (!unfinishedJob.packingDone())
        {
            NgLogger::error(tr("NOT IMPLEMENTED YET resume unfinished job: %1 with state: %2 (!= PACKING_DONE)")
                                    .arg(unfinishedJob.nzbName)
                                    .arg(unfinishedJob.state));
            // MB_TODO:
            continue;
        }

        // 1.: can it be resumed ?
        if (!unfinishedJob.couldBeResumed())
        {
            NgLogger::error(tr("couldn't resume unfinished job: %1").arg(unfinishedJob.nzbName));
            continue;
        }
        QStringList   missingFilesInDB  = missingFilesPerJobs.values(unfinishedJob.jobIdDB);
        QStringList   postedFiles       = _postedFilesFromNzb(unfinishedJob.nzbFilePath);
        QFileInfoList filesInPackingDir = _filesInPackingPath(unfinishedJob.packingPath);
        int           nbTotalFiles      = filesInPackingDir.size();

        if (unfinishedJob.packingDone()
            && !_doFilesChecks(unfinishedJob, missingFilesInDB, postedFiles, filesInPackingDir))
            continue;

        PostingJob *pendingJob = _jobsToResume(ngPost, unfinishedJob, filesInPackingDir, nbTotalFiles);

        // MB_TODO ask if we want to resume...
        if (pendingJob)
        {
            ++nbPendingJobs;
            NgLogger::log(tr("Resuming job: %1").arg(unfinishedJob.nzbName), true);
            ngPost.startPostingJob(pendingJob);
        }
    }

    return nbPendingJobs;
}

void ResumeJobsService::checkForUnfinihedJobs(NgPost &ngPost)
{
    if (!ngPost.initHistoryDatabase())
        return;

    QSqlQuery query;
    query.prepare(DB::SQL::kSqlNumberUnfinishedStatement);
    query.bindValue(":posted_sate", PostingJob::JOB_STATE::POSTED);
    if (!query.exec())
    {
        NgLogger::criticalError(
                tr("Error DB trying to get the number unfinished post... (%1)").arg(query.lastError().text()),
                NgError::ERR_CODE::DB_ERR_SELECT_UNFINISHED);
        return;
    }
    int nbRow = 0;
    if (query.next())
        nbRow = query.value(0).toInt();

    _logNbUnfinshedJobs(nbRow);
}

void ResumeJobsService::_logNbUnfinshedJobs(int nbUnfinishedJobs)
{
    if (nbUnfinishedJobs == 0)
        NgLogger::log(tr("There are no unfinished post in the Database :)"), true);
    else
        NgLogger::log(tr("There are some unfinished post in the Database: %1").arg(nbUnfinishedJobs), true);
}

QStringList ResumeJobsService::_postedFilesFromNzb(QString const &nzbPath)
{
    QFile xmlFile(nzbPath);
    if (!xmlFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        NgLogger::error(QString("Can't open nzb file: %1").arg(nzbPath));
        return QStringList();
    }
    QXmlStreamReader xmlReader(&xmlFile);

    static const QRegularExpression sSubjectRegExp(".*\"(.*)\".*");
    QStringList                     postedFileNames;
    while (!xmlReader.atEnd() && !xmlReader.hasError())
    {
        // Read next element
        QXmlStreamReader::TokenType token = xmlReader.readNext();

        // If token is StartElement - read it
        if (token == QXmlStreamReader::StartElement)
        {
            if (xmlReader.name() == QStringLiteral("file"))
            {
                QStringView subject = xmlReader.attributes().value(QStringLiteral("subject"));
                //                qDebug() << "[ResumeJobsService::postedFilesFromNzb] has file with subject: "
                //                << subject;
                QRegularExpressionMatch match = sSubjectRegExp.match(NgTools::xml2txt(subject.toString()));
                if (match.hasMatch())
                    postedFileNames << NgTools::xml2txt(match.captured(1));
            }
        }
    }

    if (xmlReader.hasError())
    {
        qDebug() << "[postedFilesFromNzb] Error parsing nzb " << nzbPath << " : " << xmlReader.errorString();
        return postedFileNames;
    }

    qDebug() << "[postedFilesFromNzb] postedFileNames: " << postedFileNames;

    // close reader and flush file
    xmlReader.clear();
    xmlFile.close();

    return postedFileNames;
}

PostingJob *ResumeJobsService::_jobsToResume(NgPost              &ngPost,
                                             UnfinishedJob const &unfinshedJob,
                                             QFileInfoList const &missingFiles,
                                             int                  nbTotalFiles)
{
    if (!unfinshedJob.couldBeResumed())
        return nullptr; // tested before normally...

    PostingJob *job = new PostingJob(ngPost, unfinshedJob, missingFiles);
    job->setParamForResume(unfinshedJob.jobIdDB, unfinshedJob.state, nbTotalFiles, missingFiles.size());
    return job;
}

QFileInfoList ResumeJobsService::_filesInPackingPath(QString const &packingDirPath)
{
    QFileInfoList listFiles;
    qDebug() << "[MB_TRACE][ResumeJobsService::filesInPackingPath] " << packingDirPath;
    QDir dir(packingDirPath);
    return dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
}
#include <QDir>
bool ResumeJobsService::_doFilesChecks(UnfinishedJob const &unfinishedJob,
                                       QStringList const   &missingFilesInDB,
                                       QStringList const   &postedFiles,
                                       QFileInfoList       &filesInPackingDir)
{
    qDebug() << "[MB_TRACE][ResumeJobsService::_doFilesChecks] missingFilesInDB: " << missingFilesInDB
             << ", postedFiles: " << postedFiles;

    // 1.: check the good numbers of missing files
    if (unfinishedJob.nzbStarted() && filesInPackingDir.size() != postedFiles.size() + missingFilesInDB.size())
    {
        NgLogger::error(tr("couldn't resume unfinished job: %1 (issue with numbers of files to post)")
                                .arg(unfinishedJob.nzbName));
        return false;
    }

    if (missingFilesInDB.isEmpty()) // list coming from DB once
        return true;

    // 2.: iterate the files in packing pack and remove the posted once
    // (also checked it is in DB (robustness))
    auto itFiles = filesInPackingDir.begin();
    while (itFiles != filesInPackingDir.end())
    {
        QString name = itFiles->fileName();
        if (postedFiles.contains(name))
        {
            itFiles = filesInPackingDir.erase(itFiles); // increment itFiles
            continue;
        }
        QString postedFilePath = QFileInfo(QDir(unfinishedJob.packingPath), name).absoluteFilePath();
        if (!missingFilesInDB.contains(postedFilePath))
        {
            NgLogger::error(tr("couldn't resume unfinished job: %1 (unexpected file : %2)")
                                    .arg(unfinishedJob.nzbName)
                                    .arg(postedFilePath));
        }
        ++itFiles;
    }

    // 3.: are all the missingFilesInDB present in filesInPackingDir  ?
    if (filesInPackingDir.size() != missingFilesInDB.size())
    {
        NgLogger::error(tr("couldn't resume unfinished job: %1 (unexpected number of missing files)")
                                .arg(unfinishedJob.nzbName));
        return false;
    }
    return true;
}

bool UnfinishedJob::couldBeResumed() const
{
    if (!nzbStarted())
        return true; // we can't be sure of anything, even the existance of the nzb

    // first check if we can read/write the nzb
    QFileInfo fiNzb(nzbFilePath);
    if (!fiNzb.exists() || !fiNzb.isFile() || !fiNzb.isReadable() || !fiNzb.isWritable())
    {
        NgLogger::error(tr("can't open nzb file for resume: %1").arg(nzbFilePath));
        return false;
    }

    // MB_TODO this test can be done only once not for all files of a same job!
    QFileInfo fiPackingPath(packingPath);
    if (!fiPackingPath.exists() || !fiPackingPath.isDir())
    {
        NgLogger::error(tr("packing Path not available: %1").arg(packingPath));
        return false;
    }

    if (QDir(packingPath).isEmpty())
    {
        NgLogger::error(tr("packing Path is empty: %1").arg(packingPath));
        return false;
    }
    return true;
}

UnfinishedJob::UnfinishedJob(qint64           jobId,
                             QDateTime const &dt,
                             QString const   &tmp,
                             QString const   &nzbFile,
                             QString const   &nzbName,
                             qint64           s,
                             QString const   &grps,
                             ushort           state_)
    : jobIdDB(jobId)
    , date(dt)
    , packingPath(tmp)
    , nzbFilePath(nzbFile)
    , nzbName(nzbName)
    , size(s)
    , groups(grps)
    , state(state_)
{
}

bool UnfinishedJob::hasEmptyPackingPath() const
{
    QFileInfo fi(packingPath);
    if (!fi.exists() || !fi.isDir() || QDir(packingPath).isEmpty())
        return false;
    return true;
}

bool UnfinishedJob::packingDone() const { return state >= PostingJob::JOB_STATE::PACKING_DONE; }

bool UnfinishedJob::nzbStarted() const { return state >= PostingJob::JOB_STATE::NZB_CREATED; }
