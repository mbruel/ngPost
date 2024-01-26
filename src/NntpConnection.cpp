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
#ifdef __USE_CONNECTION_TIMEOUT__
#include <QTimer>
#endif

NntpConnection::NntpConnection(NgPost *ngPost, int id, const NntpServerParams &srvParams):
    QObject(),
    _id(id), _srvParams(srvParams),
    _socket(nullptr), _isConnected(false),
    _logPrefix(QString("NntpCon #%1").arg(_id)),
    _postingState(PostingState::NOT_CONNECTED),
    _currentArticle(nullptr),
    _nbDisconnected(0),
    _ngPost(ngPost),
    _poster(nullptr)
#ifdef __USE_CONNECTION_TIMEOUT__
    ,_timeout(nullptr)
#endif
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
    qDebug() << "Destruction NntpConnection " << _logPrefix;
#endif
    if (_ngPost->debugMode())
        _log("Destructing connection..");

    // this should already have been triggered as the sockets lives in another thread
    if (_socket)
    {
        disconnect(_socket, &QAbstractSocket::disconnected, this, &NntpConnection::onDisconnected);
        disconnect(_socket, &QIODevice::readyRead,          this, &NntpConnection::onReadyRead);
        disconnect(_socket, SIGNAL(errorOccurred(QAbstractSocket::SocketError)),
                   this, SLOT(onErrors(QAbstractSocket::SocketError)));
        if (_srvParams.useSSL)
            disconnect(_socket, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(onSslErrors(QList<QSslError>)));

        _socket->disconnectFromHost();
        if (_socket->state() != QAbstractSocket::UnconnectedState)
            _socket->waitForDisconnected();
        deleteSocket();
    }
#ifdef __USE_CONNECTION_TIMEOUT__
    if (_timeout)
        delete _timeout;
#endif
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
    connect(_socket, SIGNAL(errorOccurred(QAbstractSocket::SocketError)),
            this, SLOT(onErrors(QAbstractSocket::SocketError)), Qt::DirectConnection);

    _socket->connectToHost(_srvParams.host, _srvParams.port);

#ifdef __USE_CONNECTION_TIMEOUT__
    if (!_timeout)
    {
        _timeout = new QTimer();
        connect(_timeout, &QTimer::timeout, this, &NntpConnection::onTimeout);
    }
    _timeout->start(_ngPost->getSocketTimeout());
#endif
}

void NntpConnection::onKillConnection()
{
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
    qDebug() << "[killConnection] #" << _id;
#endif
#ifdef __USE_CONNECTION_TIMEOUT__
    if (_timeout)
        _timeout->stop();
#endif

    if (_socket)
    {
        if (_ngPost->debugMode())
            _log("Killing connection..");

        disconnect(_socket, &QAbstractSocket::disconnected, this, &NntpConnection::onDisconnected);
        disconnect(_socket, &QIODevice::readyRead, this, &NntpConnection::onReadyRead);
        disconnect(_socket, SIGNAL(errorOccurred(QAbstractSocket::SocketError)),
                   this, SLOT(onErrors(QAbstractSocket::SocketError)));
        if (_srvParams.useSSL)
            disconnect(_socket, SIGNAL(sslErrors(QList<QSslError>)),
                       this, SLOT(onSslErrors(QList<QSslError>)));

        _socket->disconnectFromHost();
        if (_socket->state() != QAbstractSocket::UnconnectedState)
            _socket->waitForDisconnected();
        deleteSocket();

        // Coming from PostingJob::pause
        if (_currentArticle)
            _currentArticle->genNewId();
    }
}


void NntpConnection::_closeConnection(){
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
        _log("closeConnection");
#endif
        if (_ngPost->debugMode())
            _log("Closing connection...");
#ifdef __USE_CONNECTION_TIMEOUT__
    if (_timeout)
        _timeout->stop();
#endif
    if (_socket && _isConnected)
    {
        disconnect(_socket, &QIODevice::readyRead, this, &NntpConnection::onReadyRead);
        disconnect(_socket, SIGNAL(errorOccurred(QAbstractSocket::SocketError)),
                   this, SLOT(onErrors(QAbstractSocket::SocketError)));
        if (_srvParams.useSSL)
            disconnect(_socket, SIGNAL(sslErrors(QList<QSslError>)),
                       this, SLOT(onSslErrors(QList<QSslError>)));

        _socket->disconnectFromHost(); // we will end up in NntpConnect::onDisconnected
    }
    else // wrong host info or network down
    {
        _isConnected = false;
        if (_socket)        
            deleteSocket();

        if (_currentArticle && !_ngPost->tryResumePostWhenConnectionLost())
        {
#ifdef __DISP_ARTICLE_SERVER__
                if (_ngPost->debugMode())
                    _log(tr("Article FAIL2: %1 (on %2)").arg(_currentArticle->id()).arg(_srvParams.host));
#endif
#ifdef __RELEASE_ARTICLES_WHEN_CON_FAILS__
                _poster->releaseArticle(_logPrefix, _currentArticle);
#else
                emit _currentArticle->failed(_currentArticle->size());
#endif
            _currentArticle = nullptr;
        }
        emit disconnected(this);
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

        deleteSocket();
    }
    if (_poster->isPosting() && _postingState != PostingState::NO_MORE_FILES && _nbDisconnected++ < NntpArticle::nbMaxTrySending())
    {
        // Let's try to reconnect
        _error(tr("Connection lost, trying to reconnect! (nb disconnected: %1)").arg(_nbDisconnected));
        if (_currentArticle)
            _currentArticle->genNewId();

        emit startConnection();
    }
    else
    {
        if (_currentArticle)
        {
#ifdef __DISP_ARTICLE_SERVER__
            if (_ngPost->debugMode())
                _log(tr("Article FAIL3: %1 (on %2)").arg(_currentArticle->id()).arg(_srvParams.host));
#endif
#ifdef __RELEASE_ARTICLES_WHEN_CON_FAILS__
            _poster->releaseArticle(_logPrefix, _currentArticle);
#else
            emit _currentArticle->failed(_currentArticle->size());
#endif
            if (_ngPost->debugMode())
                _error(tr("Closing connection, Failed Article: %1").arg(_currentArticle->str()));
            _currentArticle = nullptr;
        }
        emit disconnected(this);
    }
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
    _closeConnection();
}



void NntpConnection::onErrors(QAbstractSocket::SocketError)
{
    _error(QString("Error Socket: %1").arg(_socket->errorString()));
    _closeConnection();
}

#ifdef __USE_CONNECTION_TIMEOUT__
void NntpConnection::onTimeout()
{
    _error(QString("Socket Timeout (%1 ms)").arg(_ngPost->getSocketTimeout()));
    _closeConnection();
}
#endif

void NntpConnection::onReadyRead()
{
    while (_isConnected && _socket->canReadLine())
    {
        QByteArray line = _socket->readLine();
#ifdef __USE_CONNECTION_TIMEOUT__
        if (_timeout)
            _timeout->start(_ngPost->getSocketTimeout());
#endif

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
//                if (++_nbErrors < NntpArticle::nbMaxTrySending())
//                {
//                    _socket->write(Nntp::POST);
//                    if (_ngPost->debugMode())
//                        _error(tr("ERROR on post command: %1").arg(line.constData()));
//                }
//                else
//                {
                    _postingState = PostingState::NOT_CONNECTED;
                    _error(tr("Closing Connection due to ERROR on post command: '%2' (%1 skipped)\n").arg(_currentArticle->str()).arg(line.constData()));
//                    emit _currentArticle->failed(_currentArticle->size());
                    _closeConnection();
//                }
            }
        }
        else if (_postingState == PostingState::WAITING_ANSWER)
        {
            if(strncmp(line.constData(), Nntp::getResponse(240), 3) == 0)
            {
                // Check if the server overwrite the Message-ID
                // 240 <5ed10f42$0$7342$f56682d5@speedium.nl> Article posted
                const char *lt= strchr(line.constData(), '<');
                if (lt)
                {
                    const char *gt= strchr(lt, '>');
                    if (gt)
                    {
                        line[static_cast<int>(gt - line.constData())] = '\0';
                        QString newMsgId(lt+1);
                        if (_ngPost->debugFull())
                            _log(QString("the server has overwritten the Message-ID to : %1 (article: %2)").arg(
                                     newMsgId).arg(_currentArticle->id()));
                        _currentArticle->overwriteMsgId(newMsgId);
                    }
                }
                _postingState = PostingState::IDLE;
#if defined(__DEBUG__) && defined(LOG_POSTING_STEPS)
                _log(tr("POSTED: %1").arg(_currentArticle->str()));
#endif
#ifdef __DISP_ARTICLE_SERVER__
                if (_ngPost->debugMode())
                    _log(tr("Article posted: %1 (on %2) %3").arg(_currentArticle->id()).arg(_srvParams.host).arg(line.constData()));
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
#ifdef __DISP_ARTICLE_SERVER__
                    if (_ngPost->debugMode())
                        _log(tr("Article FAIL: %1 (on %2) %3").arg(_currentArticle->id()).arg(_srvParams.host).arg(line.constData()));
#endif

#ifdef __RELEASE_ARTICLES_WHEN_CON_FAILS__
                    _poster->releaseArticle(_logPrefix, _currentArticle);
#else
                    emit _currentArticle->failed(_currentArticle->size());
#endif
                }

            }
            if (_postingState == PostingState::IDLE)
            {
                _currentArticle = nullptr;
                _sendNextArticle();
            }
        }
        else if (_postingState == PostingState::CONNECTED)
        {
            // Check welcome message
            if(strncmp(line.constData(), Nntp::getResponse(200), 3) != 0){
                QString err("Reading welcome message. Should start with 200... Server message: ");
                err += line.constData();
                if (_ngPost->debugMode())
                    _error(err);
//#if defined(__DEBUG__) && defined(LOG_CONNECTION_ERRORS_BEFORE_EMIT_SIGNALS)
//                _error(err);
//#endif
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
                if (_ngPost->debugMode())
                    _error(err);
//#if defined(__DEBUG__) && defined(LOG_CONNECTION_ERRORS_BEFORE_EMIT_SIGNALS)
//                _error(err);
//#endif
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
                if (_ngPost->debugMode())
                    _error(err);
//#if defined(__DEBUG__) && defined(LOG_CONNECTION_ERRORS_BEFORE_EMIT_SIGNALS)
//                _error(err);
//#endif
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
    if (!_currentArticle) // in case of error and reconnection, we repost the _currentArticle
        _currentArticle = _poster->getNextArticle(_logPrefix);

    if (_currentArticle)
    {
        _postingState = PostingState::SENDING_ARTICLE;
        if (_ngPost->debugFull())
            _log(tr("start sending article: %1").arg(_currentArticle->str()));
        _socket->write(Nntp::POST);
    }
    else
    {
        _postingState = PostingState::NO_MORE_FILES;
#ifdef __USE_CONNECTION_TIMEOUT__
        if (_timeout)
            _timeout->stop();
#endif
        if (_ngPost->debugMode())
            _log("No more articles");
        _closeConnection();
    }
}



void NntpConnection::setPoster(Poster *poster)
{
    _poster = poster;
    _logPrefix = QString("%1 {%2}").arg(poster->name(), _logPrefix);
}

QString NntpConnection::sslSupportInfo()
{
    return QString("SSL support: %1, build version: %2, system version: %3").arg(
                QSslSocket::supportsSsl() ? "yes" : "no",
                QSslSocket::sslLibraryBuildVersionString(),
                QSslSocket::sslLibraryVersionString());
}

bool NntpConnection::supportsSsl()
{
    return QSslSocket::supportsSsl();
}
