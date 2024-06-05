#ifndef TestConfig_H
#define TestConfig_H
#include "tests/MacroTest.h"
class NgPost;

class TestConfig : public MacroTest
{
    Q_OBJECT
public:
    TestConfig(QString const &testName, int argc = 0, char *argv[] = nullptr);

private slots:

    void onTestLoadDefautConfig();
    void onTestLoadOldConfig();
    void onTestLoadXSNewsPartnerConfig();
    void onTestLoadXSNewsPartnerConfAndCheckConnection();
    void onTestPostingNotAllowed();
};

#endif // TestConfig_H
