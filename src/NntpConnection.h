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

#ifndef NNTPCONNECTION_H
#define NNTPCONNECTION_H

#include <QTcpSocket>
#include <QQueue>

class NntpServerParams;
class NntpArticle;
class NgPost;

class QSslSocket;
class QSslError;
class QByteArray;


/*!
 * \brief NntpConnection is a client side Active Object that is made to Post Articles in an async way
 *
 * the
 */
class NntpConnection : public QObject
{
    Q_OBJECT

private:
    NgPost                 *_ngPost;
    const int               _id;        //!< connection id
    const NntpServerParams &_srvParams; //!< server parameters

    QTcpSocket   *_socket;         //!< Real TCP socket
    bool          _isConnected;    //!< to avoid to rely on iSocket && iSocket->isOpen()

    const QString _logPrefix;      //!< log prefix: NntpConnection[<iSocketDescriptor>]

    enum class PostingState {NOT_CONNECTED = 0, CONNECTED,
                             AUTH_USER, AUTH_PASS,
                             IDLE, SENDING_ARTICLE, WAITING_ANSWER};

    PostingState   _postingState;
    NntpArticle   *_currentArticle;

#ifndef __USE_MUTEX__
    QQueue<NntpArticle*> _articles;
#endif

    static int sSocketTimeoutMs;

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


signals:
    void startConnection();
    void killConnection();

//    void error(QTcpSocket::SocketError socketerror); //!< Socket Error
    void socketError(QString aError);                //!< Error during socket creation (ssl or not)
    void errorConnecting(QString aError);
    void disconnected(NntpConnection *con);
    void log(QString msg) const;
    void error(QString msg) const;


#ifndef __USE_MUTEX__
    void pushArticle(NntpArticle *article);
    void requestArticle(NntpConnection *con);
#endif


public slots:
    void onStartConnection();
    void onKillConnection();


    void onConnected();
    void onEncrypted();

    void onDisconnected();    //!< Handle disconnection

    void onReadyRead(); //!< To be overridden in Child class
    void onSslErrors(const QList<QSslError> &errors); //!< SSL errors handler
    void onErrors(QAbstractSocket::SocketError);      //!< Socket errors handler

#ifndef __USE_MUTEX__
    void onPushArtice(NntpArticle *article);
#endif

private:
    void _log(const QString     &aMsg) const; //!< log function for QString
    void _log(const char        *aMsg) const; //!< log function for char *
    void _log(const std::string &aMsg) const; //!< log function for std::string
    void _error(const QString     &aMsg) const; //!< log function for QString
    void _error(const char        *aMsg) const; //!< log function for char *
    void _error(const std::string &aMsg) const; //!< log function for std::string

    void _sendNextArticle();
    void _closeConnection();


};


int NntpConnection::getId() const { return _id;}

void NntpConnection::write(const QByteArray &aBuffer){_socket->write(aBuffer);}
void NntpConnection::write(const char *aBuffer){_socket->write(aBuffer);}

#endif // NNTPCONNECTION_H
