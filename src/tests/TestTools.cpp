#include "TestTools.h"
#include "Macros.h"

#include <QtTest/QtTest>

#include "NgPost.h"
#include "nntp/ServerParams.h"
#include "NntpConnection.h"

void TestTools::loadXSNewsPartnerConf(NgPost *ngPost)
{
    QStringList errors = ngPost->loadConfig(xsnewsConfig);
    if (errors.size())
        return; // we got issues...

    SharedParams const &postingParams = ngPost->postingParams();
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

bool TestTools::loadXSNewsPartnerConfAndCheckConnection(NgPost *ngPost)
{
    TestTools::loadXSNewsPartnerConf(ngPost);
    bool ok = ngPost->postingParams()->nntpServers().front()->host == "reader.xsnews.nl";
    if (!ok)
        qCritical() << "error connection xsnews partner...";

    NNTP::ServerParams *srvParam = ngPost->postingParams()->nntpServers().front();
    //    NntpConnection   *nntpCon  = new NntpConnection(ngPost, 42, NNTP::ServerParams());

    //    connect(nntpCon,
    //            &NntpConnection::disconnected,
    //            this,
    //            &PostingJob::onDisconnectedConnection,
    //            Qt::QueuedConnection);

    return ok;
}
