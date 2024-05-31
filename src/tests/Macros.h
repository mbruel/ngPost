#ifndef MACROS_FOR_TESTING_H
#define MACROS_FOR_TESTING_H

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

#endif // MACROS_FOR_TESTING_H
