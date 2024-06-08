#include <csignal>

#include <QCoreApplication>
#include <QDebug>
#include <QTest>
#include <QtTest/QtTest>

#include "LoadConfig/TestConfig.h"
#include "NgPost.h"
#include "testdatabase.h"
#include "testNgTools.h"
#include "testnzbcheck.h"
#include "testresumejobs.h"
#include "utils/MacroTest.h"
#include "utils/TestUtils.h"

static QList<QString> sFailedTests;

inline void launchTest(MacroTest *test)
{
    int res = QTest::qExec(test);
    qDebug() << "[" << test->metaObject()->className() << "] QTest::qExec return: " << res
             << ", nbFailure: " << test->nbFailure() << " on " << test->nbVerifications()
             << " verifs on a total of " << test->nbUseCases() << " usecases.";
    if (test->nbFailure() != 0)
        sFailedTests << test->metaObject()->className();
}

void handleShutdown(int signal)
{
    Q_UNUSED(signal)
    qCritical() << "Signal caught, closing the application...\n";
    qApp->quit();
}

void printSumUp()
{
    qDebug() << "\n\n\n\n";
    qDebug() << "###############################################";
    qDebug() << "# Overall Tests Results:";
    qDebug() << "#     - number of Macro Tests: " << MacroTest::numberMacroTestsRun();
    qDebug() << "#     - number of Macro Tests FAILED: " << MacroTest::numberMacroTestsFailed();
    qDebug() << "#\n";
    qDebug() << "#     - number of Unit Tests: " << MacroTest::numberOfUnitTestsRun();
    qDebug() << "#     - number of Unit Tests FAILED: " << MacroTest::numberOfUnitTestsFAIL();
    if (!sFailedTests.isEmpty())
        qDebug() << "# List of FAILED macro Tests: " << sFailedTests.join(", ");
    qDebug() << "###############################################";
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qputenv("QTEST_FUNCTION_TIMEOUT", "42000"); // 42 sec

    signal(SIGINT, &handleShutdown);  // shut down on ctrl-c
    signal(SIGTERM, &handleShutdown); // shut down on killall

#ifdef __Launch_TestNgTools__
    {
        TestNgTools test("TestNgTools");
        launchTest(&test);
    }
#endif

#ifdef __Launch_TestConfig__
    {
        TestConfig test("TestConfig");
        launchTest(&test);
    }
#endif

#ifdef __Launch_TestNzbCheck__
    {
        TestNzbGet test("TestNzbGet");
        launchTest(&test);
    }
#endif

#ifdef __Launch_TestDatabase__
    {
        TestNgHistoryDatabase test("TestNgHistoryDatabase");
        launchTest(&test);
    }
#endif

#ifdef __Launch_TestResumeJobs__
    {
        TestResumeJobs test("TestResumeJobs");
        launchTest(&test);
    }
#endif

    // Add more test cases here as needed
    // ...

    printSumUp();

    return (int)MacroTest::numberOfUnitTestsFAIL();
}
