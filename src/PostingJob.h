#ifndef POSTINGJOB_H
#define POSTINGJOB_H
#include <QFileInfoList>
#include <QVector>
#include <QSet>
#include <QQueue>
#include <QTextStream>
#include <QMutex>
#include <QTime>
class QProcess;
class NgPost;
class NntpConnection;
class NntpFile;
class NntpArticle;
class PostingWidget;

using QAtomicBool = QAtomicInteger<unsigned short>; // 16 bit only (faster than using 8 bit variable...)

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
    friend class NgPost;
    friend class PostingWidget;

private:
    NgPost *const _ngPost; //!< handle on the application to access global configs
    QFileInfoList _files; //!< populated on constuction using a QStringList of paths

    PostingWidget *_postWidget;

    QProcess   *_extProc;
    QDir       *_compressDir;
    bool        _limitProcDisplay;
    ushort      _nbProcDisp;

    QString     _tmpPath;
    QString     _rarPath;
    QString     _rarArgs;
    uint        _rarSize;
    uint        _par2Pct;

    bool        _doCompress;
    bool        _doPar2;
    bool        _genName;
    bool        _genPass;

    uint        _lengthName;
    uint        _lengthPass;
    QString     _rarName;
    QString     _rarPass;


    QVector<QThread*>        _threadPool;      //!< the connections are distributed among several threads
    QVector<NntpConnection*> _nntpConnections; //!< the NNTP connections (owning the TCP sockets)

    QString               _nzbName; //!< name of nzb that we'll write (without the extension)
    QQueue<NntpFile*>     _filesToUpload;  //!< list of files to upload (that we didn't start)
    QSet<NntpFile*>       _filesInProgress;//!< files in queue to be uploaded (Articles have been produced)
    int                   _nbFiles;  //!< number of files to post in this iteration
    int                   _nbPosted; //!< number of files posted


    const QString _nzbPath;
    QFile        *_nzb;       //!< nzb file that will be filled on the fly when a file is fully posted
    QTextStream  _nzbStream; //!< txt stream for the nzb file

    NntpFile    *_nntpFile; //!< current file that is getting processed
    QFile       *_file;     //!< file handler on the file getting processed
    char        *_buffer;   //!< buffer to read the current file
    int          _part;     //!< part number (Article) on the current file
    QMutex       _secureFile;

    QQueue<NntpArticle*> _articles; //!< prepared articles that are yEnc encoded
    QMutex               _secureArticlesQueue; //!< mutex to protect the Article stack (as the NntpConnection will pop from the ThreadPool)

    QTime       _timeStart; //!< to get some stats (upload time and avg speed)
    quint64     _totalSize; //!< total size (in Bytes) to be uploaded

    int     _nbConnections; //!< available number of NntpConnection (we may loose some)
    int     _nbThreads;     //!< size of the ThreadPool

    int       _nbArticlesUploaded; //!< number of Articles that have been uploaded (+ failed ones)
    int       _nbArticlesFailed;   //!< number of Articles that failed to be uploaded
    quint64   _uploadedSize;       //!< bytes posted (to compute the avg speed)
    int       _nbArticlesTotal;    //!< number of Articles of all the files to post


    QAtomicBool _stopPosting;
    QAtomicBool _noMoreFiles;


public:
    PostingJob(NgPost *ngPost,
               const QString &nzbPath,
               const QFileInfoList &files,
               PostingWidget *postWidget,
               const QString &tmpPath,
               const QString &rarPath,
               uint rarSize,
               uint par2Pct,
               bool doCompress,
               bool doPar2,
               bool genName,
               bool genPass,
               uint lengthName,
               uint lengthPass,
               const QString &rarName = "",
               const QString &rarPass = "",
               QObject *parent = nullptr);
    ~PostingJob();

    inline QString avgSpeed() const;

    NntpArticle *getNextArticle(const QString &threadName);


    inline void articlePosted(quint64 size);
    inline void articleFailed(quint64 size);

    int compressFiles();


signals:
    void startPosting();    //!< connected to onStartPosting (to be able to run on a different Thread)
    void stopPosting();

    void postingStarted();  //!< emitted at the end of onStartPosting
    void noMoreConnection();
    void postingFinished();

    void archiveFileNames(QStringList paths);
    void articlesNumber(int nbArticles);

    void scheduleNextArticle();

    void filePosted(QString filePath, int nbArticles, int nbFailed);



private slots:
    void onStartPosting();
    void onStopPosting(); //!< for HMI
    void onDisconnectedConnection(NntpConnection *con);

    void onPrepareNextArticle();

    void onNntpFileStartPosting();
    void onNntpFilePosted();

    void onExtProcReadyReadStandardOutput();
    void onExtProcReadyReadStandardError();




private:
    void _log(const QString &aMsg, bool newline = true) const; //!< log function for QString
    void _error(const QString &error) const;

    int  _createNntpConnections();
    void _startConnectionInThread(int conIdx, QThread *thread, const QString &threadName);

    void _prepareArticles();

    inline NntpFile *_getNextFile();
    NntpArticle *_getNextArticle(const QString &threadName);

    NntpArticle *_prepareNextArticle(const QString &threadName, bool fillQueue = true);


    void _initPosting();
    void _finishPosting();

    void _closeNzb();
    void _printStats() const;



    int _compressFiles(const QString &cmdRar,
                       const QString &tmpFolder,
                       const QString &archiveName,
                       const QString &pass,
                       uint volSize = 0);
    int _genPar2(const QString &tmpFolder,
                 const QString &archiveName,
                 uint redundancy = 0,
                 const QStringList &files = QStringList());

    bool _canCompress() const;
    bool _canGenPar2() const;

    void _cleanExtProc();
    void _cleanCompressDir();

    QString _createArchiveFolder(const QString &tmpFolder, const QString &archiveName);

    bool _checkTmpFolder() const;

};


QString PostingJob::avgSpeed() const
{
    QString power = " ";
    double bandwidth = 0.;

    if (!_timeStart.isNull())
    {
        double sec = _timeStart.elapsed()/1000.;
        bandwidth = _uploadedSize / sec;

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

NntpFile *PostingJob::_getNextFile()
{
    if (_filesToUpload.size())
    {
        NntpFile *file = _filesToUpload.dequeue();
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
}

void PostingJob::articleFailed(quint64 size)
{
    _uploadedSize += size;
    ++_nbArticlesUploaded;
    ++_nbArticlesFailed;
}


#endif // POSTINGJOB_H
