//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
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

#include "Poster.h"
#include "ArticleBuilder.h"
#include "nntp/NntpArticle.h"
#include "NntpConnection.h"
#include "PostingJob.h"
#include "utils/NgLogger.h"

Poster::Poster(PostingJob *job, ushort id)
    : _id(id)
    , _job(job)
    , _logPrefix(QString("Poster #%1").arg(_id))
    , _builderThread()
    , _connectionsThread()
    , _articleBuilder(new ArticleBuilder(this))
    , _nntpConnections()
    , _articles()
    , _secureArticles()
    , _hasBeenStopped(0x0)
{
    _builderThread.setObjectName(QString(NgConf::kThreadNameBuilder).arg(id));
    _connectionsThread.setObjectName(QString(NgConf::kThreadNameNntp).arg(id));
    _articleBuilder->setObjectName(QString("ArtBuilder #%1").arg(_id));

    // as the _articleBuilder is moved, the Poster can't own it (meaning destroy it)
    // it should be destoyed in the thread where it is living
    // cf https://doc.qt.io/qt-6/qthread.html#finished
    QObject::connect(&_builderThread, &QThread::finished, _articleBuilder, &QObject::deleteLater);
    _articleBuilder->moveToThread(&_builderThread);

#if (defined(__DEBUG__) && defined(LOG_CONSTRUCTORS)) || defined(__MOVETOTHREAD_TRACKING__)
    qDebug() << QString("[THREAD] %1 owned by Poster #%2 has moved to its thread %3")
                        .arg(_articleBuilder->objectName())
                        .arg(_id)
                        .arg(_builderThread.objectName());
#endif
}

Poster::~Poster()
{
#if (defined(__DEBUG__) && defined(LOG_CONSTRUCTORS)) || defined(__MOVETOTHREAD_TRACKING__)
    qDebug() << "Destruction Poster #" << _id;
#endif

    if (!MB_LoadAtomic(_hasBeenStopped))
        stopThreads();

    // we down own anything:
    //   - _nntpConnections will be deleteLater when _connectionsThread.quit()
    //   - _articles are owned by their NntpFile
}

void Poster::addConnection(NntpConnection *connection)
{
    _nntpConnections.append(connection);
    connection->setPoster(this); // to release Articles, set their _prefix...

    // as the connection is moved, it has no more "owner" (no parent of course)
    // it should be destoyed in the thread where it is living
    // cf https://doc.qt.io/qt-6/qthread.html#finished
    QObject::connect(&_connectionsThread, &QThread::finished, connection, &QObject::deleteLater);

    connection->moveToThread(&_connectionsThread);

    // start the connection (in _connectionsThread)
    emit connection->startConnection();

#ifdef __MOVETOTHREAD_TRACKING__
    qDebug() << QString("[THREAD] NntpCon #%1 has been started, affected to Poster #%2, moved to its thread %3")
                        .arg(connection->getId())
                        .arg(_id)
                        .arg(_connectionsThread.objectName());
#endif
}

NntpArticle *Poster::getNextArticle(QString const &conPrefix)
{

    QMutexLocker lock(&_secureArticles); // thread safety (coming from a posting thread)

    if (MB_LoadAtomic(_job->_stopPosting))
        return nullptr;

    if (NgLogger::isFullDebug())
        _log(QString("[%1] getNextArticle _articles.size() = %2").arg(conPrefix).arg(_articles.size()));

    NntpArticle *article = nullptr;
    if (_articles.size())
        article = _articles.dequeue();
    else
    {
        if (!MB_LoadAtomic(_job->_noMoreFiles))
        {
            // we should never come here as the goal is to have articles prepared in advance in the queue
            if (NgLogger::isFullDebug())
                _log(tr("no article prepared..."), true);

            article = _prepareNextArticle(conPrefix, false);
        }
    }

    if (article)
        emit _articleBuilder
                ->scheduleNextArticle(); // schedule the preparation of another Article in the _builderThread

    return article;
}

#ifdef __RELEASE_ARTICLES_WHEN_CON_FAILS__
uint Poster::nbActiveConnections() const
{
    uint nbActives = 0;
    for (NntpConnection *con : _nntpConnections)
    {
        if (con->isConnected())
            ++nbActives;
    }
    return nbActives;
}

void Poster::releaseArticle(QString const &conPrefix, NntpArticle *article)
{
    QMutexLocker lock(&_secureArticles); // thread safety (coming from a posting thread)
    if (NgLogger::isDebugMode())
        _log(QString("[%1] try to release Article: %2").arg(conPrefix).arg(article->str()));

    // the current NntpConnection releasing the Article will close
    // so we need at least another one that would try to post the Article
    if (nbActiveConnections() > 2)
    {
        article->resetNbTrySending();
        _articles.prepend(article);
    }
    else
    {
        _error(tr("[%1] giving up on Article: %1").arg(conPrefix).arg(article->str()));
        emit article->failed(article->size());
    }
}
#endif

bool Poster::tryResumePostWhenConnectionLost() const { return _job->tryResumePostWhenConnectionLost(); }

bool Poster::prepareArticlesInAdvance()
{
    bool canProduceAll       = true;
    int  nbArticlesToPrepare = _nntpConnections.size();
    for (int i = 0; i < nbArticlesToPrepare; ++i)
    {
        if (!_prepareNextArticle(_builderThread.objectName()))
        {
#ifdef __DEBUG__
            _log(QString("[%1] prepareArticlesInAdvance : no more Articles to produce after i = %2")
                         .arg(_builderThread.objectName())
                         .arg(i));
#endif
            canProduceAll = false;
            break;
        }
    }
#ifdef __DEBUG__
    _log(QString("[%1] prepareArticlesInAdvance: Article queue size:  %2")
                 .arg(_builderThread.objectName())
                 .arg(_articles.size()));
#endif

    return canProduceAll;
}

NntpArticle *Poster::_prepareNextArticle(QString const &threadName, bool fillQueue)
{
    NntpArticle *article = _articleBuilder->getNextArticle(threadName);
    if (article && fillQueue)
        _articles.enqueue(article);
    return article;
}

void Poster::_log(QString const &aMsg, bool newline) const
{
    NgLogger::log(QString("[%1] %2").arg(_logPrefix).arg(aMsg), newline);
}

void Poster::_error(QString const &error) const
{
    NgLogger::error(QString("[%1] %2").arg(_logPrefix).arg(error));
}

bool Poster::isPosting() const { return _job->isPosting(); }

void Poster::stopThreads()
{
    _builderThread.quit();
    _connectionsThread.quit();

    if (NgLogger::isDebugMode())
        _log(tr("threads: %1, %2 are stopped (no more event queue)")
                     .arg(_builderThread.objectName(), _connectionsThread.objectName()));

    bool threadDone = _connectionsThread.wait();
    if (NgLogger::isDebugMode())
        _log(tr("Connection thread is done: %2").arg(threadDone));

    threadDone = _builderThread.wait();

    if (NgLogger::isDebugMode())
        _log(tr("all threads done: %2").arg(threadDone));

    _hasBeenStopped = 0x1;
}
