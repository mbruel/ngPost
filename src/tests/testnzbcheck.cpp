#include "testnzbcheck.h"

#include <QDebug>
#include <QtTest/QtTest>

#include "NgPost.h"
#include "NzbCheck.h"

TestNzbGet::TestNzbGet(QString const &testName, int argc, char *argv[]) : MacroTest(testName)
{
    qDebug() << "Construction TestNzbGet => _ngPost...";
    _ngPost = new NgPost(argc, argv);
}

TestNzbGet::~TestNzbGet() { }

void TestNzbGet::cleanup()
{
    qDebug() << "Deleting TestNzbGet =>ngPost...";
    //    delete _ngPost;
    // Code to run after each test
}
void TestNzbGet::onTestNzbCheckOK()
{
    auto errors = _ngPost->parseDefaultConfig();
    MB_VERIFY(errors.isEmpty(), this); // Mo errors

    qDebug() << "onTestNzbCheckOK...";
    QString nzbOK = "://nzb/ngPost_v4.16_libssl3-x86_64.nzb";
    if (_ngPost->doNzbCheck(nzbOK))
        return; // eroor log already written...

    int exitCode = _ngPost->startEventLoop(); // let's do the job, we need the event loop!
    uint nbMissing = _ngPost->nbMissingArticles();
    MB_LOG("nbMissing for ngPost_v4.16_libssl3-x86_64.nzb: ", nbMissing);
    MB_VERIFY(nbMissing == 0, this);
}
void TestNzbGet::onTestNzbCheckKO()
{
    qDebug() << "onTestNzbCheckKO...";
    QString nzbKO = "://nzb/tumer_fue-40pa4-cahier-2022_ko.nzb";

    if (_ngPost->doNzbCheck(nzbKO))
        return; // eroor log already written...

    connect(_ngPost->getNzbCheck(), &NzbCheck::checkFinished, this, &TestNzbGet::onNzbCheckFinished);
    _ngPost->startEventLoop(); // let's do the job, we need the event loop!

    uint nbMissing = _ngPost->nbMissingArticles();
    MB_VERIFY(nbMissing == 0, this);
}

void TestNzbGet::onNzbCheckFinished(uint missingArticles)
{
    qDebug() << "[MBMBMBMB] TestNzbGet::onNzbCheckFinished";
    qApp->quit();
}
