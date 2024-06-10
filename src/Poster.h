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
#ifndef POSTER_H
#define POSTER_H

#include <QCoreApplication>
#include <QMutex>
#include <QQueue>
#include <QThread>
#include <QVector>

namespace NNTP
{
class Article;
} // namespace NNTP

class ArticleBuilder;
class NntpConnection;
class PostingJob;

#include "utils/NgLogger.h"

using AtomicBool = QAtomicInteger<unsigned short>; // 16 bit only (faster than using 8 bit variable...)

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
class Poster : public QObject
{
    Q_OBJECT

    friend class ArticleBuilder; // MB_TODO: check if we can avoid! we could?

signals:
    void sigNoMorePostingConnection(Poster *poster);

private:
    const ushort      _id;  //!< poster id (a PostingJob, its owner, has several)
    PostingJob *const _job; //!< owner

    QString _logPrefix; //!< log prefix: Poster[<_id>]

#ifndef __test_ngPost__
    QThread _builderThread;     //!< thread that runs _articleBuilder
    QThread _connectionsThread; //!< thread that runs the _nntpConnections
#endif
    ArticleBuilder           *_articleBuilder;  //!< build NntpArticles. deleteLater when stopThreads()
    QVector<NntpConnection *> _nntpConnections; //!< post NntpArticles. deleteLater when stopThreads()

    QQueue<NNTP::Article *> _articles; //!< NNTP::Article produced by _articleBuilder posted on _nntpConnections
    QMutex                  _secureArticles; //!< secure _articles as different producer and consumer threads

    AtomicBool _hasBeenStopped; //!< know if the the posting has been stopped before normal end

public:
    Poster(PostingJob *job, ushort id);
    ~Poster();

    void addConnection(NntpConnection *connection);

    NNTP::Article *getNextArticle(QString const &conPrefix);

#ifdef __RELEASE_ARTICLES_WHEN_CON_FAILS__
    void releaseArticle(QString const &conPrefix, NNTP::Article *article);
    uint nbActiveConnections() const;
#endif

    bool tryResumePostWhenConnectionLost() const;

    inline void lockQueue();
    inline void unlockQueue();

    inline QString const &name() const;

    inline void startThreads();
    void        stopThreads();

    bool prepareArticlesInAdvance();

    bool isPosting() const;

    QString builderThreadName() const
    {
#ifndef __test_ngPost__
        return _builderThread.objectName();
#else
        return _logPrefix;
#endif
    }

#ifdef __test_ngPost__
    void startNntpConnections();
#endif

public slots:
    void onPostingNotAllowed(NntpConnection *nntpCon);

private:
    NNTP::Article *_prepareNextArticle(QString const &threadName, bool fillQueue = true);

    void _log(QString const       &aMsg,
              NgLogger::DebugLevel debugLvl,
              bool                 newline = true) const; //!< log function for QString
    void _error(QString const &error) const;
};

void Poster::lockQueue() { _secureArticles.lock(); }
void Poster::unlockQueue() { _secureArticles.unlock(); }

QString const &Poster::name() const { return _logPrefix; }

void Poster::startThreads()
{
#ifndef __test_ngPost__
    _builderThread.start();
    _connectionsThread.start();
#endif
}

#endif // POSTER_H
