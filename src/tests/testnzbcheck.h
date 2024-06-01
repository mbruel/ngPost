#ifndef TESTNZBCHECK_H
#define TESTNZBCHECK_H
#include "tests/MacroTest.h"
class NgPost;

class TestNzbGet : public MacroTest
{
    Q_OBJECT
public:
    TestNzbGet(QString const &testName, int argc, char *argv[]);

private slots:
    void onTestNzbCheckOK();
    void onTestNzbCheckKO();
};
#endif // TESTNZBCHECK_H
