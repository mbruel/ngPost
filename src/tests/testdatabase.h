#ifndef TESTDATABASE_H
#define TESTDATABASE_H
#include "tests/utils/Macros.h"
#include "tests/utils/MacroTest.h"

class NgPost;
class NgHistoryDatabase;

class TestNgHistoryDatabase : public MacroTest
{
    Q_OBJECT
public:
    TestNgHistoryDatabase(QString const &testName, int argc = 0, char *argv[] = nullptr);

public slots:

    virtual void init() override;

    // called after each test case
    virtual void cleanup() override;

private slots:
    void onTestInitDB();

    void onListTablesAndRows();
    void onTestUnfinishedBasicCase();
};

#endif // TESTDATABASE_H
