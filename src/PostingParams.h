//========================================================================
//
// Copyright (C) 2024 Matthieu Bruel <Matthieu.Bruel@gmail.com>
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
#ifndef POSTINGPARAMS_H
#define POSTINGPARAMS_H

#include <QFileInfoList>
#include <QList>
#include <QSharedData>
#include <QString>

#include "NgConf.h"

#ifdef __USE_TMP_RAM__
class QStorageInfo;
#endif
namespace NNTP
{
struct ServerParams;
}
class PostingParams;
class MainParams;
class PostingWidget;
class NgPost;

// SharedParams is shared by all PostingJobs::PostingParamsPtr
// if a PostingJobs wants a variant of the MainParams it will have to explicitely call detach()
using SharedParams = QExplicitlySharedDataPointer<MainParams>;

// shared between a PostingJobs and its PostingWidget
#ifndef _MSC_VER // MB_TODO: investigate why MSVC compiler has an issue with QSharedPointer and initialization...
#  include <QSharedPointer>
using PostingParamsPtr = QSharedPointer<PostingParams>;
#else
#  include <memory>
using PostingParamsPtr = std::shared_ptr<PostingParams>;
#endif

class MainParams : public QSharedData
{
    Q_DECLARE_TR_FUNCTIONS(MainParams); // tr() without QObject using QCoreApplication::translate

    friend class NgConfigLoader;  // direct access to set the attributes
    friend class NgCmdLineLoader; // direct access to set the attributes
    friend class NgPost;          // _postingParams->setPar2Path(par2Embedded);

private:
    bool _quiet;
    bool _dispProgressBar;
    bool _dispFilesPosting;

    QList<NNTP::ServerParams *> _nntpServers; //!< the parameters of the available servers

#ifdef __USE_TMP_RAM__
    QStorageInfo *_storage;
    QString       _ramPath;
    double        _ramRatio;
#endif

    ushort  _nbThreads;     //!< size of the ThreadPool
    int     _socketTimeOut; //!< socket timeout
    QString _nzbPath;       //!< default path where to write the nzb files
    QString _nzbPathConf;   //!< default path where to write the nzb files

    QString     _tmpPath; //!< can be overwritten by _ngPost->_ramPath
    QString     _rarPath;
    QStringList _rarArgs;
    ushort      _rarSize;
    bool        _useRarMax;
    ushort      _rarMax;
    ushort      _par2Pct;
    QString     _par2Path;
    QString     _par2Args;
    QString     _par2PathConfig;

    ushort  _lengthName;
    ushort  _lengthPass;
    QString _rarPassFixed;

    bool        _doCompress;
    bool        _doPar2;
    bool        _genName;
    bool        _genPass;
    bool        _keepRar;
    bool        _packAuto;
    QStringList _packAutoKeywords;

    bool _obfuscateArticles; //!< shall we obfuscate each Article (subject + from)
    bool _obfuscateFileName; //!< for single file or folder, rename it with a random name prior to compress it
    bool _delFilesAfterPost;
    bool _overwriteNzb;

    bool _use7z;

    QUrl   *_urlNzbUpload;
    QString _urlNzbUploadStr;

    bool   _tryResumePostWhenConnectionLost;
    ushort _waitDurationBeforeAutoResume;

    QStringList _nzbPostCmd;
    bool        _preparePacking;

    bool        _monitorNzbFolders;
    QStringList _monitorExtensions;
    bool        _monitorIgnoreDir;
    ushort      _monitorSecDelayScan;

    bool _removeAccentsOnNzbFileName;
    bool _autoCloseTabs;
    bool _rarNoRootFolder;

    NgConf::GROUP_POLICY _groupPolicy;

    bool        _genFrom;
    bool        _saveFrom;
    std::string _from; //!< email of poster (if empty, random one will be used for each file)

    QStringList _grpList; //!< Newsgroup where we're posting in a list format to write in the nzb file
    int         _nbGroups;

    QString _inputDir; //!< Default folder to open to select files from the HMI

    bool _delAuto; //!< shall we delete file/folder once posted (only for --auto and --monitor)

public:
    // ALL PUBLIC METHODS ARE CONST AND NEED TO RETURN A CONST TO NOT DETACH !...

    MainParams();
    ~MainParams();

    MainParams(MainParams const &)            = default;
    MainParams(MainParams &&)                 = default;
    MainParams &operator=(MainParams const &) = delete;
    MainParams &operator=(MainParams &&)      = delete;

    static MainParams *resumeParams();

    inline int         nbThreads() const { return _nbThreads; }
    inline int         getSocketTimeout() const { return _socketTimeOut; }
    QString const     &nzbPath() const { return _nzbPath; }
    QStringList const &nzbPostCmd() const { return _nzbPostCmd; }

    bool doCompress() const { return _doCompress; }
    bool doPar2() const { return _doPar2; }
    bool genName() const { return _genName; }
    bool genPass() const { return _genPass; }
    bool keepRar() const { return _keepRar; }
    bool packAuto() const { return _packAuto; }

    QString inputDir() const { return _inputDir; }
    bool    delAuto() const { return _delAuto; }
    ushort  monitorSecDelayScan() const { return _monitorSecDelayScan; }
    bool    quietMode() const { return _quiet; }
    bool    dispProgressBar() const { return _dispProgressBar && !_quiet; }
    bool    dispFilesPosting() const { return _dispFilesPosting && !_quiet; }
    void    setDisplayProgress(QString const &txtValue);
    void    beQuiet()
    {
        _quiet            = true;
        _dispProgressBar  = false;
        _dispFilesPosting = false;
    }

    QStringList const   &groupList() const { return _grpList; }
    NgConf::GROUP_POLICY groupPolicy() const { return _groupPolicy; }

#ifdef __USE_TMP_RAM__
    qint64 ramAvailable() const;
    bool   useTmpRam() const { return _storage != nullptr; }
    double ramRatio() const { return _ramRatio; }

    QString const &ramPath() const { return _ramPath; }
#endif

    QString const     &rarPath() const { return _rarPath; }
    QStringList const &rarArgs() const { return _rarArgs; }
    ushort             rarSize() const { return _rarSize; }
    bool               useRarMax() const { return _useRarMax; }
    ushort             rarMax() const { return _rarMax; }
    ushort             par2Pct() const { return _par2Pct; }

    ushort         lengthName() const { return _lengthName; }
    ushort         lengthPass() const { return _lengthPass; }
    QString const &rarPassFixed() const { return _rarPassFixed; }

    QString const &par2Path() const { return _par2Path; }
    QString const &par2Args() const { return _par2Args; }
    QString const &par2PathConfig() const { return _par2PathConfig; }

    QString const &tmpPath() const { return _tmpPath; }

    QString     urlNzbUploadStr() const { return _urlNzbUploadStr; }
    QUrl const *urlNzbUpload() const { return _urlNzbUpload; }

    bool               monitorNzbFolders() const { return _monitorNzbFolders; }
    QStringList const &monitorExtensions() const { return _monitorExtensions; }
    bool               monitorIgnoreDir() const { return _monitorIgnoreDir; }

    bool obfuscateArticles() const { return _obfuscateArticles; }
    bool obfuscateFileName() const { return _obfuscateFileName; }
    bool delFilesAfterPost() const { return _delFilesAfterPost; }
    bool overwriteNzb() const { return _overwriteNzb; }

    inline bool useParPar() const { return _par2Path.toLower().contains("parpar"); }
    inline bool useMultiPar() const { return _par2Path.toLower().contains("par2j"); }

    QList<NNTP::ServerParams *> const &nntpServers() const { return _nntpServers; }
    QString                            groups() const { return _grpList.join(","); }

    QStringList getPostingGroups() const
    {
        if (_groupPolicy == NgConf::GROUP_POLICY::EACH_POST && _nbGroups > 1)
            return QStringList(_grpList.at(std::rand() % _nbGroups));
        else
            return _grpList;
    }
    bool groupPolicyPerFile() const { return _groupPolicy == NgConf::GROUP_POLICY::EACH_FILE; }

    bool   removeRarRootFolder() const { return _rarNoRootFolder; }
    bool   tryResumePostWhenConnectionLost() const { return _tryResumePostWhenConnectionLost; }
    ushort waitDurationBeforeAutoResume() const { return _waitDurationBeforeAutoResume; }

    QStringList const &packAutoKeywords() const { return _packAutoKeywords; }

    bool autoCloseTabs() const { return _autoCloseTabs; }
    bool preparePacking() const { return _preparePacking; }

    std::string getFrom() const;

    bool hasGroupPolicyEachFile() const { return _groupPolicy == NgConf::GROUP_POLICY::EACH_FILE; }
    bool use7z() const { return _use7z; }

    bool               genFrom() const { return _genFrom; }
    bool               saveFrom() const { return _saveFrom; }
    std::string const &from() const { return _from; }

    bool removeAccentsOnNzbFileName() const { return _removeAccentsOnNzbFileName; }

    void dumpParams() const;
    void setPackingDoneParamsForResume();

private:
    inline bool removeNntpServer(NNTP::ServerParams *server)
    {
        // deletion of the server will be done by the caller
        return _nntpServers.removeOne(server);
    }

    //! not everyone can save the config! only NgPost itself
    bool saveConfig(QString const &configFilePath, NgPost const &ngPost) const;

    // handy update methods for NgConfigLoader
    void updateGroups(QString const &groups);
    void enableAutoPacking(bool enable = true);
    void setPar2Path(QString const &par2Path_) { _par2Path = par2Path_; }

    void setEmptyCompressionArguments();

    //    QList<NNTP::ServerParams *> &nntpServers() { return _nntpServers; }

#ifdef __USE_TMP_RAM__
    QString setRamPathAndTestStorage(QString const &ramPath);
#endif
};

class PostingParams
{
    Q_DECLARE_TR_FUNCTIONS(PostingParams); // tr() without QObject using QCoreApplication::translate

private:
    NgPost      &_ngPost;
    SharedParams _params;

    QString             _rarName;     //!< not const for resumeJobs
    QString             _rarPass;     //!< not const for resumeJobs
    QString             _nzbFilePath; //!< can be changed for no overwritting existing nzb
    const QFileInfoList _files;

    PostingWidget *const _postWidget;
    QStringList const    _grpList; //!< Newsgroup where we're posting in a list format to write in the nzb

    // MB_TODO: we probably don't need this PostingParams::_from
    // The one _params->from() should be enough!
    const std::string _from; //!< email of poster (if empty, random one will be used for each file)

    //! list of meta to add in the nzb header (typically a category, password...)
    QMap<QString, QString> _meta;

    mutable bool _splitArchive; //!< might be set by buildCompressionCommandArgumentsList

public:
    PostingParams(NgPost                       &ngPost,
                  QString const                &rarName,
                  QString const                &rarPass,
                  QString const                &nzbFilePath,
                  QFileInfoList const          &files,
                  PostingWidget                *postWidget,
                  QList<QString> const         &grpList,
                  std::string const            &from,
                  SharedParams const           &mainParams,
                  QMap<QString, QString> const &meta = QMap<QString, QString>());

    PostingParams(PostingParams const &)            = default;
    PostingParams &operator=(PostingParams const &) = delete;
    PostingParams(PostingParams &&)                 = default;
    PostingParams &operator=(PostingParams &&)      = delete;

    void setPackingDoneParamsForResume(); // detach!

    bool quietMode() const;
    bool dispProgressBar() const;
    bool dispFilesPosting() const;

    QList<NNTP::ServerParams *> const &nntpServers() const;

    QMap<QString, QString> const &meta();

    void setMeta(QMap<QString, QString> const &meta);
    void removeMeta(QString const &metaKey);

    int nbNntpConnections() const;

    void setNzbFilePath(QString const &updatedPath);

    QString const &rarName() const;
    QString const &rarPass() const;
    QString const &nzbFilePath() const;

    QFileInfoList const &files() const;

    int nbThreads() const;

    QStringList groupsAccordingToPolicy() const;
    QString     groups() const;
    QString     from(bool emptyIfObfuscateArticle) const;

    std::string const *fromStdPtr() const;

    QString const     &tmpPath() const;
    QString const     &rarPath() const;
    QStringList const &rarArgs() const;
    ushort             rarSize() const;
    bool               useRarMax() const;
    ushort             rarMax() const;
    ushort             par2Pct() const;

    ushort         lengthName() const;
    ushort         lengthPass() const;
    QString const &rarPassFixed() const;

    QString const &par2Path() const;
    QString const &par2Args() const;
    QString const &par2PathConfig() const;

    bool doCompress() const;
    bool doPar2() const;
    bool genName() const;
    bool genPass() const;
    bool keepRar() const;
    bool packAuto() const;

    QStringList const &packAutoKeywords() const;

    QUrl const *urlNzbUpload() const;

    bool use7z() const;
    bool hasCompressed() const;
    bool hasPacking() const;

    bool saveOriginalFiles() const;

    bool obfuscateArticles() const;
    bool obfuscateFileName() const;
    bool delFilesAfterPost() const;
    bool overwriteNzb() const;

    QStringList buildCompressionCommandArgumentsList() const;

    bool canCompress() const;
    bool canGenPar2() const;

    bool splitArchive() const;

    bool useParPar() const;
    bool useMultiPar() const;

#ifdef __USE_TMP_RAM__
    qint64 ramAvailable() const;
    bool   useTmpRam() const;
    double ramRatio() const;

    QString const &ramPath() const;
#endif

    std::string fromStd() const;
    bool        removeRarRootFolder() const;
    bool        tryResumePostWhenConnectionLost() const;
    ushort      waitDurationBeforeAutoResume() const;

    void dumpParams() const;

    QString getFilesPaths() const;

    void genRarName();
    void genRarPass();

private:
    bool _checkTmpFolder() const;
};

#endif // POSTINGPARAMS_H
