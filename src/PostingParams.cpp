#include "PostingParams.h"

#include <QCoreApplication>
#include <QDir>
#include <QThread>
#include <QUrl>
#ifdef __USE_TMP_RAM__
#  include <QStorageInfo>
#endif

#include "NgPost.h" // for MainParams::saveConfig
#include "nntp/NntpArticle.h"
#include "nntp/NntpServerParams.h"
#include "utils/NgTools.h"

using namespace NgConf;

MainParams::~MainParams()
{
#ifdef __DEBUG__
    qDebug() << "[MB_TRACE][MainParams] destroyed... is it on purpose? (QSharedData)";
#endif
#ifdef __USE_TMP_RAM__
    if (_storage)
        delete _storage;
#endif
    if (_urlNzbUpload)
        delete _urlNzbUpload;

    qDeleteAll(_nntpServers);
}

MainParams::MainParams()
    : QSharedData()
    , _quiet(false)
    , _dispProgressBar(false)
    , _dispFilesPosting(false)

#ifdef __USE_TMP_RAM__
    , _storage(nullptr)
    , _ramPath()
    , _ramRatio(kRamRatioMin)
#endif
    , _nbThreads(QThread::idealThreadCount())
    , _socketTimeOut(kDefaultSocketTimeOut)
    , _nzbPath(kDefaultNzbPath)
    , _nzbPathConf(kDefaultNzbPath)
    , _tmpPath(kDefaultNzbPath)
    , _rarPath()
    , _rarArgs()
    , _rarSize(0)
    , _useRarMax(false)
    , _rarMax(kDefaultRarMax)
    , _par2Pct(0)
    , _par2Path()
    , _par2Args()
    , _par2PathConfig()
    , _lengthName(kDefaultLengthName)
    , _lengthPass(kDefaultLengthPass)
    , _rarPassFixed()
    , _doCompress(false)
    , _doPar2(false)
    , _genName(false)
    , _genPass(false)
    , _keepRar(false)
    , _packAuto(false)
    , _packAutoKeywords()
    , _obfuscateArticles(false)
    , _obfuscateFileName(false)
    , _delFilesAfterPost(false)
    , _overwriteNzb(false) // MB_TODO not sure why we would overwrite
    , _use7z(false)
    , _urlNzbUpload(nullptr)
    , _tryResumePostWhenConnectionLost(true)
    , _waitDurationBeforeAutoResume(kDefaultResumeWaitInSec)
    , _nzbPostCmd()
    , _preparePacking(false)
    , _monitorNzbFolders(false)
    , _monitorExtensions()
    , _monitorIgnoreDir(false)
    , _monitorSecDelayScan(1)
    , _removeAccentsOnNzbFileName(false)
    , _autoCloseTabs(false)
    , _rarNoRootFolder(false)
    , _groupPolicy(GROUP_POLICY::ALL)
    , _genFrom(false)
    , _saveFrom(false)
    , _from()
    , _grpList(kDefaultGroups)
    , _nbGroups(kDefaultGroups.size())
    , _inputDir()
    , _delAuto(false)

{
}

void MainParams::setDisplayProgress(QString const &txtValue)
{
    QString val = txtValue.toLower();
    if (val == kDisplayProgress[DISP_PROGRESS::BAR])
    {
        _dispProgressBar  = true;
        _dispFilesPosting = false;
        qDebug() << "Display progressbar bar\n";
    }
    else if (val == kDisplayProgress[DISP_PROGRESS::FILES])
    {
        _dispProgressBar  = false;
        _dispFilesPosting = true;
        qDebug() << "Display Files when start posting\n";
    }
    else if (val == kDisplayProgress[DISP_PROGRESS::NONE])
    { // force it in case in the config file something was on
        _dispProgressBar  = false;
        _dispFilesPosting = false;
    }
    else
        NgLogger::error(tr("Wrong display keyword: %1").arg(val.toUpper()));
}

#ifdef __USE_TMP_RAM__
qint64 MainParams::ramAvailable() const { return _storage->bytesAvailable(); }

std::string MainParams::getFrom() const
{
    if (_genFrom || _obfuscateArticles || _from.empty())
        return NgTools::randomStdFrom(_lengthName);
    return _from;
}

QString MainParams::setRamPathAndTestStorage(QString const &ramPath)
{
    QFileInfo fi(ramPath);
    if (!fi.isDir())
        return tr("should be a directory!...");
    else if (!fi.isWritable())
        return tr("should be writable!...");

    _ramPath = ramPath;
    _storage = new QStorageInfo(_ramPath);
    NgLogger::log(tr("Using RAM Storage %1, root: %2, type: %3, size: %4, available: %5")
                          .arg(_ramPath)
                          .arg(_storage->rootPath())
                          .arg(QString(_storage->fileSystemType()))
                          .arg(NgTools::humanSize(static_cast<double>(_storage->bytesTotal())))
                          .arg(NgTools::humanSize(static_cast<double>(_storage->bytesAvailable()))),
                  true);

    return QString();
}

#endif

void MainParams::updateGroups(QString const &groups)
{
    _grpList.clear();
    for (QString const &grp : groups.split(","))
        _grpList << grp;
    _nbGroups = _grpList.size();
}

PostingParams::PostingParams(NgPost                       &ngPost,
                             QString const                &rarName,
                             QString const                &rarPass,
                             QString const                &nzbFilePath,
                             QFileInfoList const          &files,
                             PostingWidget                *postWidget,
                             QList<QString> const         &grpList,
                             std::string const            &from,
                             SharedParams const           &mainParams,
                             QMap<QString, QString> const &meta)
    : _ngPost(ngPost)
    , _params(mainParams)
    , _rarName(rarName)
    , _rarPass(rarPass)
    , _nzbFilePath(nzbFilePath)
    , _files(files)
    , _postWidget(postWidget)
    , _grpList(grpList)
    , _from(from)
    , _meta(meta)
    , _splitArchive(false)
{
}

bool PostingParams::quietMode() const { return _params->quietMode(); }

bool PostingParams::dispProgressBar() const { return _params->dispProgressBar(); }

bool PostingParams::dispFilesPosting() const { return _params->dispFilesPosting(); }

QList<NntpServerParams *> const &PostingParams::nntpServers() const { return _params->nntpServers(); }

QMap<QString, QString> const &PostingParams::meta() { return _meta; }

void PostingParams::setMeta(QMap<QString, QString> const &meta)
{
    _meta = meta;
    ;
}

void PostingParams::removeMeta(QString const &metaKey) { _meta.remove(metaKey); }

int PostingParams::nbNntpConnections() const
{
    int nbConnections = 0;
    for (NntpServerParams *srvParams : _params->nntpServers())
    {
        if (srvParams->enabled)
            nbConnections += srvParams->nbCons;
    }
    return nbConnections;
}

void PostingParams::setNzbFilePath(QString const &updatedPath) { _nzbFilePath = updatedPath; }

QString const &PostingParams::rarName() const { return _rarName; }

QString const &PostingParams::rarPass() const { return _rarPass; }

QString const &PostingParams::nzbFilePath() const { return _nzbFilePath; }

QFileInfoList const &PostingParams::files() const { return _files; }

int PostingParams::nbThreads() const { return _params->nbThreads(); }

QStringList PostingParams::groupsAccordingToPolicy() const
{
    static int nbGroups = _grpList.size();
    if (_params->obfuscateArticles() && _params->hasGroupPolicyEachFile() && nbGroups > 1)
        return QStringList(_grpList.at(std::rand() % nbGroups));
    return _grpList;
}

QString PostingParams::groups() const { return _grpList.join(","); }

QString PostingParams::from(bool emptyIfObfuscateArticle) const
{
    if (emptyIfObfuscateArticle && _params->obfuscateArticles())
        return QString();

    return QString::fromStdString(_from);
}

std::string const *PostingParams::fromStdPtr() const { return &_from; }

QString const &PostingParams::tmpPath() const { return _params->tmpPath(); }

QString const &PostingParams::rarPath() const { return _params->rarPath(); }

QStringList const &PostingParams::rarArgs() const { return _params->rarArgs(); }

ushort PostingParams::rarSize() const { return _params->rarSize(); }

bool PostingParams::useRarMax() const { return _params->useRarMax(); }

ushort PostingParams::rarMax() const { return _params->rarMax(); }

ushort PostingParams::par2Pct() const { return _params->par2Pct(); }

ushort PostingParams::lengthName() const { return _params->lengthName(); }

ushort PostingParams::lengthPass() const { return _params->lengthPass(); }

QString const &PostingParams::rarPassFixed() const { return _params->rarPassFixed(); }

QString const &PostingParams::par2Path() const { return _params->par2Path(); }

QString const &PostingParams::par2Args() const { return _params->par2Args(); }

QString const &PostingParams::par2PathConfig() const { return _params->par2PathConfig(); }

bool PostingParams::doCompress() const { return _params->doCompress(); }

bool PostingParams::doPar2() const { return _params->doPar2(); }

bool PostingParams::genName() const { return _params->genName(); }

bool PostingParams::genPass() const { return _params->genPass(); }

bool PostingParams::keepRar() const { return _params->keepRar(); }

bool PostingParams::packAuto() const { return _params->packAuto(); }

QStringList const &PostingParams::packAutoKeywords() const { return _params->packAutoKeywords(); }

QUrl const *PostingParams::urlNzbUpload() const { return _params->urlNzbUpload(); }

bool PostingParams::use7z() const { return _params->use7z(); }

bool PostingParams::hasCompressed() const { return _params->doCompress(); }

bool PostingParams::hasPacking() const { return _params->doCompress() || _params->doPar2(); }

bool PostingParams::saveOriginalFiles() const
{
    return !_postWidget || _params->delFilesAfterPost() || _params->obfuscateFileName();
}

bool PostingParams::obfuscateArticles() const { return _params->obfuscateArticles(); }

bool PostingParams::obfuscateFileName() const { return _params->obfuscateFileName(); }

bool PostingParams::delFilesAfterPost() const { return _params->delFilesAfterPost(); }

bool PostingParams::overwriteNzb() const { return _params->overwriteNzb(); }

QStringList PostingParams::buildCompressionCommandArgumentsList() const
{
    // create rar args (rar a -v50m -ed -ep1 -m0 -hp"$PASS" "$TMP_FOLDER/$RAR_NAME.rar" "${FILES[@]}")
    // QStringList args = {"a", "-idp", "-ep1", compressLevel,
    // QString("%1/%2.rar").arg(archiveTmpFolder).arg(archiveName)};
    QStringList args = _params->rarArgs();

    // make sure 'a' is present: Add files to archive.
    if (!args.contains("a"))
        args.prepend("a");

    // compatibility with previous ngPost version but not sure this is needed:
    // man rar: -id[c,d,n,p,q] : Display or disable messages.
    if (!use7z() && !args.contains("-idp"))
        args << "-idp";

    // add the password
    if (!_rarPass.isEmpty())
    {
        if (use7z())
        {
            if (!args.contains("-mhe=on"))
                args << "-mhe=on"; // header encryption
            args << QString("-p%1").arg(_rarPass);
        }
        else //-hp[password]: Encrypt both file data and headers.
            args << QString("-hp%1").arg(_rarPass);
    }

    if (_params->rarSize() > 0 || _params->useRarMax())
    {
        uint volSize = _params->rarSize();
        if (_params->useRarMax())
        {
            qint64 postSize = 0;
            for (QFileInfo const &fileInfo : _files)
                postSize += NgTools::recursivePathSize(fileInfo);

            postSize /= 1024 * 1024; // to get it in MB
            if (volSize > 0)
            {
                if (postSize / volSize > _params->rarMax())
                    volSize = static_cast<uint>(postSize / _params->rarMax()) + 1;
            }
            else
                volSize = static_cast<uint>(postSize / _params->rarMax()) + 1;
            NgLogger::log(
                    tr("postSize for %1 : %2 MB => volSize: %3").arg(_nzbFilePath).arg(postSize).arg(volSize),
                    true,
                    NgLogger::DebugLevel::Debug);
        }
        args << QString("-v%1m").arg(volSize);
        _splitArchive = true; // used for par2cmd
    }
    return args;
}

bool PostingParams::_checkTmpFolder() const
{
    if (_params->tmpPath().isEmpty())
    {
        NgLogger::error(tr("NO_POSSIBLE_COMPRESSION: You must define the temporary directory..."));
        return false;
    }

    QFileInfo fi(_params->tmpPath());
    if (!fi.exists() || !fi.isDir() || !fi.isWritable())
    {
        NgLogger::error(tr("ERROR: the temporary directory must be a WRITABLE directory..."));
        return false;
    }

    return true;
}

bool PostingParams::canCompress() const
{
    // 1.: the _tmp_folder must be writable
    if (!_checkTmpFolder())
        return false;

    // 2.: check _rarPath is executable
    QFileInfo fi(_params->rarPath());
    if (!fi.exists() || !fi.isFile() || !fi.isExecutable())
    {
        NgLogger::error(tr("ERROR: the RAR path is not executable..."));
        return false;
    }

    return true;
}

bool PostingParams::canGenPar2() const
{
    // 1.: the _tmp_folder must be writable
    if (!_checkTmpFolder())
        return false;

    // 2.: check _ is executable
    QFileInfo fi(_params->par2Path());
    if (!fi.exists() || !fi.isFile() || !fi.isExecutable())
    {
        NgLogger::error(tr("ERROR: par2 is not available..."));
        return false;
    }

    return true;
}

bool PostingParams::splitArchive() const { return _splitArchive; }

bool PostingParams::useParPar() const { return _params->useParPar(); }

bool PostingParams::useMultiPar() const { return _params->useMultiPar(); }

qint64 PostingParams::ramAvailable() const { return _params->ramAvailable(); }

bool PostingParams::useTmpRam() const { return _params->useTmpRam(); }

double PostingParams::ramRatio() const { return _params->ramRatio(); }

QString const &PostingParams::ramPath() const { return _params->ramPath(); }

std::string PostingParams::fromStd() const
{
    if (_params->genFrom() || _params->from().empty())
        return NgTools::randomStdFrom();
    else
        return _params->from();
}

bool PostingParams::removeRarRootFolder() const { return _params->removeRarRootFolder(); }

bool PostingParams::tryResumePostWhenConnectionLost() const
{
    return _params->tryResumePostWhenConnectionLost();
}

ushort PostingParams::waitDurationBeforeAutoResume() const { return _params->waitDurationBeforeAutoResume(); }

void MainParams::enableAutoPacking(bool enable)
{
    _doCompress = false;
    _genName    = false;
    _genPass    = false;
    _doPar2     = false;
    _packAuto   = enable;
    if (enable)
    {
        for (auto const &keyWord : _packAutoKeywords)
        {
            QString keyLower = keyWord.toLower();
            if (keyLower == kOptionNames[Opt::COMPRESS])
                _doCompress = true;
            else if (keyLower == kOptionNames[Opt::GEN_NAME])
                _genName = true;
            else if (keyLower == kOptionNames[Opt::GEN_PASS])
                _genPass = true;
            else if (keyLower == kOptionNames[Opt::GEN_PAR2])
                _doPar2 = true;
        }
        // MB_TODO: some log
        // #ifdef __USE_HMI__
        //        if (!_hmi && !_quiet)
        // #else
        //        if (!_quiet)
        // #endif
        //            _log(tr("PACKing auto using: %1").arg(_packAutoKeywords.join(", ").toUpper()));
    }
}

void MainParams::setEmptyCompressionArguments()
{
    _use7z = _rarPath.contains(k7zip);
    if (_rarArgs.isEmpty())
    {
        if (_use7z)
            _rarArgs = QString(kDefault7zOptions).split(kCmdArgsSeparator);
        else
            _rarArgs = QString(kDefaultRarExtraOptions).split(kCmdArgsSeparator);
    }
}

bool MainParams::saveConfig(QString const &configFilePath, NgPost const &ngPost) const
{
    QFile file(configFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream stream(&file);
    stream << tr("# ngPost configuration file") << "\n"
           << "#\n"
           << "#\n"
           << "\n"
           << "CONF_VERSION = " << kVersion << "\n"
           << "\n"
           << tr("## Lang for the app. Currently supported: EN, FR, ES, DE, NL, PT, ZH") << "\n"
           << "lang = " << ngPost.lang().toUpper() << "\n"
           << "\n"
           << tr("## use Proxy (only Socks5 type!)") << "\n"
           << (ngPost.proxyUrl().isEmpty() ? "#PROXY_SOCKS5 = user:pass@192.168.1.1:5555" : ngPost.proxyUrl())
           << "\n"
           << "\n"
           << tr("## destination folder for all your nzb") << "\n"
           << tr("## if you don't put anything, the nzb will be generated in the folder of ngPost on Windows "
                 "and in "
                 "/tmp on Linux")
           << "\n"
           << tr("## this will be overwritten if you use the option -o with the full path of the nzb") << "\n"
           << "nzbPath  = " << (_nzbPath.isEmpty() ? _nzbPathConf : _nzbPath) << "\n"
           << "\n"
           << tr("## Shutdown command to switch off the computer when ngPost is done with all its queued "
                 "posting")
           << "\n"
           << tr("## this should mainly used with the auto posting") << "\n"
           << tr("## you could use whatever script instead (like to send a mail...)") << "\n"
           << tr("#SHUTDOWN_CMD = shutdown /s /f /t 0  (Windows)") << "\n"
           << tr("#SHUTDOWN_CMD = sudo -n /sbin/poweroff  (Linux, make sure poweroff has sudo rights without "
                 "any "
                 "password or change the command)")
           << "\n"
           << tr("#SHUTDOWN_CMD = sudo -n shutdown -h now (MacOS, same make sure you've sudo rights)") << "\n"
           << "SHUTDOWN_CMD = " << ngPost.shutdownCmd() << "\n"
           << "\n"
           << tr("## upload the nzb to a specific URL") << "\n"
           << tr("## only http, https or ftp (neither ftps or sftp are supported)") << "\n"
           << tr("#NZB_UPLOAD_URL = ftp://user:pass@url_or_ip:21") << "\n"
           << (_urlNzbUploadStr.isEmpty() ? QString()
                                          : QString("NZB_UPLOAD_URL = %1\n").arg(_urlNzbUpload->url()))
           << "\n"
           << tr("## execute a command or script at the end of each post (see examples)") << "\n"
           << tr("## you can use several post commands by defining several NZB_POST_CMD") << "\n"
           << tr("## here is the list of the available placehoders") << "\n"
           << "##   " << kPostCmdPlaceHolders[PostCmdPlaceHolders::nzbPath] << "          : "
           << tr("full path of the written nzb file") << "\n"
           << "##   " << kPostCmdPlaceHolders[PostCmdPlaceHolders::nzbName] << "          : "
           << tr("name of the nzb without the extension (original source name)") << "\n"
           << "##   " << kPostCmdPlaceHolders[PostCmdPlaceHolders::rarName] << "          : "
           << tr("name of the archive files (in case of obfuscation)") << "\n"
           << "##   " << kPostCmdPlaceHolders[PostCmdPlaceHolders::rarPass] << "          : "
           << tr("archive password") << "\n"
           << "##   " << kPostCmdPlaceHolders[PostCmdPlaceHolders::sizeInByte] << "       : "
           << tr("size of the post (before yEnc encoding)") << "\n"
           << "##   " << kPostCmdPlaceHolders[PostCmdPlaceHolders::groups] << "           : "
           << tr("list of groups (comma separated)") << "\n"
           << "##   " << kPostCmdPlaceHolders[PostCmdPlaceHolders::nbFiles] << "          : "
           << tr("number of files in the post") << "\n"
           << "##   " << kPostCmdPlaceHolders[PostCmdPlaceHolders::nbArticles] << "       : "
           << tr("number of Articles") << "\n"
           << "##   " << kPostCmdPlaceHolders[PostCmdPlaceHolders::nbArticlesFailed] << " : "
           << tr("number of Articles that failed to be posted") << "\n"
           << "#\n"
           << "#NZB_POST_CMD = scp \"__nzbPath__\" myBox.com:~/nzbs/\n"
           << "#NZB_POST_CMD = zip \"__nzbPath__.zip\" \"__nzbPath__\"\n"
           << "#NZB_POST_CMD = ~/scripts/postNZB.sh \"__nzbPath__\" \"__groups__\" __rarName__ __rarPass__ "
              "__sizeInByte__ __nbFiles__ __nbArticles__ __nbArticlesFailed__\n"
           << "#NZB_POST_CMD = mysql -h localhost -D myDB -u myUser -pmyPass-e \"INSERT INTO POST (release, "
              "rarName, rarPass, size) VALUES('__nzbName__', '__rarName__', '__rarPass__', '__sizeInByte__')\"\n"
           << "#NZB_POST_CMD = cmd.exe /C move \"__nzbPath__\" "
              "\"C:\\ngPost\\nzb\\__nzbName__{{__rarPass__}}.nzb\"\n"
           << "#NZB_POST_CMD = curl -X POST -F \"file=@__nzbPath__\" -F \"api=12345\" -F \"cat=45\" -F "
              "\"private=no\" https://usenet.com/post-api\n"
           << "";
    for (QString const &nzbPostCmd : _nzbPostCmd)
        stream << "NZB_POST_CMD = " << nzbPostCmd << "\n";
    stream << "\n"
           << tr("## nzb files are normally all created in nzbPath") << "\n"
           << tr("## but using this option, the nzb of each monitoring folder will be stored in their own "
                 "folder "
                 "(created in nzbPath)")
           << "\n"
           << (_monitorNzbFolders ? "" : "#") << "MONITOR_NZB_FOLDERS = true\n"
           << "\n"
           << tr("## for monitoring, extension file filter for new incoming files (coma separated, no dot)")
           << "\n"
           << (_monitorExtensions.isEmpty() ? "#" : "") << "MONITOR_EXTENSIONS = "
           << (_monitorExtensions.isEmpty() ? "mkv,mp4,avi,zip,tar,gz,iso" : _monitorExtensions.join(","))
           << "\n"
           << "\n"
           << tr("## for monitoring, ignore new incoming folders") << "\n"
           << (_monitorIgnoreDir ? "" : "#") << "MONITOR_IGNORE_DIR = true\n"
           << tr("## for monitoring, delay to check the size of an incoming file/folder to make sure it is "
                 "fully "
                 "arrived before posting it")
           << "\n"
           << tr("## must be between 1sec and 120sec (otherwise default: 1sec)") << "\n"
           << "MONITOR_SEC_DELAY_SCAN = " << _monitorSecDelayScan << "\n"
           << "\n\n"
           << tr("## Default folder to open to select files from the HMI") << "\n"
           << "inputDir = " << _inputDir << "\n"
           << "\n"
           //               << tr("## History posting file") << "\n"
           //               << tr("## each succesful post will append a line with the date, the file name, the
           //               archive name, the password...") << "\n"
           //               << (_postHistoryFile.isEmpty()  ? "#" : "") <<"POST_HISTORY = "
           //               << (_postHistoryFile.isEmpty()  ? "/nzb/ngPost_history.csv" : _postHistoryFile) <<
           //               "\n"
           //               << "\n"
           //               << tr("## Character used to separate fields in the history posting file") << "\n"
           //               << (_historyFieldSeparator == QString(kDefaultFieldSeparator) ? "#" : "") <<
           //               "FIELD_SEPARATOR = " << _historyFieldSeparator << "\n"
           //               << "\n"
           << "GROUPS   = " << _grpList.join(",") << "\n"
           << "\n"
           << tr("## If you give several Groups (comma separated) you've 3 policies for posting:") << "\n"
           << tr("##    ALL       : everything is posted on ALL the Groups") << "\n"
           << tr("##    EACH_POST : each Post will be posted on a random Group from the list") << "\n"
           << tr("##    EACH_FILE : each File will be posted on a random Group from the list") << "\n"
           << "GROUP_POLICY = " << kGroupPolicies[_groupPolicy].toUpper() << "\n"
           << "\n"
           << tr("## uncomment the next line if you want a fixed uploader email (in the nzb and in the header "
                 "of "
                 "each articles)")
           << "\n"
           << tr("## if you let it commented, we'll generate ONE random email for all the posts of the session")
           << "\n"
           << (_saveFrom ? "" : "#") << "FROM = " << _from.c_str() << "\n"
           << "\n"
           << tr("## Generate new random poster for each post (--auto or --monitor)") << "\n"
           << tr("## if this option is set the FROM email just above will be ignored") << "\n"
           << (_genFrom ? "" : "#") << "GEN_FROM = true"
           << "\n"
           << "\n"
           << "\n"
           << tr("## uncomment the next line to limit the number of threads,  (by default it'll use the number "
                 "of "
                 "cores)")
           << "\n"
           << tr("## all the connections are spread equally on those posting threads") << "\n"
           << "thread  =  " << _nbThreads << "\n"
           << "\n"
           << "\n"
           << tr("## How to display progressbar in command line: NONE, BAR, FILES") << "\n"
           << (dispProgressBar() ? "" : "#") << "DISP_Progress = BAR\n"
           << (dispFilesPosting() ? "" : "#") << "DISP_Progress = FILES\n"
           << "\n"
           << "\n"
           << tr("## suffix of the msg_id for all the articles (cf nzb file)") << "\n"
           << (kArticleIdSignature == "ngPost" ? "#msg_id  =  ngPost\n"
                                               : QString("msg_id  =  %1\n").arg(kArticleIdSignature.c_str()))
           << "\n"
           << tr("## article size (default 700k)") << "\n"
           << "article_size = " << kArticleSize << "\n"
           << "\n"
           << tr("## number of retry to post an Article in case of failure (probably due to an already existing "
                 "msg-id)")
           << "\n"
           << "retry = " << NntpArticle::nbMaxTrySending() << "\n"
           << "\n"
           << "\n"
           << tr("## uncomment the following line to obfuscate the subjects of each Article") << "\n"
           << tr("## /!\\ CAREFUL you won't find your post if you lose the nzb file /!\\") << "\n"
           << (_obfuscateArticles ? "" : "#") << "obfuscate = article\n"
           << "\n"
           << tr("## remove accents and special characters from the nzb file names") << "\n"
           << (_removeAccentsOnNzbFileName ? "" : "#") << "NZB_RM_ACCENTS = true\n"
           << "\n"
           << tr("## close Quick Post Tabs when posted successfully (for the GUI)") << "\n"
           << (_autoCloseTabs ? "" : "#") << "AUTO_CLOSE_TABS = true\n"
           << "\n"
           << "\n"
           << tr("## Time to wait (seconds) before trying to resume a Post automatically in case of loss of "
                 "Network "
                 "(min: %1)")
                      .arg(kDefaultResumeWaitInSec)
           << "\n"
           << "RESUME_WAIT = " << _waitDurationBeforeAutoResume << "\n"
           << "\n"
           << tr("## By default, ngPost tries to resume a Post if the network is down.") << "\n"
           << tr("## it won't stop trying until the network is back and the post is finished properly") << "\n"
           << tr("## you can disable this feature and thus stop a post when you loose the network") << "\n"
           << (_tryResumePostWhenConnectionLost ? "#" : "") << "NO_RESUME_AUTO = true\n"
           << "\n"
           << tr("## if there is no activity on a connection it will be closed and restarted") << "\n"
           << tr("## The duration is in second, default: %1, min: %2)")
                      .arg(kDefaultSocketTimeOut / 1000)
                      .arg(kMinSocketTimeOut / 1000)
           << "\n"
           << "SOCK_TIMEOUT = " << _socketTimeOut / 1000 << "\n"
           << "\n"
           << tr("## when several Posts are queued, prepare the packing of the next Post while uploading the "
                 "current one")
           << "\n"
           << (_preparePacking ? "" : "#") << "PREPARE_PACKING = true"
           << "\n"
           << "\n"
           << tr("## save the logs in a file (to debug potential crashes)") << "\n"
           << tr("## ~/ngPost.log on Linux and MacOS, in the executable folder for Windows") << "\n"
           << tr("## The log is overwritten each time ngPost is launched") << "\n"
           << tr("## => after a crash, please SAVE the log before relaunching ngPost and") << "\n"
           << tr("## plz post it on github as an issue %1").arg("https://github.com/mbruel/ngPost/issues")
           << "\n"
           << (NgLogger::loggingInFile() ? "" : "#") << "LOG_IN_FILE = true"
           << "\n"
           << "\n"
           << "\n"
           << "\n"
           << "\n"
           << "##############################################################\n"
           << "##           Compression and par2 section                   ##\n"
           << "##############################################################\n"
           << "\n"
           << tr("## Shortcut for automatic packing for both GUI and CMD using --pack") << "\n"
           << tr("## coma separated list using the keywords COMPRESS, GEN_NAME, GEN_PASS and GEN_PAR2") << "\n"
           << tr("## For Auto posting and Monitoring if you don't use COMPRESS you need GEN_PA2") << "\n"
           << tr("#PACK = COMPRESS, GEN_NAME, GEN_PASS, GEN_PAR2") << "\n"
           << tr("#PACK = GEN_PAR2") << "\n"
           << (_packAuto && _packAutoKeywords.size()
                       ? QString("PACK = %1\n").arg(_packAutoKeywords.join(", ").toUpper())
                       : "")
           << "\n"
           << tr("## use the same Password for all your Posts using compression") << "\n"
#ifdef __USE_HMI__
           << (ngPost.useHMI() ? (ngPost.hmiUseFixedPassword() ? "" : "#")
                               : (_rarPassFixed.isEmpty() ? "#" : ""))
#else
           << (_rarPassFixed.isEmpty() ? "#" : "")
#endif
           << "RAR_PASS = " << (_rarPassFixed.isEmpty() ? "yourPassword" : _rarPassFixed) << "\n"
           << "\n"
           << tr("## temporary folder where the compressed files and par2 will be stored") << "\n"
           << tr("## so we can post directly a compressed (obfuscated or not) archive of the selected files")
           << "\n"
           << tr("## /!\\ The directory MUST HAVE WRITE PERMISSION /!\\") << "\n"
           << tr("## this is set for Linux environment, Windows users MUST change it") << "\n"
           << "TMP_DIR = " << _tmpPath << "\n"
           << "\n";
#ifdef __USE_TMP_RAM__
    stream << tr("## temporary folder with size constraint, typically a tmpfs partition") << "\n"
           << tr("## the size of a post multiply by TMP_RAM_RATIO must available on the disk") << "\n"
           << tr("## otherwise ngPost will use TMP_DIR (with no check there)") << "\n"
           << tr("## (uncomment and define TMP_RAM to activate the feature, make sure the path is writable)")
           << "\n"
           << (_ramPath.isEmpty() ? "#" : "")
           << "TMP_RAM = " << (_ramPath.isEmpty() ? "/mnt/ngPost_tmpfs" : _ramPath) << "\n"
           << "\n"
           << tr("## Ratio used on the source files size to compensate the par2 generation") << "\n"
           << tr("## min is 10% to be sure (so 1.1), max 2.0") << "\n"
           << "TMP_RAM_RATIO = " << _ramRatio << "\n"
           << "\n";
#endif
    stream << tr("## RAR or 7zip absolute file path (external application)") << "\n"
           << tr("## /!\\ The file MUST EXIST and BE EXECUTABLE /!\\") << "\n"
           << tr("## this is set for Linux environment, Windows users MUST change it") << "\n"
           << "RAR_PATH = " << _rarPath << "\n"
           << "\n"
           << tr("## RAR EXTRA options (the first 'a' and '-idp' will be added automatically)") << "\n"
           << tr("## -hp will be added if you use a password with --gen_pass, --rar_pass or using the HMI")
           << "\n"
           << tr("## -v42m will be added with --rar_size or using the HMI") << "\n"
           << tr("## you could change the compression level, lock the archive, add redundancy...") << "\n"
           << "#RAR_EXTRA = -ep1 -m0 -k -rr5p\n"
           << "#RAR_EXTRA = -mx0 -mhe=on   (for 7-zip)\n"
           << (_rarArgs.isEmpty() ? "" : QString("RAR_EXTRA = %1\n").arg(_rarArgs.join(kCmdArgsSeparator)))
           << "\n"
           << tr("## size in MB of the RAR volumes (0 by default meaning NO split)") << "\n"
           << tr("## feel free to change the value or to comment the next line if you don't want to split the "
                 "archive")
           << "\n"
           << "RAR_SIZE = " << _rarSize << "\n"
           << "\n"
           << tr("## maximum number of archive volumes") << "\n"
           << tr("## we'll use RAR_SIZE except if it genereates too many volumes") << "\n"
           << tr("## in that case we'll update rar_size to be <size of post> / rar_max") << "\n"
           << "RAR_MAX = " << _rarMax << "\n"
           << "\n"
           << tr("##  keep rar folder after posting (otherwise it is automatically deleted uppon successful "
                 "post)")
           << "\n"
           << (_keepRar ? "" : "#") << "KEEP_RAR = true\n"
           << "\n"
           << "## " << tr("Remove root (parent) folder when compressing Folders using RAR") << "\n"
           << (_rarNoRootFolder ? "" : "#") << "RAR_NO_ROOT_FOLDER = true\n"
           << "\n"
           << tr("## par2 redundancy percentage (0 by default meaning NO par2 generation)") << "\n"
           << "PAR2_PCT = " << _par2Pct << "\n"
           << "\n"
           << tr("## par2 (or alternative) absolute file path") << "\n"
           << tr("## this is only useful if you compile from source (as par2 is included on Windows and the "
                 "AppImage)")
           << "\n"
           << tr("## or if you wish to use an alternative to par2 (for exemple Multipar on Windows)") << "\n"
           << tr("## (in that case, you may need to set also PAR2_ARGS)") << "\n";
    if (!_par2PathConfig.isEmpty())
        stream << "PAR2_PATH = " << _par2PathConfig << "\n";
#if defined(WIN32) || defined(__MINGW64__)
    stream << "#PAR2_PATH = <your_path>parpar.exe\n"
           << "#PAR2_PATH = <your_path>par2j64.exe\n";
#else
    stream << "#PAR2_PATH = /usr/bin/par2\n"
           << "#PAR2_PATH = <your_path>/parpar\n";
#endif
    stream << "\n"
           << tr("## fixed parameters for the par2 (or alternative) command") << "\n"
           << tr("## you could for exemple use Multipar on Windows") << "\n"
           << "#PAR2_ARGS = -s5M -r1n*0.6 -m2048M -p1l --progress stdout -q   (for parpar)\n"
           << "#PAR2_ARGS = c -l -m1024 -r8 -s768000                 (for par2cmdline)\n"
           << "#PAR2_ARGS = create /rr8 /lc40 /lr /rd2 /ss768000     (for Multipar)\n"
           << (_par2Args.isEmpty() ? "" : QString("PAR2_ARGS = %1\n").arg(_par2Args)) << "\n"
           << "\n"
           << tr("## length of the random generated archive's file name") << "\n"
           << "LENGTH_NAME = " << _lengthName << "\n"
           << "\n"
           << tr("## length of the random archive's passsword") << "\n"
           << "LENGTH_PASS = " << _lengthPass << "\n"
           << "\n"
           << "\n"
           << "\n"
           << "\n"
           << "##############################################################\n"
           << "##                   servers section                        ##\n"
           << "##############################################################\n"
           << "\n";

    for (NntpServerParams *param : _nntpServers)
    {
        stream << "[server]\n"
               << "host = " << param->host << "\n"
               << "port = " << param->port << "\n"
               << "ssl  = " << (param->useSSL ? "true" : "false") << "\n"
               << "user = " << param->user.c_str() << "\n"
               << "pass = " << param->pass.c_str() << "\n"
               << "connection = " << param->nbCons << "\n"
               << "enabled = " << (param->enabled ? "true" : "false") << "\n"
               << "nzbCheck = " << (param->nzbCheck ? "true" : "false") << "\n"
               << "\n\n";
    }
    stream << tr(
            "## You can add as many server if you have several providers by adding other \"server\" sections")
           << "\n"
           << "#[server]\n"
           << "#host = news.otherprovider.com\n"
           << "#port = 563\n"
           << "#ssl  = true\n"
           << "#user = myOtherUser\n"
           << "#pass = myOtherPass\n"
           << "#connection = 15\n"
           << "#enabled = false\n"
           << "#nzbCheck = false\n"
           << "\n";
    file.close();
    return true;
}

void MainParams::dumpParams() const
{
    QString servers;
    for (NntpServerParams *srv : nntpServers())
        servers += srv->str() + " ";
    qDebug() << "[MainParams::_dumpParams]>>>>>>>>>>>>>>>>>>\n"
             << "nb Servers: " << nntpServers().size() << ": " << servers << "\n\nnbThreads: " << nbThreads()
             << "\ninputDir: " << inputDir() << ", autoDelete: " << delAuto() << "\npackAuto: " << packAuto()
             << ", packAutoKeywords:" << packAutoKeywords() << ", autoClose: " << autoCloseTabs()
             << "\n, monitor delay: " << monitorSecDelayScan() << " ignore dir: " << monitorIgnoreDir()
             << " ext: " << monitorExtensions() << "\nfrom: " << from().c_str() << ", genFrom: " << genFrom()
             << ", saveFrom: " << saveFrom() << ", groups: " << groupList().join(",")
             << " policy: " << kGroupPolicies[groupPolicy()].toUpper() << "\narticleSize: " << kArticleSize
             << ", obfuscate articles: " << obfuscateArticles()
             << ", obfuscate file names: " << obfuscateFileName()
             << ", _delFilesAfterPost: " << delFilesAfterPost() << ", _overwriteNzb: " << overwriteNzb()

             << "\n\ncompression settings: <tmp_path: " << tmpPath() << ">"
#ifdef __USE_TMP_RAM__
             << " <ram_path: " << ramPath() << " ratio: " << ramRatio() << ">"
#endif
             << ", <rar_path: " << rarPath() << ">"
             << ", <rar_size: " << rarSize() << ">"
             << "\n<par2_pct: " << par2Pct() << ">"
             << ", <par2_path: " << par2Path() << ">"
             << ", <par2_pathCfg: " << par2PathConfig() << ">"
             << ", <par2_args: " << par2Args() << ">"
             << " _use7z: " << use7z() << "\n\ncompress: " << doCompress() << ", doPar2: " << doPar2()
             << ", gen_name: " << genName() << ", genPass: " << genPass() << ", lengthName: " << lengthName()
             << ", lengthPass: " << lengthPass() << "\n _urlNzbUploadStr:" << urlNzbUploadStr()
             << " _urlNzbUpload :" << (urlNzbUpload() ? "yes" : "no") << ", _nzbPostCmd: " << nzbPostCmd()
             << "\n_preparePacking: " << preparePacking();
    qDebug() << "\n[MainParams::_dumpParams]<<<<<<<<<<<<<<<<<<\n";
}

QString PostingParams::getFilesPaths() const
{
    QStringList filesPaths;
    for (auto const &fi : _files)
    {
        qDebug() << "[PostingJob::getFilesPaths]" << fi.absoluteFilePath();
        filesPaths << fi.absoluteFilePath();
    }
    return filesPaths.join(kInputFileSeparator);
}

void PostingParams::dumpParams() const
{
    qDebug() << "[PostingParams::_dumpParams]>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n";
    _params->dumpParams();
    qDebug() << "<rarName : " << rarName() << ">";
    qDebug() << "<rarPass : " << rarPass() << ">";
    qDebug() << "<nzbFilePath : " << nzbFilePath() << ">\n";
    qDebug() << "<files : " << getFilesPaths() << ">";
    qDebug() << "<_grpList : " << groups() << ">";
    qDebug() << "<_from : " << from(_params->obfuscateArticles()) << ">";
    qDebug() << "<rarName : " << _meta << ">";

    qDebug() << "[PostingParams::_dumpParams]<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<n";
}
