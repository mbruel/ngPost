#ifndef NGDBCONF_H
#define NGDBCONF_H
#include <QStringList>

namespace NgConf
{
namespace DB
{
namespace SQL
{
inline const QStringList kPragmas = { "PRAGMA journal_mode = WAL",
                                      "PRAGMA wal_autocheckpoint = 16",
                                      "PRAGMA journal_size_limit = 1536" };

inline const QString kSelectStatistics = "\
SELECT \
    min(date) as dateFirst,\
    count(*) as nbPosts,\
    sum(sizeMB) as sumSizeMB, avg(sizeMB) as avgSizeMB,\
    avg(avgSpeedKbps) as avgSpeedKbps \
FROM tHistory";
inline const QString kMissingFilesForJob =
        "SELECT job_id, filePath FROM tUnfinishedFiles WHERE (job_id = :job_id)";
inline const QString kInsertJob = "\
insert into tHistory\
    (date, nzbName, sizeMB, avgSpeedKbps, archiveName, archivePass, groups, \
    poster, packingPath, nzbFilePath, nbFiles, state)\
values\
    (:date, :nzbName, :sizeMB, :avgSpeedKbps, :archiveName, :archivePass, :groups,\
     :from, :packingPath, :nzbFilePath, :nbFiles, :state);";
inline const QString kInsertUnfinishedStatement =
        "insert into tUnfinishedFiles (job_id, filePath) values (:job_id, :filePath); ";

inline const QString kSqlSelectUnfinishedStatement =
        "SELECT id, date, packingPath, nzbFilePath, nzbName, sizeMB, groups, state, archiveName, archivePass "
        "FROM tHistory WHERE state < :posted_sate order by state DESC";
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
