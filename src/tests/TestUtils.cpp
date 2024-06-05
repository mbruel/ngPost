#include "TestUtils.h"
#include "Macros.h"

#include <QtTest/QtTest>

#include "NgPost.h"
#include "nntp/NntpArticle.h"
#include "nntp/ServerParams.h"
#include "NntpConnection.h"

ConnectionHandler *TestUtils::sConnectionHandler = nullptr;

void TestUtils::clearConnectionHandler()
{
    if (sConnectionHandler)
    {
        NgLogger::log("deleteLater ConnectionHandler static instance", true, NgLogger::DebugLevel::DebugBold);
        sConnectionHandler->deleteLater();
    }
    sConnectionHandler = nullptr;
}

void TestUtils::loadXSNewsPartnerConf(NgPost &ngPost, bool dispFirstSrv)
{
    QStringList errors = ngPost.loadConfig(xsnewsConfig);
    if (errors.size())
        return; // we got issues...

    SharedParams const &postingParams = ngPost.postingParams();
    auto const         &servers       = postingParams->nntpServers();

    // Verify parameters (conf well loaded)
    QVERIFY(servers.size() == 1);
    QVERIFY2(servers.size() == 1, "Wrong number of servers...");
    NNTP::ServerParams *srv = servers.front();
    QVERIFY2(srv->host == xsnewsAddr, "Not XSNews partner host... :(");
    QVERIFY2(srv->port == xsnewsPort, "Not XSNews partner port... :(");
    QVERIFY2(srv->user == xsnewsUser, "Not XSNews partner user... :(");
    QVERIFY2(srv->useSSL == true, "Not XSNews partner useSSL... :(");
    QVERIFY2(srv->enabled == true, "Not XSNews partner enabled... :(");
    QVERIFY2(srv->nzbCheck == true, "Not XSNews partner nzbCheck... :(");
    QVERIFY2(srv->nbCons >= 7, "Not XSNews partner useSSL... :(");

    if (dispFirstSrv)
        MB_LOG("[TestUtils::loadXSNewsPartnerConf] first srv: ", srv->str());
}

ConnectionHandler *TestUtils::loadXSNewsPartnerConfAndCheckConnection(NgPost &ngPost, bool testPosting)
{
    TestUtils::loadXSNewsPartnerConf(ngPost);
    bool ok = ngPost.postingParams()->nntpServers().front()->host == "reader.xsnews.nl";
    if (!ok)
    {
        qCritical() << "error connection xsnews partner...";
        return nullptr;
    }

    if (sConnectionHandler)
    {
        qCritical() << "sConnectionHandler already exists (should not happen...)";
        return nullptr;
    }
    sConnectionHandler = new ConnectionHandler(ngPost, false); // Leaking handler...

    return sConnectionHandler;
}

ConnectionHandler::ConnectionHandler(NgPost const &ngPost, bool testPosting, QObject *parent)
    : QObject(parent)
    , _nntpCon(NntpConnection::createNntpConnection(ngPost, 666))
    , _testDone(false)
    , _isAutenticated(false)
    , _testPosting(false)
{
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
