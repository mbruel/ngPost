#include <QCoreApplication>
#include <QDebug>
#include <QTest>
#include <QtTest/QtTest>

#include "LoadConfig/TestVesions.h"
#include "MacroTest.h"
#include "testNgTools.h"
#include "testnzbcheck.h"

#include "NgPost.h"
#include "TestUtils.h"

inline void launchTest(MacroTest *test, QList<QString> &failedTests)
{
    int res = QTest::qExec(test);
    qDebug() << "[" << test->metaObject()->className() << "] QTest::qExec return: " << res
             << ", nbFailure: " << test->nbFailure() << " on " << test->nbVerifications() <<
        " verifs on a total of " << test->nbUseCases() << " usecases.";
    if (test->nbFailure() != 0)
        failedTests << test->metaObject()->className();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qputenv("QTEST_FUNCTION_TIMEOUT", "900000"); // 5 min

    QList<QString> failedTests;
#ifdef __Launch_TestNgTools__
    {
        TestNgTools test("TestNgTools");
        launchTest(&test, failedTests);
    }
#endif

#ifdef __Launch_TestVesions__
    {
        TestVesions test("TestVesions");
        launchTest(&test, failedTests);
    }
#endif

#ifdef __Launch_TestNzbCheck__
    {
        TestNzbGet test("TestNzbGet");
        launchTest(&test, failedTests);
    }
#endif

    // Add more test cases here as needed
    // ...

    qDebug() << "\n\n\n\n";
    qDebug() << "###############################################";
    qDebug() << "# Overall Tests Results:";
    qDebug() << "#     - number of Macro Tests: " << MacroTest::numberMacroTestsRun();
    qDebug() << "#     - number of Macro Tests FAILED: " << MacroTest::numberMacroTestsFailed();
    qDebug() << "#\n";
    qDebug() << "#     - number of Unit Tests: " << MacroTest::numberOfUnitTestsRun();
    qDebug() << "#     - number of Unit Tests FAILED: " << MacroTest::numberOfUnitTestsFAIL();
    if (!failedTests.isEmpty())
        qDebug() << "# List of FAILED macro Tests: " << failedTests.join(", ");
    qDebug() << "###############################################";

    return (int)MacroTest::numberOfUnitTestsFAIL();
}
