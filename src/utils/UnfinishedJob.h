#ifndef UNFINISHEDJOB_H
#define UNFINISHEDJOB_H
#include <QCoreApplication>
#include <QDateTime>

#include "NgLogger.h"
class UnfinishedJob // seems struct can't use Q_DECLARE_TR_FUNCTIONS...
{
    Q_DECLARE_TR_FUNCTIONS(UnfinishedJob); // tr() without QObject using QCoreApplication::translate

private:
    QString _logPrefix;

public:
    qint64    jobIdDB;
    QDateTime date;
    QString   packingPath;
    QString   nzbFilePath;
    QString   nzbName;
    qint64    size;
    QString   groups;
    ushort    state;
    QString   archiveName;
    QString   archivePass;

    mutable QStringList filesToPost; //!< could be modified by canBeResumed

    //! missing files are set later to do only one SQL request on tUnfinishedFiles
    void setFilesToPost(QStringList const &files) { filesToPost = files; }

    bool canBeResumed() const;

    bool hasEmptyPackingPath() const;

    bool isCompressed() const;
    bool packingDone() const;
    bool nzbStarted() const;

    QString str() const;
    void    dump(bool dispIfCanBeResumed, NgLogger::DebugLevel debugLevel = NgLogger::DebugLevel::Info) const
    {
        if (dispIfCanBeResumed)
            NgLogger::log(
                    tr("%1 can be resumed: %2").arg(str()).arg(canBeResumed() ? "Yes" : "No"), true, debugLevel);
        else
            NgLogger::log(str(), true, debugLevel);
    }

    // Constructor
    UnfinishedJob(qint64           jobId,
                  QDateTime const &dt,
                  QString const   &tmp,
                  QString const   &nzbFile,
                  QString const   &nzbName,
                  qint64           s,
                  QString const   &grps,
                  ushort           state_,
                  QString const   &aName,
                  QString const   &aPass);

    ~UnfinishedJob()                                = default;
    UnfinishedJob(UnfinishedJob const &)            = default;
    UnfinishedJob(UnfinishedJob &&)                 = default;
    UnfinishedJob &operator=(UnfinishedJob const &) = default;
    UnfinishedJob &operator=(UnfinishedJob &&)      = default;

private:
    bool hasFilesToPost() const { return !filesToPost.isEmpty(); }
    bool areFilesToPostAvailable() const;

    bool   _checkNzbExistsAndHasReadWritePermission() const;
    bool   _checkPackingExistAndNotEmpty() const;
    void   _removePar2FromFilesToPost() const;
    ushort _deleteExistingPar2() const;
};

using UnfinishedJobs = QList<UnfinishedJob>; // we don't have the number in advance

#endif // UNFINISHEDJOB_H
