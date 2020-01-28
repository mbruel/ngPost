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

#include "ArticleBuilder.h"
#include "Poster.h"
#include "NgPost.h"
#include "PostingJob.h"
#include "nntp/NntpArticle.h"

ArticleBuilder::ArticleBuilder(Poster *poster, QObject *parent):
    QObject (parent),
    _ngPost(poster->_ngPost),
    _poster(poster),
    _job(poster->_job),
    _buffer(new char[static_cast<quint64>(_ngPost->articleSize())+1])
{
    connect(this, &ArticleBuilder::scheduleNextArticle, this, &ArticleBuilder::onPrepareNextArticle, Qt::QueuedConnection);
}

ArticleBuilder::~ArticleBuilder()
{
    delete[] _buffer;
}

NntpArticle *ArticleBuilder::getNextArticle(const QString &threadName)
{
    _job->_secureDiskAccess.lock();
    NntpArticle *article = _job->_readNextArticleIntoBufferPtr(threadName, &_buffer);
    _job->_secureDiskAccess.unlock();
    if (article)
    {
        article->yEncBody(_buffer);
#ifdef __SAVE_ARTICLES__
        article->dumpToFile("/tmp", _ngPost->aticleSignature());
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
