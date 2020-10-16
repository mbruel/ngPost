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

#ifndef NNTPCHECKCON_H
#define NNTPCHECKCON_H
#include "nntp/NntpServerParams.h"
class NzbCheck;

#include <QObject>
#include <QTcpSocket>
class QSslSocket;
class QSslError;
class QByteArray;

class NntpCheckCon : public QObject
{
    Q_OBJECT

private:
    enum class PostingState {NOT_CONNECTED = 0, CONNECTED,
                             AUTH_USER, AUTH_PASS,
                             IDLE, CHECKING_ARTICLE};

    NzbCheck *const        _nzbCheck;
    const int               _id;        //!< connection id
    const NntpServerParams &_srvParams; //!< server parameters

    QTcpSocket   *_socket;         //!< Real TCP socket
    bool          _isConnected;    //!< to avoid to rely on iSocket && iSocket->isOpen()

    PostingState   _postingState;
    QString        _currentArticle;

public:
    NntpCheckCon(NzbCheck *nzbCheck, int id, const NntpServerParams &srvParams);
    ~NntpCheckCon();

signals:
    void startConnection();
    void killConnection();

//    void error(QTcpSocket::SocketError socketerror); //!< Socket Error
    void socketError(QString aError);                //!< Error during socket creation (ssl or not)
    void errorConnecting(QString aError);
    void disconnected(NntpCheckCon *con);


public slots:
    void onStartConnection();
    void onKillConnection();


    void onConnected();
    void onEncrypted();

    void onDisconnected();    //!< Handle disconnection

    void onReadyRead(); //!< To be overridden in Child class
    void onSslErrors(const QList<QSslError> &errors); //!< SSL errors handler
    void onErrors(QAbstractSocket::SocketError);      //!< Socket errors handler


private:
    void _closeConnection();
    void _checkNextArticle();
};

#endif // NNTPCHECKCON_H
