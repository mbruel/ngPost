#ifndef NGFILEPACKER_H
#define NGFILEPACKER_H

#include <QFileInfoList>
#include <QObject>
#include <QtGlobal>

#include "NgLogger.h"
#include "PostingParams.h"

class QProcess;
class QDir;

class PostingJob; // we're its friend!

/*!
 * \brief NgFilePacker is an active object used to pack a post using compression and/or par2 generation
 * It's quite linked to PostingJob (who has NgFilePacker as a friend). It could nearly be an inner child.
 * The packing could be more generic and be reused for another project.
 * This would be done if the need and the time were there...
 */
class NgFilePacker : public QObject
{
private:
    PostingJob      &_job; //!< NgFilePacker is really linked to PostingJob. could/should be more generic
    QFileInfoList   &_files;
    PostingParamsPtr _params;
    QString          _packingTmpPath; //!< absolute path where the packing will be done
    bool             _dispStdout;
    bool             _quietMode;

    QProcess *_extProc;          //!< process to launch compression and/or par2 asynchronously
    QDir     *_packingTmpDir;    //!< directory containing the archives
    bool      _limitProcDisplay; //!< limit external process output
    ushort    _nbProcDisp;       //!< hacky way to limit external process output

    static constexpr ushort kProcDisplayLimit = 42;

    QMap<QString, QString> _obfuscatedFileNames;

public:
    NgFilePacker(PostingJob &job, QString const &packingTmpPath, QObject *parent = nullptr);
    ~NgFilePacker();

    bool startCompressFiles();
    bool startGenPar2();

    void terminateAndWaitExternalProc();

private slots:
    void onCompressionFinished(int exitCode);
    void onGenPar2Finished(int exitCode);

    void onExtProcReadyReadStandardOutput();
    void onExtProcReadyReadStandardError();

private:
    void _createArchiveFolder();
    void _createExtProcAndConnectStreams();
    void _cleanExtProc();
    void _cleanCompressDir();

    /*!
     * \brief if we do some packing, the _files to be posted needs to be update
     * by thoses created in _packingTmpDir
     * return the list of the archiveNames
     * can be used twice if we both compress and generate the par2
     */
    void _updateFilesListFromCompressDir();

    inline void _log(QString const       &aMsg,
                     NgLogger::DebugLevel debugLvl,
                     bool                 newLine = true) const; //!< log function for QString
};

void NgFilePacker::_log(QString const &aMsg, NgLogger::DebugLevel debugLvl, bool newLine) const
{
    if (_quietMode)
        return;
    NgLogger::log(aMsg, newLine, debugLvl);
}

#endif // NGFILEPACKER_H
