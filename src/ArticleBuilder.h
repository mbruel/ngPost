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
#ifndef ARTICLEBUILDER_H
#define ARTICLEBUILDER_H

#include <QObject>
class Poster;
class PostingJob;
namespace NNTP
{
class Article;
} // namespace NNTP

/*!
 * \brief The ArticleBuilder is a worker that will be moved to the Poster::_builderThread
 *
 * it is supposed to fill the Poster::_articles queue in its event loop
 *  using sigScheduleNextArticle / onPrepareNextArticle
 *
 * if an NntpConnection is in advance, it might use directly getNextArticle via the Poster
 * this scenario should not happen as there a one to one Builder / Poster Thread.
 */
class ArticleBuilder : public QObject
{
    Q_OBJECT
signals:
    void sigScheduleNextArticle();

private:
    Poster *const     _poster;
    PostingJob *const _job;

    char *_buffer; //!< buffer to read the current file to build an Article

private slots:
    void onPrepareNextArticle();

public:
    ArticleBuilder(Poster *poster, QObject *parent = nullptr);
    ~ArticleBuilder();

    NNTP::Article *getNextArticle(QString const &threadName);
};

#endif // ARTICLEBUILDER_H
