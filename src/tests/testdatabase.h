#ifndef TESTDATABASE_H
#define TESTDATABASE_H
#include "Macros.h"
#include "tests/MacroTest.h"

class NgPost;
class NgHistoryDatabase;

class TestNgHistoryDatabase : public MacroTest
{
    Q_OBJECT
    NgHistoryDatabase *_db;

    inline static const QString kDBTestFilePath = "/tmp/ngPost_tstDB.sqlite";

public:
    TestNgHistoryDatabase(QString const &testName, int argc = 0, char *argv[] = nullptr);

public slots:

    virtual void init() override;

    // called after each test case
    virtual void cleanup() override;

private slots:
    void onTestInitDB();

    void onListTablesAndRows();
    void onTestUnpostedBasicCase();

    void _deletekDBTestFile()
    {
        QFile f(kDBTestFilePath);
        bool  res = f.exists();
        if (res)
        {
            qDebug() << "[MB_TRACE] delete kDBTestFilePath: " << kDBTestFilePath;
            MB_VERIFY(f.remove(), this);
        }
    }
    bool _doInsertsIfProvided(QString const &sqlScript);
};

#endif // TESTDATABASE_H
