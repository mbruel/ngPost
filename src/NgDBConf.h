#ifndef NGDBCONF_H
#define NGDBCONF_H
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
class UnfinishedJob // seems struct can't use Q_DECLARE_TR_FUNCTIONS...
{
    Q_DECLARE_TR_FUNCTIONS(UnfinishedJob); // tr() without QObject using QCoreApplication::translate

public:
    qint64    jobIdDB;
    QDateTime date;
    QString   tmpPath;
    QString   nzbFilePath;
    QString   nzbName;
    qint64    size;
    QString   groups;
    QFileInfo _packingPath;

    bool             couldBeResumed() const;
    QFileInfo const &filesDir() const { return _packingPath; }

    bool hasEmptyPackingPath() const;

    // Constructor
    UnfinishedJob(qint64           jobId,
                  QDateTime const &dt,
                  QString const   &tmp,
                  QString const   &nzbFile,
                  QString const   &nzbName,
                  qint64           s,
                  QString const   &grps);
};
using UnfinishedJobs = QList<UnfinishedJob>; // we don't have the number in advance

namespace NgConf
{
namespace DB
{
namespace SQL
{
inline const QStringList kPragmas = { "PRAGMA journal_mode = WAL",
                                      "PRAGMA wal_autocheckpoint = 16",
                                      "PRAGMA journal_size_limit = 1536" };

inline const QString kSelectSizes = "SELECT size FROM tHistory";
inline const QString kMissingFilesForJob =
        "SELECT job_id, filePath FROM tUnpostedFiles WHERE (job_id = :job_id)";
inline const QString kInsertJob = QString("\
insert into tHistory\
    (date, nzbName, size, avgSpeed, archiveName, archivePass, groups, poster, tmpPath, nzbFilePath, nbFiles, done)\
values\
    (:date, :nzbName, :size, :avgSpeed, :archiveName, :archivePass, :groups, :from, :tmpPath, :nzbFilePath, :nbFiles, :done);\
");
inline const QString kInsertUnpostedStatement =
        "insert into tUnpostedFiles (job_id, filePath) values (:job_id, :filePath); ";
inline const QString kSqlSelectUnpostedStatement =
        "SELECT id, date, tmpPath, nzbFilePath, nzbName, size, groups FROM tHistory WHERE done = 0;";

inline const QString kYearCondition  = "strftime('%Y',date) = strftime('%Y',date('now'))";
inline const QString kMonthCondition = "strftime('%Y',date) = strftime('%Y',date('now')) \
    AND strftime('%m',date) = strftime('%m',date('now'))";
inline const QString kWeekCondition  = "strftime('%Y',date) = strftime('%Y',date('now')) \
    AND strftime('%W',date) = strftime('%W',date('now'))";
inline const QString kDayCondition   = "strftime('%Y',date) = strftime('%Y',date('now')) \
    AND strftime('%m',date) = strftime('%m',date('now'))\
    AND strftime('%d',date) = strftime('%d',date('now'))";

} // namespace SQL
} // namespace DB
} // namespace NgConf

#endif // NGDBCONF_H
