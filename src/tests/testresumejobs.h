#ifndef TESTRESUMEJOBS_H
#define TESTRESUMEJOBS_H
#include "tests/utils/MacroTest.h"

class NgPost;

class TestResumeJobs : public MacroTest
{
    Q_OBJECT
    inline static const QString kDBTestFilePath = "/tmp/ngPost_tstDB.sqlite";

public:
    TestResumeJobs(QString const &testName, int argc = 0, char *argv[] = nullptr);

    virtual void init() override;

    // called after each test case
    virtual void cleanup() override;

public slots:

private slots:
    void onTestUnfinishedBasicCase();
};

#endif // TESTRESUMEJOBS_H
