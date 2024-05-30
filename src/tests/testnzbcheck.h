#ifndef TESTNZBCHECK_H
#define TESTNZBCHECK_H
#include "tests/MacroTest.h"
class NgPost;

class TestNzbGet : public MacroTest
{
    Q_OBJECT
public:
    TestNzbGet(QString const &testName, int argc, char *argv[]);
    ~TestNzbGet();
private slots:

    //    void init();    // called before each test case
    void cleanup() override; // called after each test case

    void onTestNzbCheckOK();
    void onTestNzbCheckKO();
    void onNzbCheckFinished(uint missingArticles);
};
#endif // TESTNZBCHECK_H
