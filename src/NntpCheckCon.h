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
#ifndef NNTPCHECKCON_H
#define NNTPCHECKCON_H

#include "nntp/ServerParams.h"
class NzbCheck;

#include <QObject>
#include <QSslError>
#include <QTcpSocket>
class QSslSocket;
class QByteArray;

class NntpCheckCon : public QObject
{
    Q_OBJECT
signals:
    void sigStartConnection();
    void sigKillConnection();

    //    void error(QTcpSocket::SocketError socketerror); //!< Socket Error
    void socketError(QString aError); //!< Error during socket creation (ssl or not)
    void errorConnecting(QString aError);
    void disconnected(NntpCheckCon *con);

private:
    enum class PostingState
    {
        NOT_CONNECTED = 0,
        CONNECTED,
        AUTH_USER,
        AUTH_PASS,
        IDLE,
        CHECKING_ARTICLE
    };

    NzbCheck *const           _nzbCheck;
    int const                 _id;        //!< connection id
    NNTP::ServerParams const &_srvParams; //!< server parameters

    QTcpSocket *_socket;      //!< Real TCP socket
    bool        _isConnected; //!< to avoid to rely on iSocket && iSocket->isOpen()

    PostingState _postingState;
    QString      _currentArticle;

public:
    NntpCheckCon(NzbCheck *nzbCheck, int id, NNTP::ServerParams const &srvParams);
    ~NntpCheckCon();

public slots:
    void onStartConnection();
    void onKillConnection();

    void onConnected();
    void onEncrypted();

    void onDisconnected(); //!< Handle disconnection

    void onReadyRead();                               //!< To be overridden in Child class
    void onSslErrors(QList<QSslError> const &errors); //!< SSL errors handler
    void onErrors(QAbstractSocket::SocketError);      //!< Socket errors handler

private:
    void _closeConnection();
    void _checkNextArticle();
};

#endif // NNTPCHECKCON_H
