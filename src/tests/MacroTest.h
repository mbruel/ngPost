#ifndef MACROTEST_H
#define MACROTEST_H

#include <QDebug>
#include <QObject>
#include <QString>

#include <QtTest/QtTest> // so we can define initTestCase, cleanupTestCase...

#include "NgPost.h"

// show MB_LOGs
#define __DISP_LOGS__

class MacroTest : public QObject
{
    Q_OBJECT
protected:
    NgPost *_ngPost;

public slots:

    //!< Called once before the test cases
    //! => Create the ngPost instance
    virtual void initTestCase() { NgLogger::createInstance(); }

    virtual void cleanupTestCase() // Called once after all test cases
    {
        NgLogger::destroy();
    }
    //! called before each test case
    //! (ngPost needs argc, argv for its creation => done in child constructor)
    inline virtual void init() { }
    inline virtual void cleanup()
    {
        qDebug() << "Leaking ngPost...";
        //_ngPost->deleteLater();
    }
    // called after each test case

private:
    inline static uint sNbMacroTestsRun  = 0;
    inline static uint sNbMacroTestsFAIL = 0;

    inline static uint sNbUnitTestsRun  = 0;
    inline static uint sNbUnitTestsFAIL = 0;

    ushort _nbUseCases      = 0; //!<
    ushort _nbVerifications = 0;
    ushort _nbFailure       = 0;

    const QString _testName;        //!< test name
    const uint    _projectId = 666; //!< unused yet

public:
    explicit MacroTest(QString const &testName)
        : _ngPost(nullptr), _nbUseCases(0), _nbVerifications(0), _nbFailure(0), _testName(testName)
    {
        ++sNbMacroTestsRun;
    }
    virtual ~MacroTest()
    {
        if (_nbFailure != 0)
            ++sNbMacroTestsFAIL;
        dump();
    };

    void dump()
    {
        qDebug() << "\n\n\n\n";
        qDebug() << "###############################################";
        qDebug() << "# Results Macro Test: " << _testName;
        qDebug() << "#     - number of use cases: " << _nbUseCases;
        qDebug() << "#     - number of checks: " << _nbVerifications;
        qDebug() << "#     - number of Failures: " << _nbFailure;
        qDebug() << "###############################################";
    }

    void newUseCases() { ++_nbUseCases; }
    void newVerifications() { ++_nbVerifications; }
    void newFailure() { ++_nbFailure; }

    QString name() const { return _testName; }

    static void incrementNumerOfUnitTestRun() { ++sNbUnitTestsRun; }
    static void incrementNumerOfUnitTestFailed() { ++sNbUnitTestsFAIL; }

    static ushort numberMacroTestsRun() { return sNbMacroTestsRun; }
    static ushort numberMacroTestsFailed() { return sNbMacroTestsFAIL; }

    static ushort numberOfUnitTestsRun() { return sNbUnitTestsRun; }
    static ushort numberOfUnitTestsFAIL() { return sNbUnitTestsFAIL; }
};

//!< Macro that use variadic template log methods with __FILE__, __LINE__ in bonus ;)
#define MB_LOG(...)                                 \
    do                                              \
    {                                               \
        mbLogImpl(__FILE__, __LINE__, __VA_ARGS__); \
    } while (false)

template <typename T, typename... Args>
void mbLogImpl(char const *file, int line, T firstArg, Args... args)
{
#ifdef __DISP_LOGS__
    qDebug().nospace().noquote() << "[MB_LOG] " << file << ":" << line << " " << firstArg << " ";
#endif
    if constexpr (sizeof...(args) > 0)
    {
        mbLogImpl(file, line, args...);
    }
}

template <typename T>
void mbLogImpl(char const *file, int line, T arg)
{
#ifdef __DISP_LOGS__
    qDebug().nospace().noquote() << "[MB_LOG] " << file << ":" << line << " v = " << arg << Qt::endl;
#endif
}

//!< to use QTest::qVerify and save the results in currentTest and MacroTest::incrementNumerOfUnitTestFailed
#define MB_VERIFY(statement, currentTest)                                                      \
    currentTest->newUseCases();                                                                \
    do                                                                                         \
    {                                                                                          \
        if (!QTest::qVerify(static_cast<bool>(statement), #statement, "", __FILE__, __LINE__)) \
        {                                                                                      \
            currentTest->newFailure();                                                         \
            currentTest->incrementNumerOfUnitTestFailed();                                     \
            return;                                                                            \
        }                                                                                      \
        currentTest->incrementNumerOfUnitTestRun();                                            \
    } while (false)

#endif // MACROTEST_H
