#include "TestUtils.h"
#include "Macros.h"

#include "ConnectionHandler.h"
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
    QStringList errors = ngPost.loadConfig(kXSNewsConfig);
    // QVERIFY2(errors.size() != 0, "Wrong number of servers... ");
    if (errors.size())
    {
        qDebug() << "[MB_TRACE][TestUtils::loadXSNewsPartnerConf] error loading conf... " << kXSNewsConfig;
        qApp->processEvents();
        return; // we got issues...
    }

    SharedParams const &postingParams = ngPost.postingParams();
    auto const         &servers       = postingParams->nntpServers();

    postingParams->dumpParams();
    // Verify parameters (conf well loaded)
    // QVERIFY(servers.size() == 1);
    QVERIFY2(servers.size() == 1, "Wrong number of servers... ");
    NNTP::ServerParams *srv = servers.front();
    QVERIFY2(srv->host == kXSNewsAddr, "Not XSNews partner host... :(");
    QVERIFY2(srv->port == kXSNewsPort, "Not XSNews partner port... :(");
    QVERIFY2(srv->user == kXSNewsUser, "Not XSNews partner user... :(");
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
