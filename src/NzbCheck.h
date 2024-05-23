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
#ifndef NZBCHECK_H
#define NZBCHECK_H

#include <QCommandLineOption>
#include <QElapsedTimer>
#include <QObject>
#include <QSet>
#include <QStack>
#include <QString>
#include <QTextStream>
#include <QTimer>

#include "PostingParams.h"
#include "utils/ProgressBarShell.h"

class NntpCheckCon;

class NzbCheck : public QObject
{
    Q_OBJECT
    friend NntpCheckCon; // only class allowed to use getNextArticle and update _nbArticles*

private:
    inline static const QString       kXMLTagSubject   = "subject";
    inline static const QLatin1String kXMLTokenFile    = QLatin1String("file");
    inline static const QLatin1String kXMLTokenSegment = QLatin1String("segment");

    inline static const QRegularExpression kNntpArticleYencSubjectRegExp =
            QRegularExpression("^\\[\\d+/\\d+\\]\\s+.+\\(\\d+/(\\d+)\\)$");

    SharedParams const &_postingParams; //!< owned by NgPost (no need to copy as we won't modify it or detach)
    QString             _nzbPath;       //!< path of the nzb we need to check
    QStack<QString>     _articles;      //!< ids of all the articles of the nzb (

    QList<NntpServerParams *> _nntpServers; //!< the servers parameters that are allowed for checking headers

    QSet<NntpCheckCon *> _connections;
    uint                 _nbCons;

    uint _nbArticlesTotal;
    uint _nbArticlesMissing;
    uint _nbArticlesChecked;

    QElapsedTimer          _timeStart;
    ProgressBar::ShellBar *_progressBar;

public:
    NzbCheck(SharedParams const &postingParams, QString const &nzbPath);
    ~NzbCheck();

    /*!
     * \brief from postingParams->nntpServers() save the ones with nzbCheck allowed in _nntpServers
     *  most servers are only used for posting (upload)
     *  others can be used for download (even if checking header doesn't use download quota normally)
     * \return _nbCons
     */
    int hasCheckingConnections();

    /*!
     * \brief parseNzb the nzb file to fill _articles with the ids of the aticles
     * \return the total number of Articles (_nbArticlesTotal)
     */
    bool parseNzb();

    /*!
     * \brief start the actual job:
     * create the NntpCheckCon and start them
     * start the progress bar (if not quiet mode)
     */
    void startCheckingNzb();

    void useProgressBar(bool display);

    //! result of the check (output of the program)
    int nbMissingArticles() const { return _nbArticlesMissing; }

private slots:
    void onDisconnected(NntpCheckCon *con);

private:
    void progressUpdateInfo(ProgressBar::UpdateBarInfo &currentPos)
    {
        currentPos.update(_nbArticlesChecked,
                          _nbArticlesTotal,
                          QString(tr("missing articles: %1").arg(_nbArticlesMissing)));
    }

    QString getNextArticle() //!< only used by NntpCheckCons
    {
        if (_articles.isEmpty())
            return QString();
        else
            return _articles.pop();
    }

    void missingArticle(QString const &article);    //!< only used by NntpCheckCons
    void articleChecked() { ++_nbArticlesChecked; } //!< only used by NntpCheckCons
};

#endif // NZBCHECK_H
