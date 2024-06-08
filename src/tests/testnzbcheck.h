#ifndef TESTNZBCHECK_H
#define TESTNZBCHECK_H
#include "tests/utils/MacroTest.h"
class NgPost;

class TestNzbGet : public MacroTest
{
    Q_OBJECT
public:
    TestNzbGet(QString const &testName, int argc = 0, char *argv[] = nullptr);

private slots:
    void onTestNzbCheckOK();
    void onTestNzbCheckKO();
};
#endif // TESTNZBCHECK_H
