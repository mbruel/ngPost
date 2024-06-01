#include "TestVesions.h"
#include "../Macros.h"
#include "../TestUtils.h"
#include <QTest>

#include <QDebug>
#include <QtTest/QtTest>

#include "NgPost.h"

TestVesions::TestVesions(QString const &testName, int argc, char *argv[]) : MacroTest(testName)
{
    qDebug() << "Construction TestVesions => _ngPost...";
    _ngPost = new NgPost(argc, argv);
}

void TestVesions::onTestLoadDefautConfig()
{
    qDebug() << "onTestLoadDefautConfig...";

    auto errors = _ngPost->parseDefaultConfig();
    MB_VERIFY(errors.isEmpty(), this); // Example test, always passes
}

void TestVesions::onTestLoadOldConfig()
{
    qDebug() << "onTestLoadOldConfig...";

    QString oldConfig = ":/config/ngPost_noConfVersion.conf";
    auto    errors    = _ngPost->loadConfig(oldConfig);
    MB_VERIFY(errors.isEmpty(), this); // Example test, always passes
}

void TestVesions::onTestLoadXSNewsPartnerConfig()
{
    TestUtils::loadXSNewsPartnerConf(*_ngPost);
    MB_VERIFY(_ngPost->configFile() == TestUtils::partnerConfig(), this);
}

void TestVesions::onTestLoadXSNewsPartnerConfAndCheckConnection()
{
    ConnectionHandler *conHandler = TestUtils::loadXSNewsPartnerConfAndCheckConnection(*_ngPost);
    conHandler->start();
    MB_VERIFY(QTest::qWaitFor([&conHandler]() { return conHandler->isTestDone(); }, 5000), this);
}
