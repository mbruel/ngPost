//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
// This file is a part of ngPost : https://github.com/mbruel/ngPost
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3..
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>
//
//========================================================================
#ifndef POSTINGJOB_H
#define POSTINGJOB_H

#include <QElapsedTimer>
#include <QFileInfoList>
#include <QMutex>
#include <QQueue>
#include <QSet>
#include <QTextStream>
#include <QTime>
#include <QTimer>
#include <QVector>
class QProcess;

#include "PostingParams.h"
#include "utils/Macros.h"
#include "utils/NgLogger.h"

namespace NNTP
{
class Article;
class File;
} // namespace NNTP

class NgPost;
class NntpConnection;
class PostingWidget;
class Poster;

using AtomicBool = QAtomicInteger<unsigned short>; // 16 bit only (faster than using 8 bit variable...)

/*!
 * \brief PostingJob is an active object that will do a posting job
 * it will be possible to move it to a thread to not freeze the HMI
 * it will signal it's progressbarion to PostingWidget
 *
 * NgPost will only have one PostingJob active at a time
 * but it will be able to hold a Pending queue
 */
class PostingJob : public QObject
{
    Q_OBJECT
    friend class Poster;
    friend class ArticleBuilder;
    friend class NgPost;

signals:
    //! connected to onStartPosting (to be able to run on a different Thread)
    void startPosting(bool isActiveJob);
    void stopPosting(); //!< stop posting (pause by user (from GUI))

    void postingStarted();  //!< emitted at the end of onStartPosting
    void postingFinished(); //!< posting is finished

    //! to update the PostingWidget with the names of the archives that will be posted
    //! const ref even if it goes to main thread cause it won't be destroyed anytime soon
    void archiveFileNames(QStringList const &paths);
    void articlesNumber(uint nbArticles); //!< total number of articles for the progress bar

    //! to warn that a file is fully posted
    void filePosted(QString filePath, uint nbArticles, uint nbFailed);

    void packingDone(); //!< end of packing (compression and or par2)

private:
    NgPost &_ngPost; //!< handle on the application to access global configs

    PostingParamsPtr     _params;     //!< all posting parameters including the list of files
    PostingWidget *const _postWidget; //!< attached Windows

    QString _tmpPath;          //!< can be _params->tmpPath() or _params->ramPath()
    QString _archiveTmpFolder; //!< _tmpPath + name of folder

    /*!
     * \brief _files that will be posted by _postFiles()
     * they can be directly _params->files() if no packing (compression and/or par2)
     * most likely they will be replaced by the created archives located in _compressDir
     */
    QFileInfoList _files;
    AtomicBool    _delFilesAfterPost; //!< we're talking about the original files _params->files()

    QProcess *_extProc;          //!< process to launch compression and/or par2 asynchronously
    QDir     *_compressDir;      //!< directory containing the archives
    bool      _limitProcDisplay; //!< limit external process output
    ushort    _nbProcDisp;       //!< hacky way to limit external process output

    QVector<NntpConnection *> _nntpConnections;   //!< the NNTP connections (owning the TCP sockets)
    QVector<NntpConnection *> _closedConnections; //!< the NNTP connections (owning the TCP sockets)

    //    QString            _nzbName;         //!< name of nzb that we'll write (without the extension)
    QQueue<NNTP::File *> _filesToUpload;   //!< list of files to upload (that we didn't start)
    QSet<NNTP::File *>   _filesInProgress; //!< files in queue to be uploaded (Articles have been produced)
    QSet<NNTP::File *>   _filesFailed;     //!< files that couldn't be read
    uint                 _nbFiles;         //!< number of files to post in this iteration
    uint                 _nbPosted;        //!< number of files posted

    QFile      *_nzb;       //!< nzb file that will be filled on the fly when a file is fully posted
    QTextStream _nzbStream; //!< txt stream for the nzb file

    NNTP::File *_nntpFile; //!< current file that is getting processed
    QFile      *_file;     //!< file handler on the file getting processed
    uint        _part;     //!< part number (Article) on the current file

    QElapsedTimer _timeStart;     //!< to get some stats (upload time and avg speed)
    quint64       _totalSize;     //!< total size (in Bytes) to be uploaded
    QElapsedTimer _pauseTimer;    //!< to record the time when ngPost is in pause
    qint64        _pauseDuration; //!< total duration of all pauses

    int _nbConnections; //!< available number of NntpConnection (we may loose some)

    uint    _nbArticlesUploaded; //!< number of Articles that have been uploaded (+ failed ones)
    uint    _nbArticlesFailed;   //!< number of Articles that failed to be uploaded
    quint64 _uploadedSize;       //!< bytes posted (to compute the avg speed)
    uint    _nbArticlesTotal;    //!< number of Articles of all the files to post

    AtomicBool _stopPosting;
    AtomicBool _noMoreFiles;

    bool _postStarted;
    bool _packed;
    bool _postFinished;

    QMutex _secureDiskAccess;

    QVector<Poster *> _posters;

    QMap<QString, QString> _obfuscatedFileNames;

    bool _isPaused;

    QTimer _resumeTimer;

    bool _isActiveJob;

#ifdef __COMPUTE_IMMEDIATE_SPEED__
    quint64    _immediateSize; //!< bytes posted (to compute the avg speed)
    QTimer     _immediateSpeedTimer;
    QString    _immediateSpeed;
    bool const _useHMI;
#endif

public:
    /*!
     * \brief constructor from the GUI (postWidget)
     * \param ngPost
     * \param postWidget
     * \param params
     * \param parent
     */
    PostingJob(NgPost                 &ngPost,
               PostingWidget          *postWidget,
               PostingParamsPtr const &params,
               QObject                *parent = nullptr);

    /*!
     * \brief constructor from the CMD (no postWidget)
     * \param ngPost
     * \param postWidget
     * \param params
     * \param parent
     */
    PostingJob(NgPost                       &ngPost,
               QString const                &rarName,
               QString const                &rarPass,
               QString const                &nzbFilePath,
               QFileInfoList const          &files,
               QList<QString> const         &grpList,
               std::string const            &from,
               SharedParams const           &sharedParams,
               QMap<QString, QString> const &meta   = QMap<QString, QString>(),
               QObject                      *parent = nullptr);
    ~PostingJob();

    void pause();
    void resume();

    void                 dumpParams() const { _params->dumpParams(); }
    QFileInfoList const &paramFiles() const { return _params->files(); }

    QString getFilesPaths() const;

    bool tryResumePostWhenConnectionLost() const { return _params->tryResumePostWhenConnectionLost(); }

    inline QString avgSpeed() const;

    inline void articlePosted(quint64 size);
    inline void articleFailed(quint64 size);

    inline uint nbArticlesTotal() const { return _nbArticlesTotal; }
    inline uint nbArticlesUploaded() const { return _nbArticlesUploaded; }
    inline uint nbArticlesFailed() const { return _nbArticlesFailed; }
    inline bool hasUploaded() const { return _nbArticlesTotal > 0; }

    QString const &tmpPath() const { return _tmpPath; }

    QString               nzbName() const;
    inline QString const &nzbFilePath() const { return _params->nzbFilePath(); }
    inline QString const &rarName() const { return _params->rarName(); }
    inline QString const &rarPass() const { return _params->rarPass(); }
    QString               postSize() const;

    inline bool hasCompressed() const { return _params->hasCompressed(); }
    inline bool hasPacking() const { return _params->hasPacking(); }
    inline bool isPacked() const { return _packed; }
    inline bool hasPostStarted() const { return _postStarted; }
    inline bool hasPostFinished() const { return _postFinished; }
    inline bool hasPostFinishedSuccessfully() const { return _postFinished && !_nbArticlesFailed; }

    inline PostingWidget *widget() const { return _postWidget; }

    inline QString getFirstOriginalFile() const;

    inline void setDelFilesAfterPosted(bool delFiles) { _delFilesAfterPost = delFiles ? 0x1 : 0x0; }

    inline QString groups() const { return _params->groups(); }
    inline QString from(bool emptyIfObfuscateArticle) const { return _params->from(emptyIfObfuscateArticle); }

    inline bool isPosting() const { return MB_LoadAtomic(_stopPosting) == 0x0; }

    inline bool isPaused() const { return _isPaused; }

#ifdef __COMPUTE_IMMEDIATE_SPEED__
    QString const &immediateSpeed() const { return _immediateSpeed; }
#endif

#ifdef __USE_TMP_RAM__
    void shallWeUseTmpRam(); //!< if so update _tmpPath
#endif

    static QString sslSupportInfo();
    static bool    supportsSsl();

    void removeNonPosintingConnection(NntpConnection *nntpCon); //!< useless connection, we delete it

public slots:
    void onStopPosting(); //!< for HMI

private slots:
    /*!
     * \brief stating point of the Job. it might:
     *    - launch a compression process using _extProc with files stored in _compressDir
     *    - launch a par2 process using _extProc with files stored in _compressDir
     *    - post directly the files using _postFiles()
     *  Bare in mind that's asynchronous: the end of _extProc will trigger the next step
     *  cf startCompressFiles and startGenPar2 to know the receiving slots ;)
     * \param isActiveJob
     */
    void onStartPosting(bool isActiveJob);

    /*!
     * \brief end of compression process _extProc
     * \param exitCode
     */
    void onCompressionFinished(int exitCode);

    void onGenPar2Finished(int exitCode);

    void onDisconnectedConnection(NntpConnection *con);

    void onNntpFileStartPosting();
    void onNntpFilePosted();
    void onNntpErrorReading();

    void onExtProcReadyReadStandardOutput();
    void onExtProcReadyReadStandardError();

    void onResumeTriggered();

#ifdef __COMPUTE_IMMEDIATE_SPEED__
    void onImmediateSpeedComputation();
#endif

    void onPosterWithNoMorePostingConnection(Poster *poster);

private:
    void _log(QString const       &aMsg,
              NgLogger::DebugLevel debugLvl,
              bool                 newLine = true) const; //!< log function for QString

    void _init(); //!< mainly do the connection with itself, NgPost and PostingWidget

    void _initPosting();

    /*!
     * \brief actual method that will post the files on Usenet
     * the files are the one contained in _files
     */
    void _postFiles();
    void _finishPosting();

    int  _createNntpConnections();
    void _preparePostersArticles();

    NNTP::Article *_readNextArticleIntoBufferPtr(QString const &threadName, char **bufferPtr);

    void _delOriginalFiles();

    inline NNTP::File *_getNextFile();

    void _closeNzb();
    void _printStats() const;

    bool startCompressFiles();
    bool startGenPar2();

    void _cleanExtProc();
    void _cleanCompressDir();

    /*!
     * \brief if we do some packing, the _files to be posted needs to be update
     * by thoses created in _compressDir
     * return the list of the archiveNames
     * can be used twice if we both compress and generate the par2
     */
    QStringList _updateFilesListFromCompressDir();

    QString _createArchiveFolder(QString const &tmpFolder, QString const &archiveName);

    inline QString timestamp() const { return QTime::currentTime().toString("hh:mm:ss.zzz"); }
};

QString PostingJob::avgSpeed() const
{
    QString power     = " ";
    double  bandwidth = 0.;

    if (_timeStart.isValid())
    {
        double sec = (_timeStart.elapsed() - _pauseDuration) / 1000.;
        bandwidth  = _uploadedSize / sec;

        if (bandwidth > 1024)
        {
            bandwidth /= 1024;
            power = "k";
        }
        if (bandwidth > 1024)
        {
            bandwidth /= 1024;
            power = "M";
        }
    }

    return QString("%1 %2B/s").arg(bandwidth, 6, 'f', 2).arg(power);
}

NNTP::File *PostingJob::_getNextFile()
{
    if (_filesToUpload.size())
    {
        NNTP::File *file = _filesToUpload.dequeue();
        _filesInProgress.insert(file);
        return file;
    }
    else
        return nullptr;
}

void PostingJob::articlePosted(quint64 size)
{
    _uploadedSize += size;
    ++_nbArticlesUploaded;
#ifdef __COMPUTE_IMMEDIATE_SPEED__
    if (_useHMI || _params->dispProgressBar())
        _immediateSize += size;
#endif
}

void PostingJob::articleFailed(quint64 size)
{
    _uploadedSize += size;
    ++_nbArticlesUploaded;
    ++_nbArticlesFailed;
#ifdef __COMPUTE_IMMEDIATE_SPEED__
    if (_useHMI || _params->dispProgressBar())
        _immediateSize += size;
#endif
}

QString PostingJob::getFirstOriginalFile() const
{
    if (_params->files().isEmpty())
        return QString();
    else
        return _params->files().first().absoluteFilePath();
}

#endif // POSTINGJOB_H
