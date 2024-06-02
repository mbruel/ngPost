#ifndef TestVesions_H
#define TestVesions_H
#include "tests/MacroTest.h"
class NgPost;

class TestVesions : public MacroTest
{
    Q_OBJECT
public:
    TestVesions(QString const &testName, int argc = 0, char *argv[] = nullptr);

private slots:

    void onTestLoadDefautConfig();
    void onTestLoadOldConfig();
    void onTestLoadXSNewsPartnerConfig();
    void onTestLoadXSNewsPartnerConfAndCheckConnection();
};

#endif // TestVesions_H
