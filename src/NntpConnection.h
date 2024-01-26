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

#include <QTcpSocket>
#include <QSslError>

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

private:
    enum class PostingState {NOT_CONNECTED = 0, CONNECTED,
                             AUTH_USER, AUTH_PASS,
                             IDLE, SENDING_ARTICLE, WAITING_ANSWER, NO_MORE_FILES};

    const int               _id;        //!< connection id
    const NntpServerParams &_srvParams; //!< server parameters

    QTcpSocket   *_socket;         //!< Real TCP socket
    bool          _isConnected;    //!< to avoid to rely on iSocket && iSocket->isOpen()

    QString       _logPrefix;      //!< log prefix: NntpConnection[<iSocketDescriptor>]


    PostingState   _postingState;
    NntpArticle   *_currentArticle;
    ushort         _nbDisconnected;

    NgPost        *_ngPost;
    Poster        *_poster;
#ifdef __USE_CONNECTION_TIMEOUT__
    QTimer        *_timeout;
#endif

public:
    /*!
     * \brief NntpConnection constructor
     * \param id          : connection id
     * \param ssl         : should the connection be encrypted?
     */
    explicit NntpConnection(NgPost *ngPost, int id, const NntpServerParams &srvParams);

    NntpConnection(const NntpConnection &)              = delete;
    NntpConnection(const NntpConnection &&)             = delete;
    NntpConnection & operator=(const NntpConnection &)  = delete;
    NntpConnection & operator=(const NntpConnection &&) = delete;

    ~NntpConnection();                        //!< destructor: delete the QTcpSocket


    inline int getId() const;         //!< NntpConnection id: iSocketDescriptor

    inline void write(const QByteArray & aBuffer); //!< write on the socket
    inline void write(const char *aBuffer); //!< write on the socket

    inline void resetErrorCount();
    inline bool isConnected() const;

    void setPoster(Poster *poster);

    inline bool hasNoMoreFiles() const;

    static QString sslSupportInfo();
    static bool supportsSsl();

signals:
    void startConnection();
    void killConnection();

//    void error(QTcpSocket::SocketError socketerror); //!< Socket Error
    void socketError(QString aError);                //!< Error during socket creation (ssl or not)
    void errorConnecting(QString aError);
    void disconnected(NntpConnection *con);
    void log(QString msg, bool newline = true) const;
    void error(QString msg) const;


public slots:
    void onStartConnection();
    void onKillConnection();


    void onConnected();
    void onEncrypted();

    void onDisconnected();    //!< Handle disconnection

    void onReadyRead(); //!< To be overridden in Child class
    void onSslErrors(const QList<QSslError> &errors); //!< SSL errors handler
    void onErrors(QAbstractSocket::SocketError);      //!< Socket errors handler

#ifdef __USE_CONNECTION_TIMEOUT__
    void onTimeout();
#endif

private:
    inline void _log(const QString     &aMsg) const; //!< log function for QString
    inline void _log(const char        *aMsg) const; //!< log function for char *
    inline void _log(const std::string &aMsg) const; //!< log function for std::string
    inline void _error(const QString     &aMsg) const; //!< log function for QString
    inline void _error(const char        *aMsg) const; //!< log function for char *
    inline void _error(const std::string &aMsg) const; //!< log function for std::string

    void _sendNextArticle();
    void _closeConnection();

    inline void deleteSocket();


};


int NntpConnection::getId() const { return _id;}

void NntpConnection::write(const QByteArray &aBuffer){_socket->write(aBuffer);}
void NntpConnection::write(const char *aBuffer){_socket->write(aBuffer);}

void NntpConnection::resetErrorCount() { _nbDisconnected = 0; }
bool NntpConnection::isConnected() const { return _isConnected; }

bool NntpConnection::hasNoMoreFiles() const { return _postingState == PostingState::NO_MORE_FILES; }

void NntpConnection::_log(const char *aMsg) const
{
    emit log(QString("[%1] %2").arg(_logPrefix).arg(aMsg));
}
void NntpConnection::_log(const QString &aMsg) const
{
    emit log(QString("[%1] %2").arg(_logPrefix).arg(aMsg));
}
void NntpConnection::_log(const std::string &aMsg) const
{
    emit log(QString("[%1] %2").arg(_logPrefix).arg(QString::fromStdString(aMsg)));
}

void NntpConnection::_error(const char *aMsg) const
{
    emit error(QString("[%1] %2").arg(_logPrefix).arg(aMsg));
}
void NntpConnection::_error(const QString &aMsg) const
{
    emit error(QString("[%1] %2").arg(_logPrefix).arg(aMsg));
}
void NntpConnection::_error(const std::string &aMsg) const
{
    emit error(QString("[%1] %2").arg(_logPrefix).arg(QString::fromStdString(aMsg)));
}

void NntpConnection::deleteSocket()
{
    _isConnected = false;
    _socket->close();
    _socket->deleteLater();
    _socket = nullptr;
}

#endif // NNTPCONNECTION_H
