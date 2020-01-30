//========================================================================
//
// Copyright (C) 2019 Matthieu Bruel <Matthieu.Bruel@gmail.com>
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

#include "NntpConnection.h"
#include "nntp/NntpServerParams.h"
#include "nntp/Nntp.h"
#include "nntp/NntpArticle.h"
#include "nntp/NntpFile.h"
#include "NgPost.h"
#include "Poster.h"

#include <QByteArray>
#include <QSslSocket>
#include <QSslKey>
#include <QSslCertificate>
#include <QFile>
#include <QAbstractSocket>
#include <QSslCipher>

int NntpConnection::sSocketTimeoutMs = 5000;

NntpConnection::NntpConnection(NgPost *ngPost, int id, const NntpServerParams &srvParams):
    QObject(),
    _id(id), _srvParams(srvParams),
    _socket(nullptr), _isConnected(false),
    _logPrefix(QString("NntpCon #%1").arg(_id)),
    _postingState(PostingState::NOT_CONNECTED),
    _currentArticle(nullptr),
    _nbErrors(0),
    _ngPost(ngPost),
    _poster(nullptr)
{
#if defined(__DEBUG__) && defined(LOG_CONSTRUCTORS)
    qDebug() << QString("Creation %1 %2 ssl").arg(_logPrefix).arg(_srvParams.useSSL ? "with" : "no");
#endif

    connect(this, &NntpConnection::startConnection, this, &NntpConnection::onStartConnection, Qt::QueuedConnection);
    connect(this, &NntpConnection::killConnection,  this, &NntpConnection::onKillConnection,  Qt::QueuedConnection);
}


NntpConnection::~NntpConnection()
{
#if defined(__DEBUG__) && defined(LOG_CONSTRUCTORS)
    qDebug() << "Destruction " << _logPrefix;
#endif
    _closeConnection(); // this should already have been triggered as the sockets lives in another thread
}


void NntpConnection::onStartConnection()
{
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
    _log("Starting connection...");
#endif
    if (_srvParams.useSSL)
        _socket = new QSslSocket();
    else
        _socket = new QTcpSocket();

    _socket->setSocketOption(QAbstractSocket::KeepAliveOption, true);
    _socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    _socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, NgPost::articleSize());

    connect(_socket, &QAbstractSocket::connected,    this, &NntpConnection::onConnected,    Qt::DirectConnection);
    connect(_socket, &QAbstractSocket::disconnected, this, &NntpConnection::onDisconnected, Qt::DirectConnection);
    connect(_socket, &QIODevice::readyRead,          this, &NntpConnection::onReadyRead,    Qt::DirectConnection);

    qRegisterMetaType<QAbstractSocket::SocketError>("SocketError" );
    connect(_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onErrors(QAbstractSocket::SocketError)), Qt::DirectConnection);

    _socket->connectToHost(_srvParams.host, _srvParams.port);

    if (_ngPost->debugMode())
        _log(QString("SSL support: %1, build version: %2, system version: %3").arg(QSslSocket::supportsSsl() ? "yes" : "no").arg(
                 QSslSocket::sslLibraryBuildVersionString()).arg(QSslSocket::sslLibraryVersionString()));

}

void NntpConnection::onKillConnection()
{
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
    qDebug() << "[killConnection] #" << _id;
#endif
    if (_socket)
    {
        _closeConnection();
        _socket->deleteLater();
        _socket = nullptr;
    }
}


void NntpConnection::_closeConnection(){
    if (_socket && _socket->isOpen())
    {
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
        _log("closeConnection");
#endif
        disconnect(_socket, &QIODevice::readyRead, this, &NntpConnection::onReadyRead);
        _socket->disconnectFromHost();
    }
}


void NntpConnection::onDisconnected()
{
    if (_socket)
    {
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
        _error("> disconnected");
#endif
        _isConnected    = false;

        _socket->deleteLater();
        _socket = nullptr;
    }
    emit disconnected(this);
}

void NntpConnection::onConnected()
{
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
    _log("> connected to server");
#endif
    _isConnected = true;
    if (_srvParams.useSSL)
    {
        QSslSocket *sslSock = static_cast<QSslSocket*>(_socket);
        connect(sslSock, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(onSslErrors(QList<QSslError>)), Qt::DirectConnection);

        connect(sslSock, &QSslSocket::encrypted, this, &NntpConnection::onEncrypted, Qt::DirectConnection);
        emit sslSock->startClientEncryption();
    }
    else
    {
        _postingState = PostingState::CONNECTED;
        // We should receive the Hello Message
    }
}

void NntpConnection::onEncrypted()
{
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
    _log("> SSL handshake succeed");
#endif

    _postingState = PostingState::CONNECTED;
    // We should receive the Hello Message
}


void NntpConnection::onSslErrors(const QList<QSslError> &errors)
{
    QString err("Error SSL Socket:\n");
    for(int i = 0 ; i< errors.size() ; ++i)
        err += QString("\t- %1\n").arg(errors[i].errorString());

    _error(err);
    if (_currentArticle)
        emit _currentArticle->failed(_currentArticle->size());

    emit killConnection();
    if (++_nbErrors < NntpArticle::nbMaxTrySending())
        emit startConnection();
}



void NntpConnection::onErrors(QAbstractSocket::SocketError)
{
    _error(QString("Error Socket: %1").arg(_socket->errorString()));
    if (_currentArticle)
        emit _currentArticle->failed(_currentArticle->size());

    emit killConnection();
    if (++_nbErrors < NntpArticle::nbMaxTrySending())
        emit startConnection();
}


void NntpConnection::onReadyRead()
{
    while (_isConnected && _socket->canReadLine())
    {
        QByteArray line = _socket->readLine();

#if defined(__DEBUG__) && defined(LOG_NEWS_DATA)
        _log(QString("Data In: %1").arg(line.constData()));
#endif
        if (_postingState == PostingState::SENDING_ARTICLE)
        {
#if defined(__DEBUG__) && defined(LOG_POSTING_STEPS)
            _log(QString("post response: %1").arg(line.constData()));
#endif

            if(strncmp(line.constData(), Nntp::getResponse(340), 3) == 0)
            {
                _postingState = PostingState::WAITING_ANSWER;
                _currentArticle->write(this, _ngPost->aticleSignature()); // This will be done async
                if (_ngPost->dispPostingFile() && _currentArticle->isFirstArticle())
                    emit _currentArticle->nntpFile()->startPosting();
            }
            else
            {
                if (++_nbErrors < NntpArticle::nbMaxTrySending())
                {
                    _socket->write(Nntp::POST);
                    if (_ngPost->debugMode())
                        _error(tr("ERROR on post command: %1").arg(line.constData()));
                }
                else
                {
                    _postingState = PostingState::NOT_CONNECTED;
                    _error(tr("Closing Connection due to ERROR on post command: '%2' (%1 skipped)\n").arg(_currentArticle->str()).arg(line.constData()));
                    emit _currentArticle->failed(_currentArticle->size());
                    emit killConnection();
                }
            }
        }
        else if (_postingState == PostingState::WAITING_ANSWER)
        {
            if(strncmp(line.constData(), Nntp::getResponse(240), 3) == 0)
            {
                _postingState = PostingState::IDLE;
#if defined(__DEBUG__) && defined(LOG_POSTING_STEPS)
                _log(tr("POSTED: %1").arg(_currentArticle->str()));
#endif
                emit _currentArticle->posted(_currentArticle->size());
            }
            else
            {
#if defined(__DEBUG__) && defined(LOG_POSTING_STEPS)
                _error(tr("Error on posting article %1: %2").arg(_currentArticle->id()).arg(
                         line.constData()));
#endif
                if (_currentArticle->tryResend())
                {
                    _postingState = PostingState::SENDING_ARTICLE;
                    _socket->write(Nntp::POST);
                    if (_ngPost->debugMode())
                        _log(tr("ReTry %1 (Error: '%2')").arg(_currentArticle->str()).arg(line.constData()));
                }
                else
                {
                    _postingState = PostingState::IDLE;
                    _error(tr("FAIL posting %1 (Error: '%2')").arg(_currentArticle->str()).arg(line.constData()));
                    emit _currentArticle->failed(_currentArticle->size());
                }

            }
            if (_postingState == PostingState::IDLE)
                _sendNextArticle();
        }
        else if (_postingState == PostingState::CONNECTED)
        {
            // Check welcome message
            if(strncmp(line.constData(), Nntp::getResponse(200), 3) != 0){
                QString err("Reading welcome message. Should start with 200... Server message: ");
                err += line.constData();
#if defined(__DEBUG__) && defined(LOG_CONNECTION_ERRORS_BEFORE_EMIT_SIGNALS)
                _error(err);
#endif
                emit errorConnecting(tr("[Connection #%1] Error connecting to server %2:%3").arg(
                                         _id).arg(_srvParams.host).arg(_srvParams.port));
                _closeConnection();
            }
            else
            {
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
                _log("> received Hello Message");
#endif

                // Start authentication : send user info
                if (_srvParams.user.empty())
                {
                    _postingState = PostingState::IDLE;
                    _sendNextArticle();
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
                QString err("Wrong Authentication: response from '");
                err += Nntp::AUTHINFO_USER;
                err += "' should start with 38... resp: ";
                err += line.constData();
#if defined(__DEBUG__) && defined(LOG_CONNECTION_ERRORS_BEFORE_EMIT_SIGNALS)
                _error(err);
#endif
                emit errorConnecting(tr("[Connection #%1] Error sending user '%4' to server %2:%3").arg(
                                         _id).arg(_srvParams.host).arg(_srvParams.port).arg(_srvParams.user.c_str()));
                _closeConnection();
            }
            else
            {
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
                _log("> AUTHINFO_USER succeed");
#endif

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
                QString err("Wrong Authentication: response from '");
                err += Nntp::AUTHINFO_PASS;
                err += "' should start with 28... resp: ";
                err += line.constData();
#if defined(__DEBUG__) && defined(LOG_CONNECTION_ERRORS_BEFORE_EMIT_SIGNALS)
                _error(err);
#endif
                emit errorConnecting(tr("[Connection #%1] Error authentication to server %2:%3 with user '%4' and pass '%5'").arg(
                                         _id).arg(_srvParams.host).arg(_srvParams.port).arg(
                                         _srvParams.user.c_str()).arg(_srvParams.pass.c_str()));
                _closeConnection();
            }
            else
            {
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
                _log("> AUTHINFO_PASS succeed => ready to POST \\o/");
#endif
                _postingState = PostingState::IDLE;
                _sendNextArticle();
            }
        }
    }
}


void NntpConnection::_sendNextArticle()
{
    _currentArticle = _poster->getNextArticle(_logPrefix);
    if (_currentArticle)
    {
        _postingState = PostingState::SENDING_ARTICLE;
#if defined(__DEBUG__) && defined(LOG_POSTING_STEPS)
        _log(tr("start sending article: %1").arg(_currentArticle->id()));
#endif
        _socket->write(Nntp::POST);
    }
    else
        _postingState = PostingState::IDLE;
}



void NntpConnection::setPoster(Poster *poster)
{
    _poster = poster;
    _logPrefix = QString("%1 {%2}").arg(poster->name()).arg(_logPrefix);
}
