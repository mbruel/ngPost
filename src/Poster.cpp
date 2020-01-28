//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
//
// This file is a part of ngPost : https://github.com/mbruel/ngPost
//
// ngPost is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; version 3.0 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// You should have received a copy of the GNU Lesser General Public
// License along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301,
// USA.
//
//========================================================================

#include "Poster.h"
#include "NgPost.h"
#include "PostingJob.h"
#include "ArticleBuilder.h"
#include "NntpConnection.h"
#include "nntp/NntpArticle.h"

Poster::Poster(PostingJob *job, ushort id):
    _id(id),
    _ngPost(job->_ngPost),
    _job(job),
    _builderThread(),
    _connectionsThread(),
    _articleBuilder(new ArticleBuilder(this)),
    _nntpConnections(),
    _articles(),
    _secureArticles()
{
    _builderThread.setObjectName(QString("Builder #%1").arg(id));
    _connectionsThread.setObjectName(QString("Poster #%1").arg(id));
    _articleBuilder->moveToThread(&_builderThread);
}

Poster::~Poster()
{
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

NntpArticle *Poster::getNextArticle(const QString &conPrefix)
{

    QMutexLocker lock(&_secureArticles); // thread safety (coming from a posting thread)

    if (_job->_stopPosting.load())
        return nullptr;

    if (_ngPost->debugMode())
        _job->_log(QString("[%1][Poster::getNextArticle] _articles.size() = %2").arg(conPrefix).arg(_articles.size()));

    NntpArticle *article = nullptr;
    if (_articles.size())
        article = _articles.dequeue();
    else
    {
        if (!_job->_noMoreFiles.load())
        {
            // we should never come here as the goal is to have articles prepared in advance in the queue
            if (_ngPost->debugMode())
                _job->_log(QString("[%1][Poster::getNextArticle] no article prepared...").arg(conPrefix));

            article = _prepareNextArticle(conPrefix, false);
        }
    }

    if (article)
        emit _articleBuilder->scheduleNextArticle(); // schedule the preparation of another Article in the _builderThread

    return article;

}

bool Poster::prepareArticlesInAdvance()
{
    bool canProduceAll = true;
    int nbArticlesToPrepare = _nntpConnections.size();
    for (int i = 0; i < nbArticlesToPrepare ; ++i)
    {
        if (!_prepareNextArticle(_builderThread.objectName()))
        {
#ifdef __DEBUG__
            _job->_log(QString("[%1] prepareArticlesInAdvance : no more Articles to produce after i = %1").arg(_builderThread.objectName()).arg(i));
#endif
            canProduceAll = false;
            break;
        }
    }
#ifdef __DEBUG__
    _job->_log(QString("[%1] prepareArticlesInAdvance: Article queue size:  %1").arg(_builderThread.objectName()).arg(_articles.size()));
#endif

    return canProduceAll;
}

NntpArticle *Poster::_prepareNextArticle(const QString &threadName, bool fillQueue)
{
    NntpArticle *article = _articleBuilder->getNextArticle(threadName);
    if (article && fillQueue)
        _articles.enqueue(article);
    return article;
}
