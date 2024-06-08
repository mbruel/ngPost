#ifndef TESTNGTOOLS_H
#define TESTNGTOOLS_H
#include "tests/utils/MacroTest.h"
class NgPost;

class TestNgTools : public MacroTest
{
    Q_OBJECT
public:
    TestNgTools(QString const &testName, int argc = 0, char *argv[] = nullptr);

private slots:
#ifdef __Launch_TestLocalConfig__
    void onTestLoadDefautConfig();
#endif
    void onTestLoadOldConfig();

    void onSubstituteExistingFile();
    void onTestgetUShortVersion(QString const &version);

    void onCheckForNewVesion();
    void onCheckForNewVersionReply();
};

// #include <QFileInfo>
// class TestNgTools : public MacroTest
//{
//     Q_OBJECT

// public:
//     TestNgTools(QString const &testName, int argc, char *argv[]);

//    uint onTestIsConfigurationVesionObsolete();

//    QString onTestCurrentDateTime();

//    void onTestRemoveAccentsFromString(QString &str);

//    qint64 onTestRecursivePathSize(QFileInfo const &fi);

//    QString onTestHumanSize(double size);

//    std::string onTestRandomStdFrom(ushort length = 13);

//    QString onTestHumanSize(qint64 size);
//    QString onTestRandomFrom(ushort length);

//    QString onTestRandomFileName(uint length = 13);

//    QString ronTestRandomPass(uint length = 13);

//    QString onTestGgtConfigurationFilePath();

//    QString onTestEscapeXML(char const *str);
//    QString onTestEscapeXML(QString const &str);
//    QString onTestXml2txt(char const *str);
//    QString onTestXml2txt(QString const &str);

//    QString onTestPtrAddrInHex(void *ptr);

#endif // TESTNGTOOLS_H
