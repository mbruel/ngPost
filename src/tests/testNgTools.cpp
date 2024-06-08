#include "testNgTools.h"
#include "utils/Macros.h"

#include <filesystem> // std::filesystem::create_directory (faster that QDir!)

#include <QDebug>
#include <QNetworkReply>
#include <QtTest/QtTest>

#include "NgConf.h"
#include "NgPost.h"
#include "utils/MacroTest.h"
#include "utils/NgTools.h"

using namespace NgConf;

TestNgTools::TestNgTools(QString const &testName, int argc, char *argv[]) : MacroTest(testName)
{
    qDebug() << "Construction TestNgTools => _ngPost...";
    _ngPost = new NgPost(argc, argv);
}

#ifdef __Launch_TestLocalConfig__
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
#endif

void TestNgTools::onTestLoadOldConfig()
{
    QString const &configPath = "://config/ngPost_noConfVersion.conf";
    QStringList    errors     = _ngPost->loadConfig(configPath);
    MB_VERIFY(errors.isEmpty(), this);
    uint confVersion = NgTools::isConfigurationVesionObsolete();
    MB_LOG("confVersion: ", confVersion);
    MB_VERIFY(confVersion == 302, this);
}

void TestNgTools::onSubstituteExistingFile()
{
    // 1.: test for nzbFile
    QString nzbNzame("://testNgTools/filename.nzb");
    nzbNzame = NgTools::NgTools::substituteExistingFile(nzbNzame);
    MB_LOG("substituteNZBNameForExistingFileName(nzbNzame)=", nzbNzame);
    MB_VERIFY(nzbNzame == "://testNgTools/filename_4.nzb", this);

    // 2.: test for temp folder (bored to have to manually remove it during my tests...)
#if defined(__linux__) || defined(__APPLE__)
    QString               testPathQStr = "/tmp/substituteExistingFolder";
    std::filesystem::path testFolder   = testPathQStr.toStdString();
    // if it exist first we remove it
    if (std::filesystem::exists(testFolder))
        std::filesystem::remove(testFolder);

    // then we create it
    MB_VERIFY(std::filesystem::create_directory(testFolder), this);
    // try to create again, it should fail...
    MB_VERIFY(std::filesystem::create_directory(testFolder) == false, this);

    // create 4 alternatives
    for (int i = 1; i < 4; ++i)
    {
        QString currentPath = testFolder.string().c_str();
        // not forget the 2 false in substituteExistingFile to say it's not an nzb file but a directory
        QString newPath = NgTools::substituteExistingFile(currentPath, false, false);
        MB_VERIFY(newPath == QString("%1_%2").arg(currentPath).arg(i), this);
        MB_VERIFY(std::filesystem::create_directory(QString("%1_%2").arg(currentPath).arg(i).toStdString())
                          == true,
                  this);
    }
    // delete all folders
    for (int i = 1; i < 4; ++i)
    {
        MB_VERIFY(std::filesystem::remove(
                          std::filesystem::path(QString("%1_%2").arg(testPathQStr).arg(i).toStdString()))
                          == true,
                  this);
    }

    std::filesystem::create_directory("/tmp/onSubstituteNZBNameForExistingFileName");
#endif
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
