#include "ConnectionHandler.h"

#include "nntp/NntpArticle.h"
#include "NntpConnection.h"

ConnectionHandler::ConnectionHandler(NgPost const &ngPost, bool testPosting, QObject *parent)
    : QObject(parent)
    , _nntpCon(NntpConnection::createNntpConnection(ngPost, 666))
    , _testDone(false)
    , _isAutenticated(false)
    , _testPosting(false)
{
    _thread.setObjectName("testThread");
    connect(this,
            &ConnectionHandler::sendTestArticleInGoodThread,
            this,
            &ConnectionHandler::onSendTestArticleInGoodThread,
            Qt::QueuedConnection);

    if (_nntpCon)
    {
        // starting _thread (using ConnectionHandler::start) starts the _nntpCon
        connect(&_thread,
                &QThread::started,
                _nntpCon,
                &NntpConnection::sigStartConnection,
                Qt::QueuedConnection);

        connect(_nntpCon, &NntpConnection::sigConnected, this, &ConnectionHandler::onNntpConnected);
        connect(_nntpCon, &NntpConnection::sigHelloReceived, this, &ConnectionHandler::onNntpHelloReceived);
        connect(_nntpCon, &NntpConnection::sigAuthenticated, this, &ConnectionHandler::onNntpAuthenticated);

        // when the nntp con is disconnected we stop the test
        connect(_nntpCon, &NntpConnection::sigDisconnected, this, &ConnectionHandler::onNntpConDisconnected);
        connect(_nntpCon, &NntpConnection::sigStopTest, this, &ConnectionHandler::onStopTest);
        if (testPosting)
            connect(_nntpCon,
                    &NntpConnection::sigPostingNotAllowed,
                    this,
                    &ConnectionHandler::onPostingNotAllowed);

        _nntpCon->moveToThread(&_thread);
    }
    else
        qCritical() << "ConnectionHandler couldn't create a connection...";

    this->moveToThread(&_thread);
}

ConnectionHandler::~ConnectionHandler()
{
    if (_thread.isRunning())
        _thread.quit();
}

void ConnectionHandler::start()
{
    // no need cause  to _nntpCon->sigStartConnection() as it's connected to &QThread::started
    _thread.start();
}

void ConnectionHandler::onNntpConDisconnected() { NgLogger::log("Nntp connection  disconnected...", true); }
void ConnectionHandler::onNntpConnected() { NgLogger::log("Nntp connection successfully connected!", true); }
void ConnectionHandler::onNntpHelloReceived()
{
    NgLogger::log("Nntp connection recieved Hello Message :)", true);
}
void ConnectionHandler::onNntpAuthenticated()
{
    _isAutenticated = true;

    NgLogger::log("Nntp connection is Authenticated! Let's stop here, we're good :)", true);
    if (!_testPosting)
        emit _nntpCon->sigKillConnection(); // let's stop the connection here
}

void ConnectionHandler::onPostingNotAllowed()
{
    NgLogger::log("seems we can't post...", true, NgLogger::DebugLevel::DebugBold);
    emit _nntpCon->sigKillConnection(); // let's stop the connection here
}

void ConnectionHandler::onStopTest()
{
    qDebug() << "[ConnectionHandler::onStopTest] Stopping the Test!!!...";
    _nntpCon->deleteLater();
    _thread.quit();
    _testDone = true;
}

void ConnectionHandler::onSendTestArticleInGoodThread()
{
    std::string article(NNTP::Article::testPostingStdString());
    _nntpCon->write(article.c_str());
}
