#include "MacroTest.h"
#include "Macros.h"
#include "NgHistoryDatabase.h"
#include "NgPost.h"

void MacroTest::cleanup()
{
    qDebug() << "Reset config ngPost...";
    _ngPost->resetConfig();
}

void MacroTest::_doInsertsIfProvided(QString const &sqlScript)
{
    qDebug() << "_doInsertsIfProvided" << sqlScript;
    int res = _db->_execSqlFile(sqlScript);
    MB_VERIFY(res == 0, this);
}

void MacroTest::_deletekDBTestFile()
{
    QFile f(kDBTestFilePath);
    bool  res = f.exists();
    if (res)
    {
        qDebug() << "[MB_TRACE] delete kDBTestFilePath: " << kDBTestFilePath;
        MB_VERIFY(f.remove(), this);
    }
}

bool MacroTest::_copyResourceFile(QString const &resoucePath, QString const &dstWritable)
{
    // Open the resource file for reading
    QFile nzbResourceFile(resoucePath);
    if (!nzbResourceFile.open(QIODevice::ReadOnly))
    {
        qDebug() << "Failed to open resource file for reading";
        return false;
    }

    // Open the writable file for writing
    QFile nzbWritableFile(dstWritable);
    bool  canWrite = nzbWritableFile.open(QIODevice::WriteOnly);
    if (!canWrite)
    {
        qDebug() << "Failed to open writable file for writing";
        nzbResourceFile.close();
        return false;
    }
    // Read the contents of the resource file and write them to the writable file
    while (!nzbResourceFile.atEnd())
    {
        QByteArray data = nzbResourceFile.read(1024);
        nzbWritableFile.write(data);
    }

    // Close the files
    nzbResourceFile.close();
    nzbWritableFile.close();

    qDebug() << "resource File " << resoucePath << " is copied successfully to " << dstWritable;
    return true;
}
