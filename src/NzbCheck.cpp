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

#include "NzbCheck.h"
#include <cmath>

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTime>
#include <QXmlStreamReader>

#include "NgConf.h"
#include "nntp/NntpServerParams.h"
#include "NntpCheckCon.h"

const QRegularExpression NzbCheck::sNntpArticleYencSubjectRegExp =
        QRegularExpression(kNntpArticleYencSubjectStrRegExp);

void NzbCheck::onDisconnected(NntpCheckCon *con)
{
    _connections.remove(con);
    if (_connections.isEmpty())
    {
        if (_dispProgressBar)
        {
            disconnect(&_progressbarTimer, &QTimer::timeout, this, &NzbCheck::onRefreshprogressbarBar);
            onRefreshprogressbarBar();
            _cout << "\n" << MB_FLUSH;
        }

        if (!_quietMode)
        {
            qint64 duration = _timeStart.elapsed();
            _cout << tr("Nb Missing Article(s): %1/%2 (check done in %3 (%4 sec) using %5 connections on %6 "
                        "server(s))")
                             .arg(_nbMissingArticles)
                             .arg(_nbTotalArticles)
                             .arg(QTime::fromMSecsSinceStartOfDay(static_cast<int>(duration))
                                          .toString("hh:mm:ss.zzz"))
                             .arg(std::round(1. * duration / 1000))
                             .arg(_nbCons)
                             .arg(nbCheckingServers())
                  << "\n"
                  << MB_FLUSH;
        }
        qApp->quit();
    }
}

void NzbCheck::onRefreshprogressbarBar()
{
    float progressbar = static_cast<float>(_nbCheckedArticles);
    progressbar /= _nbTotalArticles;

    _cout << "\r[";
    int pos = static_cast<int>(std::floor(progressbar * NgConf::kProgressbarBarWidth));
    for (int i = 0; i < NgConf::kProgressbarBarWidth; ++i)
    {
        if (i < pos)
            _cout << "=";
        else if (i == pos)
            _cout << ">";
        else
            _cout << " ";
    }
    _cout << "] " << int(progressbar * 100) << " %"
          << " (" << _nbCheckedArticles << " / " << _nbTotalArticles << ")" << tr(" missing: ")
          << _nbMissingArticles;
    _cout.flush();

    if (_nbCheckedArticles < _nbTotalArticles)
        _progressbarTimer.start(NgConf::kDefaultRefreshRate);
}

NzbCheck::NzbCheck()
    : QObject()
    , _nzbPath()
    , _articles()
    , _cout(stdout)
    , _cerr(stderr)
    , _nbTotalArticles(0)
    , _nbMissingArticles(0)
    , _nbCheckedArticles(0)
    , _nntpServers()
    , _debug(0)
    , _connections()
    , _dispProgressBar(false)
    , _progressbarTimer()
    , _quietMode(false)
{
}

NzbCheck::~NzbCheck()
{
    if (_dispProgressBar)
        _progressbarTimer.stop();
}

int NzbCheck::parseNzb()
{
    QFile file(_nzbPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        _cerr << tr("Error opening nzb file...") << "\n" << MB_FLUSH;
        return -1;
    }

    QXmlStreamReader xmlReader(&file);
    while (!xmlReader.atEnd())
    {
        QXmlStreamReader::TokenType type = xmlReader.readNext();
        if (type == QXmlStreamReader::TokenType::StartElement
            && xmlReader.name().compare(QLatin1String("file")) == 0)
        {
            QString                 subject    = xmlReader.attributes().value("subject").toString();
            QRegularExpressionMatch match      = sNntpArticleYencSubjectRegExp.match(subject);
            int                     nbArticles = 0, nbExpectedArticles = 0;
            if (match.hasMatch())
                nbExpectedArticles = match.captured(1).toInt();
            while (!xmlReader.atEnd())
            {
                QXmlStreamReader::TokenType type = xmlReader.readNext();
                if (type == QXmlStreamReader::TokenType::EndElement
                    && xmlReader.name().compare(QLatin1String("file")) == 0)
                {
                    if (debugMode())
                        _cout << tr("The file '%1' has %2 articles in the nzb (expected: %3)")
                                         .arg(subject)
                                         .arg(nbArticles)
                                         .arg(nbExpectedArticles)
                              << "\n"
                              << MB_FLUSH;
                    if (nbArticles < nbExpectedArticles)
                    {
                        if (!_quietMode)
                            _cout << tr("- %1 missing Article(s) in nzb for '%2'")
                                             .arg(nbExpectedArticles - nbArticles)
                                             .arg(subject)
                                  << "\n"
                                  << MB_FLUSH;

                        _nbMissingArticles += nbExpectedArticles - nbArticles;
                    }

                    break;
                }
                else if (type == QXmlStreamReader::TokenType::StartElement
                         && xmlReader.name().compare(QLatin1String("segment")) == 0)
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
        _cerr << "parsing error: " << xmlReader.errorString() << " at line: " << xmlReader.lineNumber() << "\n"
              << MB_FLUSH;
        return -2;
    }
    _nbTotalArticles = _articles.size();
    if (!_quietMode)
        _cout << tr("%1 has %2 articles").arg(QFileInfo(_nzbPath).fileName()).arg(_nbTotalArticles) << "\n"
              << MB_FLUSH;
    return _nbTotalArticles;
}

void NzbCheck::checkPost()
{
    _timeStart.start();

    _nbCons = 0;
    for (NntpServerParams *srvParam : _nntpServers)
    {
        if (srvParam->nzbCheck)
            _nbCons += srvParam->nbCons;
    }

    _nbCons = std::min(_nbTotalArticles, _nbCons);

    int nb = 0;
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

    if (debugMode())
        _cout << tr("Using %1 Connections").arg(_nbCons) << "\n" << MB_FLUSH;

    if (_dispProgressBar)
    {
        connect(&_progressbarTimer,
                &QTimer::timeout,
                this,
                &NzbCheck::onRefreshprogressbarBar,
                Qt::DirectConnection);
        _progressbarTimer.start(NgConf::kDefaultRefreshRate);
    }
}

int NzbCheck::nbCheckingServers()
{
    int nb = 0;
    for (NntpServerParams *srvParam : _nntpServers)
    {
        if (srvParam->nzbCheck)
            ++nb;
    }
    return nb;
}
