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
#include "NgConf.h"
#include "NgPost.h"
#include "nntp/NntpArticle.h"
#include "nntp/NntpFile.h"
#include "nntp/rfc.h"
#include "nntp/ServerParams.h"
#include "Poster.h"

#include <QAbstractSocket>
#include <QByteArray>
#include <QFile>
#include <QSslCertificate>
#include <QSslCipher>
#include <QSslKey>
#include <QSslSocket>
#ifdef __USE_CONNECTION_TIMEOUT__
#  include <QTimer>
#endif

using namespace NNTP;

NntpConnection *NntpConnection::createNntpConnection(NgPost const &ngPost, const ushort id)
{
    if (ngPost.postingParams()->nntpServers().isEmpty())
        return nullptr;

    NNTP::ServerParams *srvParams = ngPost.postingParams()->nntpServers().front();
    return new NntpConnection(ngPost, id, *srvParams);
}

NntpConnection::NntpConnection(NgPost const &ngPost, ushort id, NNTP::ServerParams const &srvParams)
    : QObject()
    , _id(id)
    , _srvParams(srvParams)
    , _socket(nullptr)
    , _isConnected(false)
    , _logPrefix(QString("NntpCon #%1").arg(_id))
    , _postingState(PostingState::NOT_CONNECTED)
    , _currentArticle(nullptr)
    , _nbDisconnected(0)
    , _ngPost(ngPost)
    , _poster(nullptr)
    , _postingNotAllowed(false)
#ifdef __USE_CONNECTION_TIMEOUT__
    , _timeout(nullptr)
#endif
{
#if defined(__DEBUG__) && defined(LOG_CONSTRUCTORS)
    NgLogger::log(tr("Creation %1 %2 ssl").arg(_logPrefix).arg(_srvParams.useSSL ? "with" : "no"),
                  true,
                  NgLogger::DebugLevel::DebugBold);
#endif

    connect(this,
            &NntpConnection::sigStartConnection,
            this,
            &NntpConnection::onStartConnection,
            Qt::QueuedConnection);
    connect(this,
            &NntpConnection::sigKillConnection,
            this,
            &NntpConnection::onKillConnection,
            Qt::QueuedConnection);

    connect(this,
            &NntpConnection::sigScheduleDeleteLaterInOwnThread,
            this,
            &QObject::deleteLater,
            Qt::QueuedConnection);

    connect(this,
            &NntpConnection::sigLog,
            [](QString const &msg, bool newline, NgLogger::DebugLevel debugLvl)
            { NgLogger::log(msg, newline, debugLvl); });
    connect(this, &NntpConnection::sigError, qOverload<QString>(&NgLogger::error));
    connect(this, &NntpConnection::sigErrorConnecting, qOverload<QString>(&NgLogger::error));
}

NntpConnection::~NntpConnection()
{
#if defined(__DEBUG__) && defined(LOG_CONSTRUCTORS)
    qDebug() << "Destruction " << _logPrefix << " from thread: " << QThread::currentThread()->objectName();
#endif
    _log("Destructing connection..", NgLogger::DebugLevel::Debug);

    // this should already have been triggered as the sockets lives in another thread
    if (_socket)
    {
        disconnect(_socket, &QAbstractSocket::disconnected, this, &NntpConnection::onDisconnected);
        disconnect(_socket, &QIODevice::readyRead, this, &NntpConnection::onReadyRead);
        disconnect(_socket,
                   SIGNAL(errorOccurred(QAbstractSocket::SocketError)),
                   this,
                   SLOT(onErrors(QAbstractSocket::SocketError)));
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
    _socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, NgConf::kArticleSize);

    connect(_socket, &QAbstractSocket::connected, this, &NntpConnection::onConnected, Qt::DirectConnection);
    connect(_socket,
            &QAbstractSocket::disconnected,
            this,
            &NntpConnection::onDisconnected,
            Qt::DirectConnection);
    connect(_socket, &QIODevice::readyRead, this, &NntpConnection::onReadyRead, Qt::DirectConnection);

    qRegisterMetaType<QAbstractSocket::SocketError>("SocketError");
    connect(_socket,
            SIGNAL(errorOccurred(QAbstractSocket::SocketError)),
            this,
            SLOT(onErrors(QAbstractSocket::SocketError)),
            Qt::DirectConnection);

    _socket->connectToHost(_srvParams.host, _srvParams.port);

#ifdef __USE_CONNECTION_TIMEOUT__
    if (!_timeout)
    {
        _timeout = new QTimer();
        connect(_timeout, &QTimer::timeout, this, &NntpConnection::onTimeout);
    }
    _timeout->start(_ngPost.getSocketTimeout());
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
        _log("Killing connection..", NgLogger::DebugLevel::Debug);
        disconnect(_socket, &QAbstractSocket::disconnected, this, &NntpConnection::onDisconnected);
        disconnect(_socket, &QIODevice::readyRead, this, &NntpConnection::onReadyRead);
        disconnect(_socket,
                   SIGNAL(errorOccurred(QAbstractSocket::SocketError)),
                   this,
                   SLOT(onErrors(QAbstractSocket::SocketError)));
        if (_srvParams.useSSL)
            disconnect(_socket, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(onSslErrors(QList<QSslError>)));

        _socket->disconnectFromHost();
        if (_socket->state() != QAbstractSocket::UnconnectedState)
            _socket->waitForDisconnected();
        deleteSocket();

        // Coming from PostingJob::pause
        if (_currentArticle)
            _currentArticle->genNewId();
    }

#ifdef __test_ngPost__
    emit sigStopTest();
#endif
}

void NntpConnection::_closeConnection()
{
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
    _log("closeConnection");
#endif
    _log("Closing connection...", NgLogger::DebugLevel::Debug);
#ifdef __USE_CONNECTION_TIMEOUT__
    if (_timeout)
        _timeout->stop();
#endif
    if (_socket && _isConnected)
    {
        disconnect(_socket, &QIODevice::readyRead, this, &NntpConnection::onReadyRead);
        disconnect(_socket,
                   SIGNAL(errorOccurred(QAbstractSocket::SocketError)),
                   this,
                   SLOT(onErrors(QAbstractSocket::SocketError)));
        if (_srvParams.useSSL)
            disconnect(_socket, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(onSslErrors(QList<QSslError>)));

        _socket->disconnectFromHost(); // we will end up in NntpConnect::onDisconnected
    }
    else // wrong host info or network down
    {
        _isConnected = false;
        if (_socket)
            deleteSocket();

        if (_currentArticle && !_poster->tryResumePostWhenConnectionLost())
        {
#ifdef __DISP_ARTICLE_SERVER__
            if (NgLogger::isDebugMode())
                _error(tr("Article FAIL2: %1 (on %2)").arg(_currentArticle->id()).arg(_srvParams.host));
#endif
#ifdef __RELEASE_ARTICLES_WHEN_CON_FAILS__
            _poster->releaseArticle(_logPrefix, _currentArticle);
#else
            emit _currentArticle->failed(_currentArticle->size());
#endif
            _currentArticle = nullptr;
        }
        emit sigDisconnected(this);
    }
}

void NntpConnection::onDisconnected()
{
    if (_socket)
    {
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
        if (!hasNoMoreFiles())
            _error("> onDisconnected");
#endif
        _isConnected = false;

        deleteSocket();
    }
    if (_poster->isPosting() && _postingState != PostingState::NO_MORE_FILES
        && _nbDisconnected++ < NNTP::Article::nbMaxTrySending())
    {
        // Let's try to reconnect
        _error(tr("Connection lost, trying to reconnect! (nb disconnected: %1)").arg(_nbDisconnected));
        if (_currentArticle)
            _currentArticle->genNewId();

        emit sigStartConnection();
    }
    else
    {
        if (_currentArticle)
        {
#ifdef __DISP_ARTICLE_SERVER__
            if (NgLogger::isDebugMode())
                _error(tr("Article FAIL3: %1 (on %2)").arg(_currentArticle->id()).arg(_srvParams.host));
#endif
#ifdef __RELEASE_ARTICLES_WHEN_CON_FAILS__
            _poster->releaseArticle(_logPrefix, _currentArticle);
#else
            emit _currentArticle->failed(_currentArticle->size());
#endif
            if (NgLogger::isDebugMode())
                _error(tr("Closing connection, Failed Article: %1").arg(_currentArticle->str()));
            _currentArticle = nullptr;
        }
        emit sigDisconnected(this);
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
        QSslSocket *sslSock = static_cast<QSslSocket *>(_socket);
        connect(sslSock,
                SIGNAL(sslErrors(QList<QSslError>)),
                this,
                SLOT(onSslErrors(QList<QSslError>)),
                Qt::DirectConnection);

        connect(sslSock, &QSslSocket::encrypted, this, &NntpConnection::onEncrypted, Qt::DirectConnection);
        emit sslSock->startClientEncryption();
    }
    else
    {
        _postingState = PostingState::CONNECTED;
        // We should receive the Hello Message

#ifdef __test_ngPost__
        emit sigConnected();
#endif
    }
}

void NntpConnection::onEncrypted()
{
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
    _log("> SSL handshake succeed");
#endif

    _postingState = PostingState::CONNECTED;
    // We should receive the Hello Message

#ifdef __test_ngPost__
    emit sigConnected();
#endif
}

void NntpConnection::onSslErrors(QList<QSslError> const &errors)
{
    QString err("Error SSL Socket:\n");
    for (int i = 0; i < errors.size(); ++i)
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
    _error(QString("Socket Timeout (%1 ms)").arg(_ngPost.getSocketTimeout()));
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
            _timeout->start(_ngPost.getSocketTimeout());
#endif

#if defined(__DEBUG__) && defined(LOG_NEWS_DATA)
        _log(QString("Data In: %1").arg(line.constData()));
#endif
        if (_postingState == PostingState::SENDING_ARTICLE)
        {
#if defined(__DEBUG__) && defined(LOG_POSTING_STEPS)
            _log(QString("post response: %1").arg(line.constData()));
#endif

            if (strncmp(line.constData(), RFC::getResponse(RFC::RespCode::SEND_ARTICLE), 3) == 0)
            {
                _postingState = PostingState::WAITING_ANSWER;
                _currentArticle->write(this, NgConf::kArticleIdSignature); // This will be done async
                if (_ngPost.dispPostingFile() && _currentArticle->isFirstArticle())
                    emit _currentArticle->nntpFile()->sigStartPosting();
            }
            else
            {
                //                if (++_nbErrors < NNTP::Article::nbMaxTrySending())
                //                {
                //                    _socket->write(RFC::POST);
                //                    if (NgLogger::isDebugMode())
                //                        _error(tr("ERROR on post command: %1").arg(line.constData()));
                //                }
                //                else
                //                {
                _postingState = PostingState::NOT_CONNECTED;
                if (strncmp(line.constData(), RFC::getResponse(RFC::RespCode::POST_NOT_ALLOWED), 3) == 0)
                {
                    _postingNotAllowed = true;
                    emit sigPostingNotAllowed(this);
                }
                _error(tr("Closing Connection due to ERROR on post command: '%2' (%1 skipped)\n")
                               .arg(_currentArticle->str())
                               .arg(line.constData()));
                //                    emit _currentArticle->failed(_currentArticle->size());
                _closeConnection();
                //                }
            }
        }
        else if (_postingState == PostingState::WAITING_ANSWER)
        {
            if (strncmp(line.constData(), RFC::getResponse(RFC::RespCode::POST_OK), 3) == 0)
            {
                // Check if the server overwrite the Message-ID
                // 240 <5ed10f42$0$7342$f56682d5@speedium.nl> Article posted
                char const *lt = strchr(line.constData(), '<');
                if (lt)
                {
                    char const *gt = strchr(lt, '>');
                    if (gt)
                    {
                        line[static_cast<int>(gt - line.constData())] = '\0';
                        QString newMsgId(lt + 1);
                        if (NgLogger::isFullDebug())
                            _log(QString("the server has overwritten the Message-ID to : %1 (article: %2)")
                                         .arg(newMsgId)
                                         .arg(_currentArticle->id()));
                        _currentArticle->overwriteMsgId(newMsgId);
                    }
                }
                _postingState = PostingState::IDLE;
#if defined(__DEBUG__) && defined(LOG_POSTING_STEPS)
                _log(tr("POSTED: %1").arg(_currentArticle->str()));
#endif
#ifdef __DISP_ARTICLE_SERVER__
                _log(tr("Article posted: %1 (on %2) %3")
                             .arg(_currentArticle->id())
                             .arg(_srvParams.host)
                             .arg(line.constData()),
                     NgLogger::DebugLevel::Debug);
#endif
                emit _currentArticle->sigPosted(_currentArticle->size());
            }
            else
            {
#if defined(__DEBUG__) && defined(LOG_POSTING_STEPS)
                _error(tr("Error on posting article %1: %2").arg(_currentArticle->id()).arg(line.constData()));
#endif
                if (_currentArticle->tryResend())
                {
                    _postingState = PostingState::SENDING_ARTICLE;
                    _socket->write(RFC::POST);
                    _log(tr("ReTry %1 (Error: '%2')").arg(_currentArticle->str()).arg(line.constData()),
                         NgLogger::DebugLevel::Debug);
                }
                else
                {
                    _postingState = PostingState::IDLE;
                    _error(tr("FAIL posting %1 (Error: '%2')")
                                   .arg(_currentArticle->str())
                                   .arg(line.constData()));
#ifdef __DISP_ARTICLE_SERVER__
                    _log(tr("Article FAIL: %1 (on %2) %3")
                                 .arg(_currentArticle->id())
                                 .arg(_srvParams.host)
                                 .arg(line.constData()),
                         NgLogger::DebugLevel::Debug);
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
            if (strncmp(line.constData(), RFC::getResponse(RFC::RespCode::SERV_READY), 3) != 0)
            {
                QString err("Reading welcome message. Should start with 200... Server message: ");
                err += line.constData();
                if (NgLogger::isDebugMode())
                    _error(err);
                // #if defined(__DEBUG__) && defined(LOG_CONNECTION_ERRORS_BEFORE_EMIT_SIGNALS)
                //                 _error(err);
                // #endif
                emit sigErrorConnecting(tr("[Connection #%1] Error connecting to server %2:%3")
                                                .arg(_id)
                                                .arg(_srvParams.host)
                                                .arg(_srvParams.port));
                _closeConnection();
            }
            else
            {
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
                _log("> received Hello Message");
#endif
#ifdef __test_ngPost__
                emit sigHelloReceived();
#endif
                // Start authentication : send user info
                if (_srvParams.user.empty())
                {
                    _postingState = PostingState::IDLE;

#ifndef __test_ngPost__
                    _sendNextArticle();
#endif
                }
                else
                {
                    _postingState = PostingState::AUTH_USER;

                    std::string cmd(RFC::AUTHINFO_USER);
                    cmd += _srvParams.user;
                    cmd += RFC::ENDLINE;
                    _socket->write(cmd.c_str());
                }
            }
        }
        else if (_postingState == PostingState::AUTH_USER)
        {
            // validate the reply
            if (strncmp(line.constData(), RFC::getResponse(RFC::RespCode::PASS_NEEDED), 2) != 0)
            {
                QString err("Wrong Authentication: response from '");
                err += RFC::AUTHINFO_USER;
                err += "' should start with 38... resp: ";
                err += line.constData();
                if (NgLogger::isDebugMode())
                    _error(err);
                // #if defined(__DEBUG__) && defined(LOG_CONNECTION_ERRORS_BEFORE_EMIT_SIGNALS)
                //                 _error(err);
                // #endif
                emit sigErrorConnecting(tr("[Connection #%1] Error sending user '%4' to server %2:%3")
                                                .arg(_id)
                                                .arg(_srvParams.host)
                                                .arg(_srvParams.port)
                                                .arg(_srvParams.user.c_str()));
                _closeConnection();
            }
            else
            {
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
                _log("> AUTHINFO_USER succeed");
#endif

                // Continue authentication : send pass info
                _postingState = PostingState::AUTH_PASS;

                std::string cmd(RFC::AUTHINFO_PASS);
                cmd += _srvParams.pass;
                cmd += RFC::ENDLINE;
                _socket->write(cmd.c_str());
            }
        }
        else if (_postingState == PostingState::AUTH_PASS)
        {
            if (strncmp(line.constData(), RFC::getResponse(RFC::RespCode::AUTH_OK), 2) != 0)
            {
                QString err("Wrong Authentication: response from '");
                err += RFC::AUTHINFO_PASS;
                err += "' should start with 28... resp: ";
                err += line.constData();
                if (NgLogger::isDebugMode())
                    _error(err);
                // #if defined(__DEBUG__) && defined(LOG_CONNECTION_ERRORS_BEFORE_EMIT_SIGNALS)
                //                 _error(err);
                // #endif
                emit sigErrorConnecting(
                        tr("[Connection #%1] Error authentication to server %2:%3 with user '%4' and pass '%5'")
                                .arg(_id)
                                .arg(_srvParams.host)
                                .arg(_srvParams.port)
                                .arg(_srvParams.user.c_str())
                                .arg(_srvParams.pass.c_str()));
                _closeConnection();
            }
            else
            {
#if defined(__DEBUG__) && defined(LOG_CONNECTION_STEPS)
                _log("> AUTHINFO_PASS succeed => ready to POST \\o/");
#endif
                _postingState = PostingState::IDLE;

#ifdef __test_ngPost__
                emit sigAuthenticated();
                if (sTestShouldSendFirstArticle)
                    _sendNextArticle();
#else
                _sendNextArticle();
#endif
            }
        }
    }
}
// void NntpConnection::testSenddArticle(Article const &article);
#ifdef __test_ngPost__

#endif
void NntpConnection::_sendNextArticle()
{
#ifdef __test_ngPost__
    if (!_poster)
    {
        _postingState = PostingState::NO_MORE_FILES;
        _log("No Poster => no articles");
        _closeConnection();
    }
#endif
    if (!_currentArticle) // in case of error and reconnection, we repost the _currentArticle
        _currentArticle = _poster->getNextArticle(_logPrefix);

    if (_currentArticle)
    {
        _postingState = PostingState::SENDING_ARTICLE;
        if (NgLogger::isFullDebug())
            _log(tr("start sending article: %1").arg(_currentArticle->str()));
        _socket->write(RFC::POST);
    }
    else
    {
        _postingState = PostingState::NO_MORE_FILES;
#ifdef __USE_CONNECTION_TIMEOUT__
        if (_timeout)
            _timeout->stop();
#endif
        _log("No more articles", NgLogger::DebugLevel::Debug);
        _closeConnection();
    }
}

void NntpConnection::setPoster(Poster *poster)
{
    _poster    = poster;
    _logPrefix = QString("%1 {%2}").arg(poster->name(), _logPrefix);
}

QString NntpConnection::sslSupportInfo()
{
    return QString("SSL support: %1, build version: %2, system version: %3")
            .arg(QSslSocket::supportsSsl() ? "yes" : "no",
                 QSslSocket::sslLibraryBuildVersionString(),
                 QSslSocket::sslLibraryVersionString());
}

bool NntpConnection::supportsSsl() { return QSslSocket::supportsSsl(); }
