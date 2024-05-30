#include "testNgTools.h"

#include <QDebug>
#include <QNetworkReply>
#include <QtTest/QtTest>

#include "MacroTest.h"
#include "NgConf.h"
#include "NgPost.h"
#include "utils/NgTools.h"

using namespace NgConf;

TestNgTools::TestNgTools(QString const &testName, int argc, char *argv[]) : MacroTest(testName)
{
    qDebug() << "Construction TestNgTools => _ngPost...";
    _ngPost = new NgPost(argc, argv);
}

void TestNgTools::cleanup()
{
    qDebug() << "Deleting ngPost...";
    //    delete _ngPost;
    // Code to run after each test
}

void TestNgTools::onTestLoadDefautConfig()
{
    ushort nbVerifs = 0, nbFailed = 0;
    qDebug() << "onTestLoadDefautConfig...";

    auto errors = _ngPost->parseDefaultConfig();
    MB_VERIFY(errors.isEmpty(), this); // Mo errors

    SharedParams const &params = _ngPost->postingParams();

    MB_VERIFY(params->inputDir() == "/home/mb/Downloads", this);
    MB_VERIFY(params->rarPath() == "/usr/bin/rar", this);
    MB_LOG("Checking: params->rarPath(): ", params->rarPath());
    MB_VERIFY(params->rarArgs().join(" ") == kDefaultRarExtraOptions, this); // default values

    MB_VERIFY(params->rarSize() == 7, this);
    MB_VERIFY((params->useRarMax() == true), this);
    MB_VERIFY(params->rarMax() == 99, this);

    MB_VERIFY(params->lengthName() == 33, this);
    MB_VERIFY(params->lengthPass() == 25, this);
    MB_LOG("params->rarPassFixed(): ", params->rarPassFixed());
    MB_VERIFY(params->rarPassFixed() == "", this); // unused

    MB_VERIFY(params->par2Path() == "/home/mb/apps/bin/parpar", this);
    MB_VERIFY(params->par2Args() == "-s1M -r1n*0.6 -m2048M -p1l -t7 --progress stdout -q", this);
    qDebug() << "YOYOYO->groups(:" << params->groups();

    MB_VERIFY(params->tmpPath() == "/tmp", this);
    MB_VERIFY(params->ramPath() == "", this);
    MB_LOG("params->tmpPath(): ", params->tmpPath());

    MB_VERIFY(params->urlNzbUploadStr() == "", this);
    MB_VERIFY(params->urlNzbUpload() == nullptr, this);

    MB_VERIFY(params->obfuscateArticles() == false, this);
    MB_VERIFY(params->obfuscateFileName() == false, this);
    MB_VERIFY(params->delFilesAfterPost() == false, this);
    MB_VERIFY(params->overwriteNzb() == false, this);

    MB_VERIFY(params->useParPar() == true, this);
    MB_VERIFY(params->useMultiPar() == false, this);

    MB_LOG("params->groups(): ", params->groups());
    MB_VERIFY(params->groups() == "alt.binaries.xylo,alt.binaries.superman,alt.binaries.paxer", this);
}

void TestNgTools::onTestLoadOldConfig()
{
    QString const &configPath = "://config/ngPost_noConfVersion.conf";
    QStringList    errors     = _ngPost->loadConfig(configPath);
    MB_VERIFY(errors.isEmpty(), this);
    uint confVersion = NgTools::isConfigurationVesionObsolete();
    MB_LOG("confVersion: ", confVersion);
    MB_VERIFY(confVersion == 302, this);
}

void TestNgTools::onSubstituteNZBNameForExistingFileName()
{
    QString nzbNzame("://testNgTools/filename.nzb");
    NgTools::substituteNZBNameForExistingFileName(nzbNzame);
    MB_LOG("substituteNZBNameForExistingFileName(nzbNzame)=", nzbNzame);
    MB_VERIFY(nzbNzame == "://testNgTools/filename_4.nzb", this);
}

void TestNgTools::onTestgetUShortVersion(QString const &version)
{
    MB_VERIFY(NgTools::getUShortVersion("5") == 500, this);
    MB_VERIFY(NgTools::getUShortVersion("5.0") == 500, this);
    MB_VERIFY(NgTools::getUShortVersion("5.42") == 542, this);
    MB_VERIFY(NgTools::getUShortVersion("4.4") == 404, this);
    MB_VERIFY(NgTools::getUShortVersion("1.11") == 111, this);
    MB_VERIFY(NgTools::getUShortVersion("4.16") == 416, this);
}

//! check NgPost::checkForNewVersion and NgTools::getUShortVersion
//! we're supposed to get a new version!
void TestNgTools::onCheckForNewVesion()
{
    QNetworkReply *reply = _ngPost->checkForNewVersion();
    connect(reply, &QNetworkReply::finished, this, &TestNgTools::onCheckForNewVersionReply);
    disconnect(reply, &QNetworkReply::finished, _ngPost, &NgPost::onCheckForNewVersion);
    _ngPost->startEventLoop();
}

void TestNgTools::onCheckForNewVersionReply()
{
    QNetworkReply *reply = static_cast<QNetworkReply *>(sender());
    while (reply && !reply->atEnd())
    {
        QString                 line  = reply->readLine().trimmed();
        QRegularExpressionMatch match = kAppVersionRegExp.match(line);
        if (match.hasMatch())
        {
            ushort lastRelease  = match.captured(2).toUShort() * 100 + match.captured(3).toUShort();
            ushort currentBuilt = NgTools::getUShortVersion(kVersion);
            MB_VERIFY(lastRelease > currentBuilt, this);
            MB_LOG("TestNgTools::onCheckForNewVersionReply: lastRelease: ",
                   lastRelease,
                   "currentBuilt:",
                   currentBuilt);

            break; // no need to continue to parse the page
        }
    }
    disconnect(reply, &QNetworkReply::finished, _ngPost, &NgPost::onCheckForNewVersion);
    reply->deleteLater();
    qApp->quit();
}
