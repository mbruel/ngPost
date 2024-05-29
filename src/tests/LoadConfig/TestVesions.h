#ifndef TestVesions_H
#define TestVesions_H
#include "tests/MacroTest.h"
class NgPost;

class TestVesions : public MacroTest
{
    Q_OBJECT
    NgPost *_ngPost;

public:
    TestVesions(QString const &testName, int argc, char *argv[]) : MacroTest(testName) { }

private slots:

    void onTestLoadDefautConfig();
    void onTestLoadOldConfig();
};

#endif // TestVesions_H
