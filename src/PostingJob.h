//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
//
// This file is a part of ngPost : https://github.com/mbruel/ngPost
//
// ngPost is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; version 3.0 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// You should have received a copy of the GNU Lesser General Public
// License along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301,
// USA.
//
//========================================================================

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
class Poster;

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
    friend class Poster;
    friend class ArticleBuilder;

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
    bool        _useRarMax;
    uint        _par2Pct;

    bool        _doCompress;
    bool        _doPar2;
    QString     _rarName;
    QString     _rarPass;


    QVector<NntpConnection*> _nntpConnections; //!< the NNTP connections (owning the TCP sockets)

    QString               _nzbName; //!< name of nzb that we'll write (without the extension)
    QQueue<NntpFile*>     _filesToUpload;  //!< list of files to upload (that we didn't start)
    QSet<NntpFile*>       _filesInProgress;//!< files in queue to be uploaded (Articles have been produced)
    QSet<NntpFile*>       _filesFailed;//!< files that couldn't be read
    uint                  _nbFiles;  //!< number of files to post in this iteration
    uint                  _nbPosted; //!< number of files posted


    const QString _nzbFilePath;
    QFile        *_nzb;       //!< nzb file that will be filled on the fly when a file is fully posted
    QTextStream  _nzbStream; //!< txt stream for the nzb file

    NntpFile    *_nntpFile; //!< current file that is getting processed
    QFile       *_file;     //!< file handler on the file getting processed
    uint         _part;     //!< part number (Article) on the current file

    QTime       _timeStart; //!< to get some stats (upload time and avg speed)
    quint64     _totalSize; //!< total size (in Bytes) to be uploaded

    int     _nbConnections; //!< available number of NntpConnection (we may loose some)
    int     _nbThreads;     //!< size of the ThreadPool

    uint      _nbArticlesUploaded; //!< number of Articles that have been uploaded (+ failed ones)
    uint      _nbArticlesFailed;   //!< number of Articles that failed to be uploaded
    quint64   _uploadedSize;       //!< bytes posted (to compute the avg speed)
    uint      _nbArticlesTotal;    //!< number of Articles of all the files to post


    QAtomicBool _stopPosting;
    QAtomicBool _noMoreFiles;

    bool _postFinished;

    bool _obfuscateArticles;


    const bool _delFilesAfterPost;
    const QFileInfoList _originalFiles;


    QMutex _secureDiskAccess;

    QVector<Poster*> _posters;


public:
    PostingJob(NgPost *ngPost,
               const QString &nzbFilePath,
               const QFileInfoList &files,
               PostingWidget *postWidget,
               bool obfuscateArticles,
               const QString &tmpPath,
               const QString &rarPath,
               uint rarSize,
               bool useRarMax,
               uint par2Pct,
               bool doCompress,
               bool doPar2,
               const QString &rarName,
               const QString &rarPass,
               bool delFilesAfterPost = false,
               QObject *parent = nullptr);
    ~PostingJob();

    inline QString avgSpeed() const;

    inline void articlePosted(quint64 size);
    inline void articleFailed(quint64 size);

    inline uint nbArticlesTotal() const;
    inline uint nbArticlesUploaded() const;
    inline uint nbArticlesFailed() const;
    inline bool hasUploaded() const;

    inline QString nzbName() const;
    inline QString rarName() const;
    inline QString rarPass() const;
    inline QString postSize() const;

    inline bool hasCompressed() const;
    inline bool hasPostFinished() const;

    inline PostingWidget *widget() const;

signals:
    void startPosting();    //!< connected to onStartPosting (to be able to run on a different Thread)
    void stopPosting();

    void postingStarted();  //!< emitted at the end of onStartPosting
    void noMoreConnection();
    void postingFinished();

    void archiveFileNames(QStringList paths);
    void articlesNumber(uint nbArticles);

    void filePosted(QString filePath, uint nbArticles, uint nbFailed);


public slots:
    void onStopPosting(); //!< for HMI

private slots:
    void onStartPosting();
    void onDisconnectedConnection(NntpConnection *con);

    void onNntpFileStartPosting();
    void onNntpFilePosted();
    void onNntpErrorReading();

    void onExtProcReadyReadStandardOutput();
    void onExtProcReadyReadStandardError();

    void onCompressionFinished(int exitCode);
    void onGenPar2Finished(int exitCode);



private:
    void _log(const QString &aMsg, bool newline = true) const; //!< log function for QString
    void _error(const QString &error) const;

    int  _createNntpConnections();
    void _preparePostersArticles();

    NntpArticle *_readNextArticleIntoBufferPtr(const QString &threadName, char **bufferPtr);


    void _delOriginalFiles();

    inline NntpFile *_getNextFile();

    void _initPosting();
    void _postFiles();
    void _finishPosting();

    void _closeNzb();
    void _printStats() const;    


    bool startCompressFiles(const QString &cmdRar,
                            const QString &tmpFolder,
                            const QString &archiveName,
                            const QString &pass,
                            uint volSize = 0);
    bool startGenPar2(const QString &tmpFolder,
                      const QString &archiveName,
                      uint redundancy = 0);

    bool _canCompress() const;
    bool _canGenPar2() const;

    void _cleanExtProc();
    void _cleanCompressDir();

    QString _createArchiveFolder(const QString &tmpFolder, const QString &archiveName);

    bool _checkTmpFolder() const;


    qint64 _dirSize(const QString &path);


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

uint PostingJob::nbArticlesTotal() const { return _nbArticlesTotal; }
uint PostingJob::nbArticlesUploaded() const { return _nbArticlesUploaded; }
uint PostingJob::nbArticlesFailed() const{ return _nbArticlesFailed; }
bool PostingJob::hasUploaded() const{ return _nbArticlesTotal > 0; }

QString PostingJob::nzbName() const { return _nzbName; }
QString PostingJob::rarName() const { return _rarName; }
QString PostingJob::rarPass() const { return _rarPass; }
QString PostingJob::postSize() const
{
    QString unit = "B";
    double size = static_cast<double>(_totalSize);
    if (size > 1024)
    {
        size /= 1024;
        unit = "kB";
    }
    if (size > 1024)
    {
        size /= 1024;
        unit = "MB";
    }
    if (size > 1024)
    {
        size /= 1024;
        unit = "GB";
    }
    return QString("%1 %2").arg(size, 0, 'f', 2).arg(unit);
}

bool PostingJob::hasCompressed() const { return _doCompress; }
bool PostingJob::hasPostFinished() const { return _postFinished; }

PostingWidget *PostingJob::widget() const { return _postWidget; }
#endif // POSTINGJOB_H
