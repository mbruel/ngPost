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

#ifndef POSTER_H
#define POSTER_H

#include <QThread>
#include <QVector>
#include <QQueue>
#include <QMutex>
class NgPost;
class ArticleBuilder;
class NntpConnection;
class NntpArticle;
class PostingJob;

/*!
 * \brief Poster is a container class that has 2 Threads
 *    _builderThread to prepare Articles and fill the Queue
 *    _connectionsThread to run several connections that will consume the Article queue
 *
 *  The goal is to make sure that we've the same number of Posters threads than Builders.
 *  Each time a Poster consume an Article, it schedules an event to its Builder to prepare another one
 *
 * It owns the _articleBuilder
 * but it doesn't own the NntpConnection (the PostingJob does)
 */
class Poster
{
    friend class ArticleBuilder;

private:
    const ushort      _id;
    NgPost     *const _ngPost;
    PostingJob *const _job;

    QThread _builderThread;
    QThread _connectionsThread;

    ArticleBuilder          *_articleBuilder;
    QVector<NntpConnection*> _nntpConnections; //!< we don't own them, we just move them to _connectionsThread

    QQueue<NntpArticle*> _articles;
    QMutex               _secureArticles;

public:
    Poster(PostingJob *job, ushort id);
    ~Poster();

    void addConnection(NntpConnection *connection);

    NntpArticle *getNextArticle(const QString &conPrefix);

    inline void lockQueue();
    inline void unlockQueue();

    inline QString name() const;

    inline void startThreads();
    inline void stopThreads();

    bool prepareArticlesInAdvance();


private:
    NntpArticle *_prepareNextArticle(const QString &threadName, bool fillQueue = true);


};

void Poster::lockQueue() { _secureArticles.lock(); }
void Poster::unlockQueue() { _secureArticles.unlock(); }

QString Poster::name() const { return _connectionsThread.objectName(); }

void Poster::startThreads()
{
    _builderThread.start();
    _connectionsThread.start();
}

void Poster::stopThreads()
{
    _builderThread.quit();
    _connectionsThread.quit();

    _connectionsThread.wait();
    _builderThread.wait();
}


#endif // POSTER_H
