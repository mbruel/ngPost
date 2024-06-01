#include "TestUtils.h"
#include "Macros.h"

#include <QtTest/QtTest>

#include "NgPost.h"
#include "nntp/ServerParams.h"
#include "NntpConnection.h"

ConnectionHandler *TestUtils::sConnectionHandler = nullptr;

void TestUtils::clearConnectionHandler()
{
    if (sConnectionHandler)
        sConnectionHandler->deleteLater();
    sConnectionHandler = nullptr;
}

void TestUtils::loadXSNewsPartnerConf(NgPost &ngPost)
{
    QStringList errors = ngPost.loadConfig(xsnewsConfig);
    if (errors.size())
        return; // we got issues...

    SharedParams const &postingParams = ngPost.postingParams();
    auto const         &servers       = postingParams->nntpServers();

    // Verify parameters (conf well loaded)
    QVERIFY(servers.size() == 1);
    QVERIFY2(servers.size() == 1, "Wrong number of servers...");
    QVERIFY2(servers.front()->host == xsnewsAddr, "Not XSNews partner host... :(");
    QVERIFY2(servers.front()->port == xsnewsPort, "Not XSNews partner port... :(");
    QVERIFY2(servers.front()->user == xsnewsUser, "Not XSNews partner user... :(");
    QVERIFY2(servers.front()->useSSL == true, "Not XSNews partner useSSL... :(");
    QVERIFY2(servers.front()->enabled == true, "Not XSNews partner enabled... :(");
    QVERIFY2(servers.front()->nzbCheck == true, "Not XSNews partner nzbCheck... :(");
    QVERIFY2(servers.front()->nbCons >= 7, "Not XSNews partner useSSL... :(");
}

ConnectionHandler *TestUtils::loadXSNewsPartnerConfAndCheckConnection(NgPost &ngPost)
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
    sConnectionHandler = new ConnectionHandler(ngPost); // Leaking handler...

    return sConnectionHandler;
}

ConnectionHandler::ConnectionHandler(NgPost const &ngPost, QObject *parent)
    : QObject(parent), _nntpCon(NntpConnection::createNntpConnection(ngPost, 666)), _testDone(false)
{
    if (_nntpCon)
    {
        // starting _thread (using ConnectionHandler::start) starts the _nntpCon
        connect(&_thread, &QThread::started, _nntpCon, &NntpConnection::startConnection, Qt::QueuedConnection);

        connect(_nntpCon, &NntpConnection::connected, this, &ConnectionHandler::onNntpConnected);
        connect(_nntpCon, &NntpConnection::helloReceived, this, &ConnectionHandler::onNntpHelloReceived);
        connect(_nntpCon, &NntpConnection::authenticated, this, &ConnectionHandler::onNntpAuthenticated);

        // when the nntp con is disconnected we stop the test
        connect(_nntpCon, &NntpConnection::disconnected, this, &ConnectionHandler::onNntpConDisconnected);
        connect(_nntpCon, &NntpConnection::stopTest, this, &ConnectionHandler::onStopTest);

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
    // no need cause  to _nntpCon->startConnection() as it's connected to &QThread::started
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
    NgLogger::log("Nntp connection is Authenticated! Let's stop here, we're good :)", true);
    emit _nntpCon->killConnection(); // let's stop the connection here
}

void ConnectionHandler::onStopTest()
{
    qDebug() << "[ConnectionHandler::onStopTest] Stopping the Test!!!...";
    _nntpCon->deleteLater();
    _thread.quit();
    _testDone = true;
}
