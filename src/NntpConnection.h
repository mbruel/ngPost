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
#ifndef NNTPCONNECTION_H
#define NNTPCONNECTION_H

#include <QSslError>
#include <QTcpSocket>

#include "utils/NgLogger.h"

struct NntpServerParams;
class NntpArticle;
class NgPost;
class Poster;

class QSslSocket;
class QByteArray;
#ifdef __USE_CONNECTION_TIMEOUT__
class QTimer;
#endif

/*!
 * \brief NntpConnection is a client side Active Object that is made to Post Articles in an async way
 *
 * the
 */
class NntpConnection : public QObject
{
    Q_OBJECT
signals:
    void startConnection();
    void killConnection();

    //    void error(QTcpSocket::SocketError socketerror); //!< Socket Error
    void socketError(QString aError); //!< Error during socket creation (ssl or not)
    void errorConnecting(QString aError);
    void disconnected(NntpConnection *con);
    void log(QString const &msg, bool newline, NgLogger::DebugLevel debugLvl) const;
    void error(QString const &msg) const;

private:
    enum class PostingState
    {
        NOT_CONNECTED = 0,
        CONNECTED,
        AUTH_USER,
        AUTH_PASS,
        IDLE,
        SENDING_ARTICLE,
        WAITING_ANSWER,
        NO_MORE_FILES
    };

    int const               _id;        //!< connection id
    NntpServerParams const &_srvParams; //!< server parameters

    QTcpSocket *_socket;      //!< Real TCP socket
    bool        _isConnected; //!< to avoid to rely on iSocket && iSocket->isOpen()

    QString _logPrefix; //!< log prefix: NntpConnection[<iSocketDescriptor>]

    PostingState _postingState;
    NntpArticle *_currentArticle;
    ushort       _nbDisconnected;

    NgPost const &_ngPost; //!< MB_TODO try to avoid it and just use NgLogger directly
    Poster       *_poster; //!< as to pointer cause created with nullptr
#ifdef __USE_CONNECTION_TIMEOUT__
    QTimer *_timeout;
#endif

public:
    /*!
     * \brief NntpConnection constructor
     * \param id          : connection id
     * \param ssl         : should the connection be encrypted?
     */
    explicit NntpConnection(NgPost const &ngPost, int id, NntpServerParams const &srvParams);

    NntpConnection(NntpConnection const &)             = delete;
    NntpConnection(NntpConnection const &&)            = delete;
    NntpConnection &operator=(NntpConnection const &)  = delete;
    NntpConnection &operator=(NntpConnection const &&) = delete;

    ~NntpConnection(); //!< destructor: delete the QTcpSocket

    inline int getId() const; //!< NntpConnection id: iSocketDescriptor

    inline void write(QByteArray const &aBuffer); //!< write on the socket
    inline void write(char const *aBuffer);       //!< write on the socket

    inline void resetErrorCount();
    inline bool isConnected() const;

    void setPoster(Poster *poster);

    inline bool hasNoMoreFiles() const;

    static QString sslSupportInfo();
    static bool    supportsSsl();

public slots:
    void onStartConnection();
    void onKillConnection();

    void onConnected();
    void onEncrypted();

    void onDisconnected(); //!< Handle disconnection

    void onReadyRead();                               //!< To be overridden in Child class
    void onSslErrors(QList<QSslError> const &errors); //!< SSL errors handler
    void onErrors(QAbstractSocket::SocketError);      //!< Socket errors handler

#ifdef __USE_CONNECTION_TIMEOUT__
    void onTimeout();
#endif

private:
    //! log function for QString
    inline void _log(QString const &aMsg, NgLogger::DebugLevel debugLvl = NgLogger::DebugLevel::None) const
    {
        emit log(QString("[%1] %2").arg(_logPrefix).arg(aMsg), true, debugLvl);
    }

    //! overload for log function for char *
    inline void _log(char const *aMsg, NgLogger::DebugLevel debugLvl = NgLogger::DebugLevel::None) const
    {
        emit log(QString("[%1] %2").arg(_logPrefix).arg(aMsg), true, debugLvl);
    }

    //! overload log function for std::string
    inline void _log(std::string const &aMsg, NgLogger::DebugLevel debugLvl = NgLogger::DebugLevel::None) const
    {
        emit log(QString("[%1] %2").arg(_logPrefix).arg(QString::fromStdString(aMsg)), true, debugLvl);
    }

    //! error function for QString
    inline void _error(QString const &aMsg) const { emit error(QString("[%1] %2").arg(_logPrefix).arg(aMsg)); }

    //! overload error function for std::string
    inline void _error(std::string const &aMsg) const
    {
        emit error(QString("[%1] %2").arg(_logPrefix).arg(QString::fromStdString(aMsg)));
    }

    //! overload error function for char *
    inline void _error(char const *aMsg) const
    {
        emit error(QString("[%1] %2").arg(_logPrefix).arg(aMsg));
    } //!< log function for char *

    void _sendNextArticle();
    void _closeConnection();

    inline void deleteSocket();
};

int NntpConnection::getId() const { return _id; }

void NntpConnection::write(QByteArray const &aBuffer) { _socket->write(aBuffer); }
void NntpConnection::write(char const *aBuffer) { _socket->write(aBuffer); }

void NntpConnection::resetErrorCount() { _nbDisconnected = 0; }
bool NntpConnection::isConnected() const { return _isConnected; }

bool NntpConnection::hasNoMoreFiles() const { return _postingState == PostingState::NO_MORE_FILES; }

void NntpConnection::deleteSocket()
{
    _isConnected = false;
    _socket->close();
    _socket->deleteLater();
    _socket = nullptr;
}

#endif // NNTPCONNECTION_H
