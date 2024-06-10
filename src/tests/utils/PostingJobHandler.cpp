#include "PostingJobHandler.h"
#include <QDebug>

#include "PostingJob.h"

PostingJobHandler::PostingJobHandler(PostingJob *job, QObject *parent)
    : QObject(parent), _job(job), _testDone(false), _isAutenticated(false), _testPosting(false)
{
    if (_job)
    {
        // starting _thread (using PostingJobHandler::start) starts the _nntpCon
        connect(
                &_thread,
                &QThread::started,
                _job,
                [this]() { emit _job->sigStartPosting(true); },
                Qt::QueuedConnection);

        connect(_job,
                &PostingJob::sigPostingStarted,
                this,
                &PostingJobHandler::onPostingStarted,
                Qt::QueuedConnection);
        connect(_job,
                &PostingJob::sigPostingFinished,
                this,
                &PostingJobHandler::onPostingFinished,
                Qt::QueuedConnection);
        _job->moveToThread(&_thread);
    }
    else
        qCritical() << "PostingJobHandler couldn't create a connection...";

    // this->moveToThread(&_thread);
}

PostingJobHandler::~PostingJobHandler()
{
    if (_thread.isRunning())
        _thread.quit();
}

void PostingJobHandler::start()
{
    // no need cause  to _nntpCon->sigStartConnection() as it's connected to &QThread::started
    _thread.start();
}

void PostingJobHandler::onPostingStarted() { NgLogger::log("onPostingStarted...", true); }
void PostingJobHandler::onPostingFinished()
{
    NgLogger::log("onPostingFinished!", true);
    qDebug() << "[ConnectionHandler::onStopTest] Stopping the Test!!!...";
    if (_job)
        _job->deleteLater();
    _thread.quit();
    _testDone = true;
}
