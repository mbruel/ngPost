#include "TestConfig.h"
#include "../Macros.h"
#include "../TestUtils.h"
#include <QTest>

#include <QDebug>
#include <QtTest/QtTest>

#include "NgPost.h"
#include "nntp/NntpArticle.h"
#include "NntpConnection.h"
#include "Poster.h"

TestConfig::TestConfig(QString const &testName, int argc, char *argv[]) : MacroTest(testName)
{
    qDebug() << "Construction TestConfig => _ngPost...";
    _ngPost = new NgPost(argc, argv);
}

#ifdef __Launch_TestLocalConfig__
void TestConfig::onTestLoadDefautConfig()
{
    qDebug() << "onTestLoadDefautConfig...";

    auto errors = _ngPost->parseDefaultConfig();
    MB_VERIFY(errors.isEmpty(), this); // Example test, always passes
}
#endif

void TestConfig::onTestLoadOldConfig()
{
    qDebug() << "onTestLoadOldConfig...";

    QString oldConfig = ":/config/ngPost_noConfVersion.conf";
    auto    errors    = _ngPost->loadConfig(oldConfig);
    MB_VERIFY(errors.isEmpty(), this); // Example test, always passes
}

void TestConfig::onTestLoadXSNewsPartnerConfig()
{
    TestUtils::loadXSNewsPartnerConf(*_ngPost, true);
    MB_VERIFY(_ngPost->configFile() == TestUtils::partnerConfig(), this);
}

void TestConfig::onTestLoadXSNewsPartnerConfAndCheckConnection()
{
    ConnectionHandler *conHandler = TestUtils::loadXSNewsPartnerConfAndCheckConnection(*_ngPost);
    conHandler->start();
    MB_VERIFY(QTest::qWaitFor([&conHandler]() { return conHandler->isTestDone(); }, 5000), this);
    TestUtils::clearConnectionHandler();
}

void TestConfig::onTestPostingNotAllowed()
{
    ConnectionHandler *conHandler = TestUtils::loadXSNewsPartnerConfAndCheckConnection(*_ngPost, true);
    conHandler->start();
    MB_VERIFY(QTest::qWaitFor([&conHandler]() { return conHandler->isAutenticated(); }, 5000), this);

    emit conHandler->sendTestArticleInGoodThread(); // we do the send in the good thread!

    std::string article(NNTP::Article::testPostingStdString());
    NgLogger::log(
            "TestConfig sending NNTP::Article::testPostingStdString", true, NgLogger::DebugLevel::DebugBold);
    MB_VERIFY(QTest::qWaitFor([&conHandler]() { return conHandler->isTestDone(); }, 5000), this);

    TestUtils::clearConnectionHandler();
}
