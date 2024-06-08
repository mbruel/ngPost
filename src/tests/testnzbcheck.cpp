#include "testnzbcheck.h"
#include "utils/Macros.h"

#include <QDebug>
#include <QtTest/QtTest>

#include "utils/ConnectionHandler.h"
#include "utils/TestUtils.h"

#include "NgPost.h"
#include "NzbCheck.h"

TestNzbGet::TestNzbGet(QString const &testName, int argc, char *argv[]) : MacroTest(testName)
{
    qDebug() << "Construction TestNzbGet => _ngPost...";
    _ngPost = new NgPost(argc, argv);
}

void TestNzbGet::onTestNzbCheckOK()
{
    ConnectionHandler *conHandler = TestUtils::loadXSNewsPartnerConfAndCheckConnection(*_ngPost);
    conHandler->start();
    bool conOK = QTest::qWaitFor([&conHandler]() { return conHandler->isTestDone(); }, 5000); // 5 sec
    TestUtils::clearConnectionHandler();
    MB_VERIFY(conOK == true, this);
    if (!conOK)
    {
        qCritical() << "loadXSNewsPartnerConfAndCheckConnection FAILED...";
        return;
    }
    _ngPost->beQuiet();

    qDebug() << "onTestNzbCheckOK...";
    QString nzbOK      = "://nzb/ngPost_v4.16_libssl3-x86_64.nzb";
    bool    canDoCheck = _ngPost->doNzbCheck(nzbOK);
    MB_VERIFY(canDoCheck, this);
    if (!canDoCheck)
    {
        return;
    }
    NzbCheck *nzbCheck  = _ngPost->getNzbCheck();
    bool      checkDone = QTest::qWaitFor([&nzbCheck]() { return nzbCheck->isTestDone(); }, 120000); // 2min
    MB_VERIFY(checkDone == true, this);
    MB_VERIFY(nzbCheck->nbMissingArticles() == 0, this);
}

void TestNzbGet::onTestNzbCheckKO()
{
    ConnectionHandler *conHandler = TestUtils::loadXSNewsPartnerConfAndCheckConnection(*_ngPost);
    conHandler->start();
    bool conOK = QTest::qWaitFor([&conHandler]() { return conHandler->isTestDone(); }, 5000); // 5 sec
    TestUtils::clearConnectionHandler();
    MB_VERIFY(conOK == true, this);
    if (!conOK)
    {
        qCritical() << "loadXSNewsPartnerConfAndCheckConnection FAILED...";
        return;
    }
    _ngPost->beQuiet();

    qDebug() << "onTestNzbCheckOK...";
    QString nzbKO      = "://nzb/ngPost_v4.16_libssl3-x86_64_10missing.nzb";
    bool    canDoCheck = _ngPost->doNzbCheck(nzbKO);
    MB_VERIFY(canDoCheck, this);
    if (!canDoCheck)
    {
        return;
    }
    NzbCheck *nzbCheck  = _ngPost->getNzbCheck();
    bool      checkDone = QTest::qWaitFor([&nzbCheck]() { return nzbCheck->isTestDone(); }, 120000); // 2min
    MB_VERIFY(checkDone == true, this);
    uint nbMissings = nzbCheck->nbMissingArticles();
    MB_LOG("nbMissings for ngPost_v4.16_libssl3-x86_64_10missing: 10?", nbMissings);
    MB_VERIFY(nbMissings == 10, this);
}
