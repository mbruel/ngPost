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
#include "NgPost.h"
#include "nntp/NntpArticle.h"
#include "NntpConnection.h"
#include "PostingJob.h"

Poster::Poster(PostingJob *job, ushort id)
    : _id(id)
    , _ngPost(job->_ngPost)
    , _job(job)
    , _builderThread()
    , _connectionsThread()
    , _articleBuilder(new ArticleBuilder(this))
    , _nntpConnections()
    , _articles()
    , _secureArticles()
{
    _builderThread.setObjectName(QString("Builder #%1").arg(id));
    _connectionsThread.setObjectName(QString("Poster #%1").arg(id));
    _articleBuilder->moveToThread(&_builderThread);
}

Poster::~Poster()
{
#if defined(__DEBUG__) && defined(LOG_CONSTRUCTORS)
    qDebug() << "Destruction Poster #" << _id;
#endif

    stopThreads();

    if (_articleBuilder)
        delete _articleBuilder;
}

void Poster::addConnection(NntpConnection *connection)
{
    _nntpConnections.append(connection);
    connection->setPoster(this);
    connection->moveToThread(&_connectionsThread);
    emit connection->startConnection();
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
#endif

NntpArticle *Poster::getNextArticle(QString const &conPrefix)
{

    QMutexLocker lock(&_secureArticles); // thread safety (coming from a posting thread)

    if (MB_LoadAtomic(_job->_stopPosting))
        return nullptr;

    if (_ngPost->debugFull())
        _job->_log(QString("[%1][Poster::getNextArticle] _articles.size() = %2")
                           .arg(conPrefix)
                           .arg(_articles.size()));

    NntpArticle *article = nullptr;
    if (_articles.size())
        article = _articles.dequeue();
    else
    {
        if (!MB_LoadAtomic(_job->_noMoreFiles))
        {
            // we should never come here as the goal is to have articles prepared in advance in the queue
            if (_ngPost->debugFull())
                _job->_log(QString("[%1][Poster::getNextArticle] no article prepared...").arg(conPrefix));

            article = _prepareNextArticle(conPrefix, false);
        }
    }

    if (article)
        emit _articleBuilder
                ->scheduleNextArticle(); // schedule the preparation of another Article in the _builderThread

    return article;
}

#ifdef __RELEASE_ARTICLES_WHEN_CON_FAILS__
void Poster::releaseArticle(QString const &conPrefix, NntpArticle *article)
{
    QMutexLocker lock(&_secureArticles); // thread safety (coming from a posting thread)
    if (_ngPost->debugMode())
        _job->_log(QString("[%1] releasing Article: %2").arg(conPrefix).arg(article->str()));

    // the current NntpConnection releasing the Article will close
    // so we need at least another one that would try to post the Article
    if (nbActiveConnections() > 2)
    {
        article->resetNbTrySending();
        _articles.prepend(article);
    }
    else
    {
        _job->_error(QString("give up on Article: %1").arg(article->str()));
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
            _job->_log(QString("[%1] prepareArticlesInAdvance : no more Articles to produce after i = %1")
                               .arg(_builderThread.objectName())
                               .arg(i));
#endif
            canProduceAll = false;
            break;
        }
    }
#ifdef __DEBUG__
    _job->_log(QString("[%1] prepareArticlesInAdvance: Article queue size:  %1")
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

bool Poster::isPosting() const { return _job->isPosting(); }

void Poster::stopThreads()
{
    _builderThread.quit();
    _connectionsThread.quit();

    if (_ngPost->debugMode())
        _job->_log(QString("[Poster #%1] threads quit").arg(_id));

    _connectionsThread.wait();
    if (_ngPost->debugMode())
        _job->_log(QString("[Poster #%1] connection thread done").arg(_id));

    _builderThread.wait();

    if (_ngPost->debugMode())
        _job->_log(QString("[Poster #%1] threads stopped").arg(_id));
}
