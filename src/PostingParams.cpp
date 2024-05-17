#include "PostingParams.h"
#include <QCoreApplication>
#include <QDir>
#include <QThread>
#ifdef __USE_TMP_RAM__
#  include <QStorageInfo>
#endif

#include "NgPost.h" // for treadsafe logging
#include "nntp/NntpArticle.h"
#include "nntp/NntpServerParams.h"
#include "utils/NgTools.h"

using namespace NgConf;

MainParams::~MainParams()
{
    qDebug() << "[MB_TRACE][MainParams] destroyed... is it on purpose?";
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
#ifdef __USE_TMP_RAM__
    , _storage(nullptr)
    , _ramPath()
    , _ramRatio(kRamRatioMin)
#endif
    , _nbThreads(QThread::idealThreadCount())
    , _socketTimeOut(kDefaultSocketTimeOut)
    , _nzbPath(kDefaultNzbPath)
    , _nzbPathConf(kDefaultNzbPath)
    , _tmpPath()
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
    , _meta()
    , _grpList(kDefaultGroups)
    , _nbGroups(kDefaultGroups.size())
    , _inputDir()
    , _delAuto(false)

{
}

#ifdef __USE_TMP_RAM__
qint64 MainParams::ramAvailable() const { return _storage->bytesAvailable(); }

std::string MainParams::getFrom() const
{
    if (_genFrom || _obfuscateArticles || _from.empty())
        return NgTools::randomStdFrom(_lengthName);
    return _from;
}

QString MainParams::setRamPathAndTestStorage(NgPost *const ngPost, QString const &ramPath)
{
    QFileInfo fi(ramPath);
    if (!fi.isDir())
        return tr("should be a directory!...");
    else if (!fi.isWritable())
        return tr("should be writable!...");

    _ramPath = ramPath;
    _storage = new QStorageInfo(_ramPath);
    ngPost->onLog(tr("Using RAM Storage %1, root: %2, type: %3, size: %4, available: %5")
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

QString PostingParams::from(bool emptyIfObfuscateArticle) const
{
    if (emptyIfObfuscateArticle && _params->obfuscateArticles())
        return QString();

    return QString::fromStdString(_from);
}

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

            if (_ngPost->debugMode())
                emit _ngPost->log(
                        QCoreApplication::translate("PostingParams", "postSize for %1 : %2 MB => volSize: %3")
                                .arg(_nzbFilePath, postSize)
                                .arg(volSize),
                        true);
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
        emit _ngPost->error(QCoreApplication::translate(
                "PostingParams", "NO_POSSIBLE_COMPRESSION: You must define the temporary directory..."));
        return false;
    }

    QFileInfo fi(_params->tmpPath());
    if (!fi.exists() || !fi.isDir() || !fi.isWritable())
    {
        emit _ngPost->error(QCoreApplication::translate(
                "PostingParams", "ERROR: the temporary directory must be a WRITABLE directory..."));
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
        emit _ngPost->error(
                QCoreApplication::translate("PostingParams", "ERROR: the RAR path is not executable..."));
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
        emit _ngPost->error(QCoreApplication::translate("PostingParams", "ERROR: par2 is not available..."));
        return false;
    }

    return true;
}

std::string PostingParams::fromStd() const
{
    if (_params->genFrom() || _params->from().empty())
        return NgTools::randomStdFrom();
    else
        return _params->from();
}

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
           << (ngPost.dispProgressBar() ? "" : "#") << "DISP_Progress = BAR\n"
           << (ngPost.dispFilesPosting() ? "" : "#") << "DISP_Progress = FILES\n"
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
           << tr("## For GUI ONLY, save the logs in a file (to debug potential crashes)") << "\n"
           << tr("## ~/ngPost.log on Linux and MacOS, in the executable folder for Windows") << "\n"
           << tr("## The log is overwritten each time ngPost is launched") << "\n"
           << tr("## => after a crash, please SAVE the log before relaunching ngPost") << "\n"
           << (ngPost.loggingInFile() ? "" : "#") << "LOG_IN_FILE = true"
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

#ifdef __DEBUG__
void MainParams::dumpParams() const
{
    QString servers;
    for (NntpServerParams *srv : _nntpServers)
        servers += srv->str() + " ";
    qDebug() << "[MainParams::_dumpParams]>>>>>>>>>>>>>>>>>>\n"
             << "nb Servers: " << _nntpServers.size() << ": " << servers << "\n\nnbThreads: " << _nbThreads
             << "\ninputDir: " << _inputDir << ", autoDelete: " << _delAuto << "\npackAuto: " << _packAuto
             << ", packAutoKeywords:" << _packAutoKeywords << ", autoClose: " << _autoCloseTabs
             << "\n, monitor delay: " << _monitorSecDelayScan << " ignore dir: " << _monitorIgnoreDir
             << " ext: " << _monitorExtensions << "\nfrom: " << _from.c_str() << ", genFrom: " << _genFrom
             << ", saveFrom: " << _saveFrom << ", groups: " << _grpList.join(",")
             << " policy: " << kGroupPolicies[_groupPolicy].toUpper() << "\narticleSize: " << kArticleSize
             << ", obfuscate articles: " << _obfuscateArticles
             << ", obfuscate file names: " << _obfuscateFileName
             << ", _delFilesAfterPost: " << _delFilesAfterPost << ", _overwriteNzb: " << _overwriteNzb

             << "\n\ncompression settings: <tmp_path: " << _tmpPath << ">"
#  ifdef __USE_TMP_RAM__
             << " <ram_path: " << _ramPath << " ratio: " << _ramRatio << ">"
#  endif
             << ", <rar_path: " << _rarPath << ">"
             << ", <rar_size: " << _rarSize << ">"
             << "\n<par2_pct: " << _par2Pct << ">"
             << ", <par2_path: " << _par2Path << ">"
             << ", <par2_pathCfg: " << _par2PathConfig << ">"
             << ", <par2_args: " << _par2Args << ">"
             << " _use7z: " << _use7z << "\n\ncompress: " << _doCompress << ", doPar2: " << _doPar2
             << ", gen_name: " << _genName << ", genPass: " << _genPass << ", lengthName: " << _lengthName
             << ", lengthPass: " << _lengthPass << "\n _urlNzbUploadStr:" << _urlNzbUploadStr
             << " _urlNzbUpload :" << (_urlNzbUpload ? "yes" : "no") << ", _nzbPostCmd: " << _nzbPostCmd
             << "\n_preparePacking: " << _preparePacking << "\n[MainParams::_dumpParams]<<<<<<<<<<<<<<<<<<\n";
}
#endif
