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

#include "ArticleBuilder.h"
#include "NgConf.h"
#include "nntp/NntpArticle.h"
#include "Poster.h"
#include "PostingJob.h"

ArticleBuilder::ArticleBuilder(Poster *poster, QObject *parent)
    : QObject(parent)
    , _poster(poster)
    , _job(poster->_job)
    , _buffer(new char[static_cast<quint64>(NgConf::kArticleSize) + 1])
{
    connect(this,
            &ArticleBuilder::scheduleNextArticle,
            this,
            &ArticleBuilder::onPrepareNextArticle,
            Qt::QueuedConnection);
}

ArticleBuilder::~ArticleBuilder() { delete[] _buffer; }

NntpArticle *ArticleBuilder::getNextArticle(QString const &threadName)
{
    _job->_secureDiskAccess.lock();
    NntpArticle *article = _job->_readNextArticleIntoBufferPtr(threadName, &_buffer);
    _job->_secureDiskAccess.unlock();
    if (article)
    {
        article->yEncBody(_buffer);
#ifdef __SAVE_ARTICLES__
        article->dumpToFile("/tmp", NgConf::kArticleIdSignature);
#endif
    }
    return article;
}

void ArticleBuilder::onPrepareNextArticle()
{
    QMutexLocker lock(&_poster->_secureArticles); // thread safety (coming from _builderThread)

    NntpArticle *article = getNextArticle(_poster->_builderThread.objectName());
    if (article)
        _poster->_articles.enqueue(article);
}
