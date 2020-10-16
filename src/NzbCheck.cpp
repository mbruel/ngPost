//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
//
// This file is a part of ngPost : https://github.com/mbruel/nzbCheck
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

#include "NzbCheck.h"
#include "NntpCheckCon.h"
#include "nntp/NntpServerParams.h"
#include <cmath>

#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QCoreApplication>
#include <QRegularExpression>

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
            _cout << tr("Nb Article Failed: %1/%2").arg(_nbMissingArticles).arg(_nbTotalArticles) << "\n" << MB_FLUSH;
        qApp->quit();
    }
}

void NzbCheck::onRefreshprogressbarBar()
{
    float progressbar = static_cast<float>(_nbCheckedArticles);
    progressbar /= _nbTotalArticles;

    _cout << "\r[";
    int pos = static_cast<int>(std::floor(progressbar * sprogressbarBarWidth));
    for (int i = 0; i < sprogressbarBarWidth; ++i) {
        if (i < pos) _cout << "=";
        else if (i == pos) _cout << ">";
        else _cout << " ";
    }
    _cout << "] " << int(progressbar * 100) << " %"
              << " (" << _nbCheckedArticles << " / " << _nbTotalArticles << ")"
              << tr(" missing: ") << _nbMissingArticles;
    _cout.flush();

    if (_nbCheckedArticles < _nbTotalArticles)
        _progressbarTimer.start(_refreshRate);
}

NzbCheck::NzbCheck():QObject(),
    _nzbPath(), _articles(),
    _cout(stdout), _cerr(stderr),
    _nbTotalArticles(0),  _nbMissingArticles(0), _nbCheckedArticles(0),
    _nntpServers(),
    _debug(0), _connections(),
    _dispProgressBar(false), _progressbarTimer(), _refreshRate(sDefaultRefreshRate),
    _quietMode(false)
{}

NzbCheck::~NzbCheck()
{
    if (_dispProgressBar)
        _progressbarTimer.stop();
}

int NzbCheck::parseNzb()
{
    QFile file(_nzbPath);
    if (file.open(QIODevice::ReadOnly|QIODevice::Text))
    {
        QXmlStreamReader xmlReader(&file);
        while ( !xmlReader.atEnd() )
        {
            QXmlStreamReader::TokenType type = xmlReader.readNext();
            if (type == QXmlStreamReader::TokenType::StartElement
                    && xmlReader.name() == "segment")
            {
                xmlReader.readNext();
                _articles.push(QString("<%1>").arg(xmlReader.text().toString()));
            }
        }

        if (xmlReader.hasError()) {
            _cerr << "parsing error: " << xmlReader.errorString()
                      << " at line: " << xmlReader.lineNumber() << "\n" << MB_FLUSH;
            return -2;
        }
        _nbTotalArticles = _articles.size();
        if (!_quietMode)
            _cout << tr("%1 has %2 articles").arg(QFileInfo(_nzbPath).fileName()).arg(_nbTotalArticles) << "\n" << MB_FLUSH;
        return _nbTotalArticles;
    }
    else
    {
        _cerr << tr("Error opening nzb file...") << "\n" << MB_FLUSH;
        return -1;
    }

}

void NzbCheck::checkPost()
{
    int nbCons = 0;
    for (NntpServerParams *srvParam : _nntpServers)
    {
        if (srvParam->nzbCheck)
            nbCons += srvParam->nbCons;
    }

    nbCons = std::min(_nbTotalArticles, nbCons);

    int nb = 0;
    for (NntpServerParams *srvParam : _nntpServers)
    {
        if (srvParam->nzbCheck)
        {
            for (int i = 1 ; i <= srvParam->nbCons; ++i)
            {
                NntpCheckCon *con = new NntpCheckCon(this, i, *srvParam);
                connect(con, &NntpCheckCon::disconnected, this, &NzbCheck::onDisconnected, Qt::DirectConnection);
                emit con->startConnection();

                _connections.insert(con);

                if (++nb == nbCons)
                    break;
            }
            if (nb == nbCons)
                break;
        }
    }

    if (debugMode())
        _cout << tr("Using %1 Connections").arg(nbCons) << "\n" << MB_FLUSH;

    if (_dispProgressBar)
    {
        connect(&_progressbarTimer, &QTimer::timeout, this, &NzbCheck::onRefreshprogressbarBar, Qt::DirectConnection);
        _progressbarTimer.start(_refreshRate);
    }
}
