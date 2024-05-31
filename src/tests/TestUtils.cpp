#include "TestUtils.h"
#include "Macros.h"

#include <QtTest/QtTest>

#include "NgPost.h"
#include "nntp/ServerParams.h"
#include "NntpConnection.h"

ConnectionHandler *TestUtils::sConnectionHandler = nullptr;

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

bool TestUtils::loadXSNewsPartnerConfAndCheckConnection(NgPost &ngPost)
{
    TestUtils::loadXSNewsPartnerConf(ngPost);
    bool ok = ngPost.postingParams()->nntpServers().front()->host == "reader.xsnews.nl";
    if (!ok)
    {
        qCritical() << "error connection xsnews partner...";
        return false;
    }

    if (sConnectionHandler)
    {
        qCritical() << "sConnectionHandler already exists (should not happen...)";
        return false;
    }
    sConnectionHandler = new ConnectionHandler(ngPost); // Leaking handler...

    let's wait the thread!!!! return ok;
}

ConnectionHandler::ConnectionHandler(NgPost const &ngPost, QObject *parent)
    : QObject(parent), _nntpCon(NntpConnection::createNntpConnection(ngPost, 666))
{
    if (_nntpCon)
    {
        QObject::connect(_nntpCon,
                         &NntpConnection::disconnected,
                         this,
                         &ConnectionHandler::onDisconnected,
                         Qt::QueuedConnection);
        _nntpCon->moveToThread(&_thread);
    }
    else
        qCritical() << "ConnectionHandler couldn't create a connection...";
}

ConnectionHandler::~ConnectionHandler()
{
    if (_thread.isRunning())
        _thread.quit();
}

void ConnectionHandler::startConnection() { emit _nntpCon->startConnection(); }

void ConnectionHandler::onDisconnected()
{
    qDebug() << "[ConnectionHandler::onDisconnected] connection disconnected...";
    _nntpCon->deleteLater();
}
