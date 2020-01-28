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

#ifndef ARTICLEBUILDER_H
#define ARTICLEBUILDER_H

#include <QObject>
class NgPost;
class Poster;
class PostingJob;
class NntpArticle;

/*!
 * \brief The ArticleBuilder is a worker that will be moved to the Poster::_builderThread
 *
 * it is supposed to fill the Poster::_articles queue in its event loop
 *  using scheduleNextArticle / onPrepareNextArticle
 *
 * if an NntpConnection is in advance, it might use directly getNextArticle via the Poster
 * this scenario should not happen as there a one to one Builder / Poster Thread.
 */
class ArticleBuilder : public QObject
{
    Q_OBJECT

private:
    NgPost     *const _ngPost;
    Poster     *const _poster;
    PostingJob *const _job;

    char *_buffer;   //!< buffer to read the current file to build an Article

signals:
    void scheduleNextArticle();


private slots:
    void onPrepareNextArticle();


public:
    ArticleBuilder(Poster *poster, QObject *parent = nullptr);
    ~ArticleBuilder();

    NntpArticle *getNextArticle(const QString &threadName);

};

#endif // ARTICLEBUILDER_H
