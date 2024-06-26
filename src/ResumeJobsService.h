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
#ifndef RESUMEJOBSSERVICE_H
#define RESUMEJOBSSERVICE_H

#include "utils/PureStaticClass.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfoList>
#include <QStringList>
#include <QtGlobal>

class NgPost;
class PostingJob;
class UnfinishedJob;

class ResumeJobsService : public PureStaticClass
{

    Q_DECLARE_TR_FUNCTIONS(ResumeJobsService); // tr() without QObject using QCoreApplication::translate

#ifdef __test_ngPost__
    friend class TestResumeJobs;
    static PostingJob *getPostingJobFirstUnfinishedJob(NgPost &ngPost, QString const &nzbWritableFilePath);
#endif

public:
    static uint resumeUnfinihedJobs(NgPost &ngPost);
    static void checkForUnfinihedJobs(NgPost &ngPost);

private:
    static QStringList   _postedFilesFromNzb(QString const &nzbFile);
    static PostingJob   *_jobsToResume(NgPost              &ngPost,
                                       UnfinishedJob const &unfinshedJob,
                                       QFileInfoList const &missingFiles,
                                       int                  nbTotalFiles);
    static QFileInfoList _filesInPackingPath(QString const &packingDirPath);

    static bool _doFilesChecks(UnfinishedJob const &unfinishedJob,
                               QStringList const   &postedFiles,
                               QFileInfoList       &filesInPackingDir);

    static void _logNbUnfinshedJobs(int nbUnfinishedJobs);

    static ushort _deleteExistingPar2(QString const &packingPath);
    static bool   _deletePackingDirectory(QString const &packingPath);
};

#endif // RESUMEJOBSSERVICE_H
