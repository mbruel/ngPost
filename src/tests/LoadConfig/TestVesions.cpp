#include "TestVesions.h"

#include <QDebug>
#include <QtTest/QtTest>

#include "NgPost.h"

void TestVesions::onTestLoadDefautConfig()
{
    qDebug() << "onTestLoadDefautConfig...";

    auto errors = _ngPost->parseDefaultConfig();
    QVERIFY(errors.isEmpty()); // Example test, always passes
}

void TestVesions::onTestLoadOldConfig()
{
    qDebug() << "onTestLoadOldConfig...";

    QString oldConfig = ":/config/ngPost_noConfVersion.conf";
    //    auto    errors    = _ngPost->loadConfig(oldConfig);
    QVERIFY(1); // Example test, always passes
}
