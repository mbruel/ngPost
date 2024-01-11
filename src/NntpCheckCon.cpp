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

#include "NntpCheckCon.h"
#include "NzbCheck.h"
#include "nntp/Nntp.h"
#include <QSslSocket>

NntpCheckCon::NntpCheckCon(NzbCheck *nzbCheck, int id, const NntpServerParams &srvParams)
    : QObject(),
      _nzbCheck(nzbCheck), _id(id), _srvParams(srvParams),
      _socket(nullptr), _isConnected(false),
      _postingState(PostingState::NOT_CONNECTED),
      _currentArticle()
{
    connect(this, &NntpCheckCon::startConnection, this, &NntpCheckCon::onStartConnection, Qt::QueuedConnection);
    connect(this, &NntpCheckCon::killConnection,  this, &NntpCheckCon::onKillConnection,  Qt::QueuedConnection);
}

NntpCheckCon::~NntpCheckCon()
{
    if (_socket)
    {
        disconnect(_socket, &QAbstractSocket::disconnected, this, &NntpCheckCon::onDisconnected);
        disconnect(_socket, &QIODevice::readyRead,          this, &NntpCheckCon::onReadyRead);
        _socket->disconnectFromHost();
        if (_socket->state() != QAbstractSocket::UnconnectedState)
            _socket->waitForDisconnected();
        _socket->deleteLater();
    }
}

void NntpCheckCon::onStartConnection()
{
    if (_srvParams.useSSL)
        _socket = new QSslSocket();
    else
        _socket = new QTcpSocket();

    _socket->setSocketOption(QAbstractSocket::KeepAliveOption, true);
    _socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    connect(_socket, &QAbstractSocket::connected,    this, &NntpCheckCon::onConnected,    Qt::DirectConnection);
    connect(_socket, &QAbstractSocket::disconnected, this, &NntpCheckCon::onDisconnected, Qt::DirectConnection);
    connect(_socket, &QIODevice::readyRead,          this, &NntpCheckCon::onReadyRead,    Qt::DirectConnection);

    qRegisterMetaType<QAbstractSocket::SocketError>("SocketError" );
    connect(_socket, SIGNAL(errorOccurred(QAbstractSocket::SocketError)),
            this, SLOT(onErrors(QAbstractSocket::SocketError)), Qt::DirectConnection);

    _socket->connectToHost(_srvParams.host, _srvParams.port);
}

void NntpCheckCon::onKillConnection()
{
    if (_socket)
    {
        disconnect(_socket, &QIODevice::readyRead,          this, &NntpCheckCon::onReadyRead);
        disconnect(_socket, &QAbstractSocket::disconnected, this, &NntpCheckCon::onDisconnected);
        _socket->disconnectFromHost();
        if (_socket->state() != QAbstractSocket::UnconnectedState)
            _socket->waitForDisconnected();
        _socket->deleteLater();
        _socket = nullptr;
    }
}

void NntpCheckCon::onConnected()
{
    _isConnected = true;
    if (_srvParams.useSSL)
    {
        QSslSocket *sslSock = static_cast<QSslSocket*>(_socket);
        connect(sslSock, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(onSslErrors(QList<QSslError>)), Qt::DirectConnection);

        connect(sslSock, &QSslSocket::encrypted, this, &NntpCheckCon::onEncrypted, Qt::DirectConnection);
        emit sslSock->startClientEncryption();
    }
    else
    {
        if (_nzbCheck->debugMode())
            _nzbCheck->log(tr("[Con #%1] Connected").arg(_id));

        _postingState = PostingState::CONNECTED;
        // We should receive the Hello Message
    }
}

void NntpCheckCon::onEncrypted()
{
    if (_nzbCheck->debugMode())
        _nzbCheck->log(tr("[Con #%1] Connected").arg(_id));

    _postingState = PostingState::CONNECTED;
    // We should receive the Hello Message
}

void NntpCheckCon::onDisconnected()
{
    if (_socket)
    {
        _isConnected    = false;
        _socket->deleteLater();
        _socket = nullptr;
    }
    emit disconnected(this);
}

void NntpCheckCon::onReadyRead()
{
    while (_isConnected && _socket->canReadLine())
    {
        QByteArray line = _socket->readLine();
//        qDebug() << "line: " << line.constData();

        if (_postingState == PostingState::CHECKING_ARTICLE)
        {
            if(strncmp(line.constData(), Nntp::getResponse(430), 3) == 0)
                _nzbCheck->missingArticle(_currentArticle);

            _nzbCheck->articleChecked();
            _postingState = PostingState::IDLE;
            _checkNextArticle();
        }
        else if (_postingState == PostingState::CONNECTED)
        {
            // Check welcome message
            if(strncmp(line.constData(), Nntp::getResponse(200), 3) != 0){
                emit errorConnecting(tr("[Connection #%1] Error connecting to server %2:%3").arg(
                                         _id).arg(_srvParams.host).arg(_srvParams.port));
                _closeConnection();
            }
            else
            {
                // Start authentication : send user info
                if (_srvParams.user.empty())
                {
                    _postingState = PostingState::IDLE;
                    _checkNextArticle();
                }
                else
                {
                    _postingState = PostingState::AUTH_USER;

                    std::string cmd(Nntp::AUTHINFO_USER);
                    cmd += _srvParams.user;
                    cmd += Nntp::ENDLINE;
                    _socket->write(cmd.c_str());
                }
            }
        }
        else if (_postingState == PostingState::AUTH_USER)
        {
            // validate the reply
            if(strncmp(line.constData(), Nntp::getResponse(381), 2) != 0){
                emit errorConnecting(tr("[Connection #%1] Error sending user '%4' to server %2:%3").arg(
                                         _id).arg(_srvParams.host).arg(_srvParams.port).arg(_srvParams.user.c_str()));
                _closeConnection();
            }
            else
            {
                // Continue authentication : send pass info
                _postingState = PostingState::AUTH_PASS;

                std::string cmd(Nntp::AUTHINFO_PASS);
                cmd += _srvParams.pass;
                cmd += Nntp::ENDLINE;
                _socket->write(cmd.c_str());
            }
        }
        else if (_postingState == PostingState::AUTH_PASS)
        {
            if(strncmp(line.constData(), Nntp::getResponse(281), 2) != 0){
                emit errorConnecting(tr("[Connection #%1] Error authentication to server %2:%3 with user '%4' and pass '%5'").arg(
                                         _id).arg(_srvParams.host).arg(_srvParams.port).arg(
                                         _srvParams.user.c_str()).arg(_srvParams.pass.c_str()));
                _closeConnection();
            }
            else
            {
                _postingState = PostingState::IDLE;
                _checkNextArticle();
            }
        }
    }
}

void NntpCheckCon::onSslErrors(const QList<QSslError> &errors)
{
    QString err("Error SSL Socket:\n");
    for(int i = 0 ; i< errors.size() ; ++i)
        err += QString("\t- %1\n").arg(errors[i].errorString());
    _nzbCheck->error(err);
    _closeConnection();

}

void NntpCheckCon::onErrors(QAbstractSocket::SocketError)
{
    _nzbCheck->error(QString("Error Socket: %1").arg(_socket->errorString()));
    _closeConnection();
}

void NntpCheckCon::_closeConnection()
{
    if (_socket && _isConnected)
    {
        disconnect(_socket, &QIODevice::readyRead, this, &NntpCheckCon::onReadyRead);
        _socket->disconnectFromHost();
    }
    else // wrong host info or network down
    {
        if (_socket)
            _socket->deleteLater();
        _socket = nullptr;
        emit disconnected(this);
    }
}

void NntpCheckCon::_checkNextArticle()
{
    _currentArticle = _nzbCheck->getNextArticle();

    if (!_currentArticle.isNull())
    {
        if (_nzbCheck->debugMode())
            _nzbCheck->log(tr("[Con #%1] Checking article %2").arg(_id).arg(_currentArticle));

        _postingState = PostingState::CHECKING_ARTICLE;
        _socket->write(QString("%1 %2\r\n").arg(Nntp::STAT).arg(_currentArticle).toLocal8Bit());
    }
    else
    {
        if (_nzbCheck->debugMode())
            _nzbCheck->log(tr("[Con #%1] No more Article").arg(_id));

        _postingState = PostingState::IDLE;
        _closeConnection();
    }
}
