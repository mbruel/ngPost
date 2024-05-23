//========================================================================
//
// Copyright (C) 2020-2024 Matthieu Bruel <Matthieu.Bruel@gmail.com>
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

#include "NzbCheck.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTime>
#include <QXmlStreamReader>

#include "NgConf.h"
#include "nntp/NntpServerParams.h"
#include "NntpCheckCon.h"
#include "utils/NgLogger.h"

NzbCheck::NzbCheck(SharedParams const &postingParams, QString const &nzbPath)
    : QObject(nullptr)
    , _postingParams(postingParams)
    , _nzbPath(nzbPath)
    , _articles()
    , _nntpServers()
    , _connections()
    , _nbCons(0)
    , _nbArticlesTotal(0)
    , _nbArticlesMissing(0)
    , _nbArticlesChecked(0)
    , _progressBar(nullptr)
{
}

NzbCheck::~NzbCheck()
{
    if (_progressBar)
        _progressBar->deleteLater();
}

void NzbCheck::useProgressBar(bool display)
{
    if (display && !_postingParams->quietMode())

        _progressBar = new ::ProgressBar::ShellBar([this](ProgressBar::UpdateBarInfo &currentPos)
                                                   { progressUpdateInfo(currentPos); },
                                                   NgConf::kProgressBarWidth,
                                                   NgConf::kDefaultRefreshRate);
}

void NzbCheck::startCheckingNzb()
{
    _timeStart.start();
    _nbCons = std::min(_nbArticlesTotal, _nbCons);

    uint nb = 0;
    for (NntpServerParams *srvParam : _nntpServers)
    {
        if (srvParam->nzbCheck)
        {
            for (int i = 1; i <= srvParam->nbCons; ++i)
            {
                NntpCheckCon *con = new NntpCheckCon(this, i, *srvParam);
                connect(con, &NntpCheckCon::disconnected, this, &NzbCheck::onDisconnected, Qt::DirectConnection);
                emit con->startConnection();

                _connections.insert(con);

                if (++nb == _nbCons)
                    break;
            }
            if (nb == _nbCons)
                break;
        }
    }

    if (!_postingParams->quietMode())
        NgLogger::log(tr("Start checking the nzb with %1 connections on %2 servers)")
                              .arg(_nbCons)
                              .arg(_nntpServers.size()),
                      true);

    if (_progressBar)
        _progressBar->start();
}

void NzbCheck::missingArticle(QString const &article)
{
    if (!_postingParams->quietMode())
    {
        if (_progressBar)
            NgLogger::log("", true);
        NgLogger::log(tr("+ Missing Article on server: %1)").arg(article), true);
    }
    ++_nbArticlesMissing;
}

void NzbCheck::onDisconnected(NntpCheckCon *con)
{
    _connections.remove(con);
    if (_connections.isEmpty())
    {
        if (_progressBar)
            _progressBar->stop();

        if (!_postingParams->quietMode())
        {
            qint64 duration = _timeStart.elapsed();
            NgLogger::log(
                    tr("Nb Missing Article(s): %1/%2 (check done in %3 (%4 sec) using %5 connections on %6 "
                       "server(s))")
                            .arg(_nbArticlesMissing)
                            .arg(_nbArticlesTotal)
                            .arg(QTime::fromMSecsSinceStartOfDay(static_cast<int>(duration))
                                         .toString("hh:mm:ss.zzz"))
                            .arg(std::round(1. * duration / 1000))
                            .arg(_nbCons)
                            .arg(_nntpServers.size()),
                    true);
        }
        qApp->quit(); // end of game :)
    }
}

int NzbCheck::hasCheckingConnections()
{
    auto const &allNntpSrvs = _postingParams->nntpServers();
    for (NntpServerParams *srvParam : allNntpSrvs)
    {
        if (srvParam->nzbCheck)
        {
            _nntpServers << srvParam;
            _nbCons += srvParam->nbCons;
        }
    }
    return _nbCons;
}

bool NzbCheck::parseNzb()
{
    QFile file(_nzbPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        NgLogger::criticalError(tr("Error opening nzb file %1").arg(_nzbPath),
                                NgError::ERR_CODE::ERR_FILE_NOT_EXIST);
        return false;
    }

    QXmlStreamReader xmlReader(&file);
    while (!xmlReader.atEnd())
    {
        QXmlStreamReader::TokenType type = xmlReader.readNext();
        if (type == QXmlStreamReader::TokenType::StartElement && xmlReader.name().compare(kXMLTokenFile) == 0)
        {
            auto subject    = xmlReader.attributes().value(kXMLTagSubject).toString();
            auto match      = kNntpArticleYencSubjectRegExp.match(subject);
            int  nbArticles = 0, nbExpectedArticles = 0;
            if (match.hasMatch())
                nbExpectedArticles = match.captured(1).toInt();
            while (!xmlReader.atEnd())
            {
                auto type = xmlReader.readNext();
                if (type == QXmlStreamReader::TokenType::EndElement
                    && xmlReader.name().compare(kXMLTokenFile) == 0)
                {
                    NgLogger::log(tr("The file '%1' has %2 articles in the nzb (expected: %3)")
                                          .arg(subject)
                                          .arg(nbArticles)
                                          .arg(nbExpectedArticles),
                                  true,
                                  NgLogger::DebugLevel::Debug);
                    if (nbArticles < nbExpectedArticles)
                    {
                        if (!_postingParams->quietMode())
                            NgLogger::log(tr("- %1 missing Article(s) in nzb for '%2'")
                                                  .arg(nbExpectedArticles - nbArticles)
                                                  .arg(subject),
                                          true);
                        _nbArticlesMissing += nbExpectedArticles - nbArticles;
                    }

                    break;
                }
                else if (type == QXmlStreamReader::TokenType::StartElement
                         && xmlReader.name().compare(kXMLTokenSegment) == 0)
                {
                    ++nbArticles;
                    xmlReader.readNext();
                    _articles.push(QString("<%1>").arg(xmlReader.text().toString()));
                }
            }
        }
    }

    if (xmlReader.hasError())
    {
        NgLogger::criticalError(tr("Error parsing nzb file at line %1 : %2")
                                        .arg(xmlReader.lineNumber())
                                        .arg(xmlReader.errorString()),
                                NgError::ERR_CODE::ERR_FILE_NOT_EXIST);
        return false;
    }
    _nbArticlesTotal = _articles.size();
    if (!_postingParams->quietMode())
        NgLogger::log(tr("%1 has %2 articles").arg(_nzbPath).arg(_nbArticlesTotal), true);
    return _nbArticlesTotal;
}
