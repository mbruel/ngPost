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
    QString   packingPath;
    QString   nzbFilePath;
    QString   nzbName;
    qint64    size;
    QString   groups;
    ushort    state;

    bool couldBeResumed() const;

    bool hasEmptyPackingPath() const;

    bool packingDone() const;
    bool nzbStarted() const;

    // Constructor
    UnfinishedJob(qint64           jobId,
                  QDateTime const &dt,
                  QString const   &tmp,
                  QString const   &nzbFile,
                  QString const   &nzbName,
                  qint64           s,
                  QString const   &grps,
                  ushort           state_);
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
        "SELECT job_id, filePath FROM tUnfinishedFiles WHERE (job_id = :job_id)";
inline const QString kInsertJob = QString("\
insert into tHistory\
    (date, nzbName, size, avgSpeed, archiveName, archivePass, groups, poster, packingPath, nzbFilePath, nbFiles, state)\
values\
    (:date, :nzbName, :size, :avgSpeed, :archiveName, :archivePass, :groups, :from, :packingPath, :nzbFilePath, :nbFiles, :state);\
");
inline const QString kInsertUnfinishedStatement =
        "insert into tUnfinishedFiles (job_id, filePath) values (:job_id, :filePath); ";

inline const QString kSqlSelectUnfinishedStatement =
        "SELECT id, date, packingPath, nzbFilePath, nzbName, size, groups, state "
        "FROM tHistory WHERE state < :posted_sate;";
inline const QString kSqlNumberUnfinishedStatement =
        "SELECT count(*) FROM tHistory WHERE state < :posted_sate ;";

inline const QString kSqlUpdateHistoryDoneUnfinishedStatement =
        "UPDATE tHistory SET state = :value WHERE id = :job_id";
inline const QString kSqlDeleteUnfinisheFilesdStatement = "DELETE FROM tUnfinishedFiles WHERE job_id = :job_id";

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
