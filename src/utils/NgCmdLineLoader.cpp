#include "NgCmdLineLoader.h"

#include <QCommandLineParser>
#include <QDir>

#include "NgConf.h"
#include "NgError.h"
using namespace NgConf;

// #include "FoldersMonitorForNewFiles.h"
#include "NgPost.h"
#include "nntp/NntpArticle.h"
#include "nntp/NntpServerParams.h"
#include "PostingParams.h"
#include "utils/Macros.h"
#include "utils/NgConfigLoader.h"
#include "utils/NgTools.h"

QList<QCommandLineOption> const NgCmdLineLoader::kCmdOptions = {
    { kOptionNames[Opt::HELP], tr("Help: display syntax") },
    { { "v", kOptionNames[Opt::VERSION] }, tr("app version") },
    { { "c", kOptionNames[Opt::CONF] },
     tr("use configuration file (if not provided, we try to load $HOME/.ngPost)"),
     kOptionNames[Opt::CONF] },
    { kOptionNames[Opt::DISP_PROGRESS],
     tr("display cmd progressbar: NONE (default), BAR or FILES"),
     kOptionNames[Opt::DISP_PROGRESS] },
    { { "d", kOptionNames[Opt::DEBUG] }, tr("display extra information") },
    { kOptionNames[Opt::DEBUG_FULL], tr("display full debug information") },
    { { "l", kOptionNames[Opt::LANG] }, tr("application language"), kOptionNames[Opt::LANG] },

    { kOptionNames[Opt::CHECK],
     tr("check nzb file (if articles are available on Usenet) cf https://github.com/mbruel/nzbCheck"),
     kOptionNames[Opt::CHECK] },
    { { "q", kOptionNames[Opt::QUIET] }, tr("quiet mode (no output on stdout)") },

 // automated posting (scanning and/or monitoring)
    { kOptionNames[Opt::AUTO_DIR],
     tr("parse directory and post every file/folder separately. You must use --compress, "
         "should add --gen_par2, --gen_name and --gen_pass"),
     kOptionNames[Opt::AUTO_DIR] },
    { kOptionNames[Opt::MONITOR_DIR],
     tr("monitor directory and post every new file/folder. You must use --compress, should "
         "add --gen_par2, --gen_name and --gen_pass"),
     kOptionNames[Opt::MONITOR_DIR] },
    { kOptionNames[Opt::DEL_AUTO],
     tr("delete file/folder once posted. You must use --auto or --monitor with this option.") },

 // quick posting (several files/folders)
    { { "i", kOptionNames[Opt::INPUT] },
     tr("input file to upload (single file or directory), you can use it multiple times"),
     kOptionNames[Opt::INPUT] },
    { { "o", kOptionNames[Opt::OUTPUT] }, tr("output file path (nzb)"), kOptionNames[Opt::OUTPUT] },

 // general options
    { { "x", kOptionNames[Opt::OBFUSCATE] },
     tr("obfuscate the subjects of the articles (CAREFUL you won't find your post "
         "if you lose the nzb file)") },
    { { "g", kOptionNames[Opt::GROUPS] },
     tr("newsgroups where to post the files (coma separated without space)"),
     kOptionNames[Opt::GROUPS] },
    { { "m", kOptionNames[Opt::META] },
     tr("extra meta data in header (typically \"password=qwerty42\")"),
     kOptionNames[Opt::META] },
    { { "f", kOptionNames[Opt::FROM] },
     tr("poster email (random one if not provided)"),
     kOptionNames[Opt::FROM] },
    { { "a", kOptionNames[Opt::ARTICLE_SIZE] },
     tr("article size (default one: %1)").arg(kDefaultArticleSize),
     kOptionNames[Opt::ARTICLE_SIZE] },
    { { "z", kOptionNames[Opt::MSG_ID] },
     tr("msg id signature, after the @ (default one: ngPost)"),
     kOptionNames[Opt::MSG_ID] },
    { { "r", kOptionNames[Opt::NB_RETRY] },
     tr("number of time we retry to an Article that failed (default: 5)"),
     kOptionNames[Opt::NB_RETRY] },
    { { "t", kOptionNames[Opt::THREAD] },
     tr("number of Threads (the connections will be distributed amongs them)"),
     kOptionNames[Opt::THREAD] },
    { kOptionNames[Opt::GEN_FROM], tr("generate a new random email for each Post (--auto or --monitor)") },

 // for compression and par2 support
    { kOptionNames[Opt::TMP_DIR],
     tr("temporary folder where the compressed files and par2 will be stored"),
     kOptionNames[Opt::TMP_DIR] },
    { kOptionNames[Opt::RAR_PATH],
     tr("RAR absolute file path (external application)"),
     kOptionNames[Opt::RAR_PATH] },
    { kOptionNames[Opt::RAR_SIZE],
     tr("size in MB of the RAR volumes (0 by default meaning NO split)"),
     kOptionNames[Opt::RAR_SIZE] },
    { kOptionNames[Opt::RAR_MAX], tr("maximum number of archive volumes"), kOptionNames[Opt::RAR_MAX] },
    { kOptionNames[Opt::PAR2_PCT],
     tr("par2 redundancy percentage (0 by default meaning NO par2 generation)"),
     kOptionNames[Opt::PAR2_PCT] },
    { kOptionNames[Opt::PAR2_PATH],
     tr("par2 absolute file path (in case of self compilation of ngPost)"),
     kOptionNames[Opt::PAR2_PCT] },

    { kOptionNames[Opt::PACK],
     tr("Pack posts using config PACK definition with a subset of (COMPRESS, "
         "GEN_NAME, GEN_PASS, GEN_PAR2)") },
    { kOptionNames[Opt::COMPRESS], tr("compress inputs using RAR or 7z") },
    { kOptionNames[Opt::GEN_PAR2], tr("generate par2 (to be used with --compress)") },
    { kOptionNames[Opt::RAR_NAME],
     tr("provide the RAR file name (to be used with --compress)"),
     kOptionNames[Opt::RAR_NAME] },
    { kOptionNames[Opt::RAR_PASS],
     tr("provide the RAR password (to be used with --compress)"),
     kOptionNames[Opt::RAR_PASS] },
    { kOptionNames[Opt::GEN_NAME], tr("generate random RAR name (to be used with --compress)") },
    { kOptionNames[Opt::GEN_PASS], tr("generate random RAR password (to be used with --compress)") },
    { kOptionNames[Opt::LENGTH_NAME],
     tr("length of the random RAR name (to be used with --gen_name), default: 17"),
     kOptionNames[Opt::LENGTH_NAME] },
    { kOptionNames[Opt::LENGTH_PASS],
     tr("length of the random RAR password (to be used with --gen_pass), default: 13"),
     kOptionNames[Opt::LENGTH_PASS] },
    { kOptionNames[Opt::RAR_NO_ROOT_FOLDER],
     tr("Remove root (parent) folder when compressing Folders using RAR") },

    { { "S", kOptionNames[Opt::SERVER] },
     tr("NNTP server following the format (<user>:<pass>@@@)?<host>:<port>:<nbCons>:(no)?ssl"),
     kOptionNames[Opt::SERVER] },
 // without config file, you can provide all the parameters to connect to ONE SINGLE server
    { { "h", kOptionNames[Opt::HOST] }, tr("NNTP server hostname (or IP)"), kOptionNames[Opt::HOST] },
    { { "P", kOptionNames[Opt::PORT] }, tr("NNTP server port"), kOptionNames[Opt::PORT] },
    { { "s", kOptionNames[Opt::SSL] }, tr("use SSL") },
    { { "u", kOptionNames[Opt::USER] }, tr("NNTP server username"), kOptionNames[Opt::USER] },
    { { "p", kOptionNames[Opt::PASS] }, tr("NNTP server password"), kOptionNames[Opt::PASS] },
    { { "n", kOptionNames[Opt::CONNECTION] }, tr("number of NNTP connections"), kOptionNames[Opt::CONNECTION] },
};

bool NgCmdLineLoader::loadCmdLine(char *appName, NgPost &ngPost, SharedParams &postingParams)
{
    QString            appVersion = QString("%1_v%2").arg(kAppName, kVersion);
    QCommandLineParser parser;
    parser.setApplicationDescription(appVersion);
    parser.addOptions(kCmdOptions);

    // Process the actual command line arguments given by the user
    // return NgError::ERR_CODE::ERR_WRONG_ARG if QCommandLineParser fails.
    QStringList args = QCoreApplication::arguments();
    if (!parser.parse(args))
    {
#ifdef __DEBUG__
        qDebug() << "cmd args: " << args;
#endif
        NgLogger::criticalError(tr("Error syntax: %1\nTo list the available options use: %2 --help\n")
                                        .arg(parser.errorText(), appName),
                                NgError::ERR_CODE::ERR_WRONG_ARG);
        return false; // end of game
    }

    // 0.: Set the quiet mode if needed ;)
    if (parser.isSet(kOptionNames[Opt::QUIET]))
    {
        NgLogger::setDebug(NgLogger::DebugLevel::None);
        postingParams->_quiet            = true;
        postingParams->_dispProgressBar  = false;
        postingParams->_dispFilesPosting = false;
    }

    // 1.: show version ?
    if (parser.isSet(kOptionNames[Opt::VERSION]))
    {
        ngPost.showVersionASCII();
        return false; // end of game :)
    }

    // 2.: Change lang (for the help => we don't return!)
    if (parser.isSet(kOptionNames[Opt::LANG]))
    {
        QString lang = parser.value(kOptionNames[Opt::LANG]).toLower();
        qDebug() << "Lang: " << lang << "\n";
        ngPost.changeLanguage(lang);
    }

    // 3.: show help ?
    if (parser.isSet(kOptionNames[Opt::HELP]))
    {
        ngPost.showVersionASCII();
        // we need to process the events to print the version before the syntax
        // cause the syntax is using directly stdout without NgLogger
        qApp->processEvents();
        syntax(ngPost, appName);
        return false; // end of game :)
    }

    // 4.: Either load the config given in parameter or the default one
    if (parser.isSet(kOptionNames[Opt::CONF]))
    {
        QStringList errors =
                NgConfigLoader::loadConfig(ngPost, parser.value(kOptionNames[Opt::CONF]), postingParams);
        if (!errors.isEmpty())
        {
            NgLogger::criticalError(errors.join("\n"), NgError::ERR_CODE::ERR_CONF_FILE);
            return false; // end of game :(
        }
    }
    else
    {
        QStringList errors = ngPost.parseDefaultConfig();
        if (!errors.isEmpty())
        {
            NgLogger::criticalError(errors.join("\n"), NgError::ERR_CODE::ERR_CONF_FILE);
            return false; // end of game :(
        }
    }

    // 5.: debug level
    if (parser.isSet(kOptionNames[Opt::DEBUG]))
    {
        NgLogger::setDebug(NgLogger::DebugLevel::Debug);
        qDebug() << tr("Extra logs are ON\n");
    }
    if (parser.isSet(kOptionNames[Opt::DEBUG_FULL]))
    {
        NgLogger::setDebug(NgLogger::DebugLevel::FullDebug);
        qDebug() << tr("Full debug logs are ON\n");
    }

    // 6.: Set the progress display
    if (parser.isSet(kOptionNames[Opt::DISP_PROGRESS]))
    {
        QString val = parser.value(kOptionNames[Opt::DISP_PROGRESS]);
        postingParams->setDisplayProgress(val.trimmed());
    }

    // 7.: Load server(s) if given (so we can do a nzbCheck if required)
    if (!loadServersParameters(parser, postingParams))
        return false; // end of game :( (error has been sent)

    // 8.: is it an nzbCheck demand ? (no posting)
    if (parser.isSet(kOptionNames[Opt::CHECK]))
    {
        QFileInfo fi(parser.value(kOptionNames[Opt::CHECK]).trimmed());
        if (!fi.exists())
        {
            NgLogger::error(tr("Input file %1 doesn't exist...").arg(fi.absoluteFilePath()));
            return false; // end of Game :(
        }
        // for testing
        if (parser.isSet("o"))
        {
            QString nzbPath = parser.value(kOptionNames[Opt::OUTPUT]);
            if (QDir(nzbPath).exists())
                nzbPath = QFileInfo(QDir(nzbPath), QString("%1.nzb").arg(fi.baseName())).absoluteFilePath();
            else
            {
                NgLogger::error(tr("Destination directory %1 doesn't exist...").arg(nzbPath));
                return false; // end of Game :(
            }
            if (!postingParams->overwriteNzb())
                postingParams->_nzbPath = NgTools::substituteExistingFile(
                        QFileInfo(QDir(nzbPath), QString("%1.nzb").arg(fi.baseName())).absoluteFilePath());
        }
        return ngPost.doNzbCheck(parser.value(kOptionNames[Opt::CHECK]).trimmed());
    }

    // 9.: check if we've inputs (either files, auto directory or monitoring one
    if (!parser.isSet(kOptionNames[Opt::INPUT]) && !parser.isSet(kOptionNames[Opt::AUTO_DIR])
        && !parser.isSet(kOptionNames[Opt::MONITOR_DIR]))
    {
        NgLogger::criticalError(
                tr("Error syntax: you should provide at least one input file or directory using the option -i, "
                   "--auto or --monitor"),
                NgError::ERR_CODE::ERR_NO_INPUT);
        return false; // end of game :(
    }

    // 10.: verify validity of automatic deletion
    // (MB_TODO: why not for single files? forgot reason...)
    if (parser.isSet(kOptionNames[Opt::DEL_AUTO]))
    {
        if (!parser.isSet(kOptionNames[Opt::AUTO_DIR]) && !parser.isSet(kOptionNames[Opt::MONITOR_DIR]))
        {
            NgLogger::criticalError(tr("Error syntax: --del option is only available with --auto or --monitor"),
                                    NgError::ERR_CODE::ERR_DEL_AUTO);
            return false; // end of game :(
        }
        else
            postingParams->_delAuto = true;
    }

    if (!loadPostingParameters(parser, postingParams))
        return false; // end of game :( (error has been sent)

    // 11.: packing compression obfuscation settings
    if (!loadPackingParameters(parser, postingParams))
        return false; // end of game :( (error has been sent)

    // 12.1: single post using -i can provide the rar name
    QString rarName;
    if (parser.isSet(kOptionNames[Opt::RAR_NAME]))
        rarName = parser.value(kOptionNames[Opt::RAR_NAME]);
    // 12.2: pass for all archives whatever the mode
    QString rarPass;
    if (parser.isSet(kOptionNames[Opt::RAR_PASS]))
    {
        rarPass                      = parser.value(kOptionNames[Opt::RAR_PASS]);
        postingParams->_rarPassFixed = rarPass;
    }
    // 12.3: we could also get a fixed poster email (from)
    std::string from;
    if (parser.isSet(kOptionNames[Opt::FROM]))
    {
        QString            val = parser.value(kOptionNames[Opt::FROM]);
        QRegularExpression email("\\w+@\\w+\\.\\w+");
        if (!email.match(val).hasMatch())
            val += "@ngPost.com";
        from = NgTools::escapeXML(val).toStdString();
    }
    // or why may generate a new one for each post(s)
    else if (parser.isSet(kOptionNames[Opt::GEN_FROM]))
    {
        postingParams->_genFrom = true;
        if (!postingParams->quietMode())
            NgLogger::log(tr("Generate new random poster for each post"), true);
        from = NgTools::randomStdFrom();
    }
    else if (from.empty())
        from = NgTools::randomStdFrom();

    // 13.: do we have some auto directory to process? fill the list!
    QList<QDir> autoDirs;
    if (!getAutoDirectories(autoDirs, parser, postingParams))
        return false; // end of game :( (error has been sent)

    // 14.: do we have folders to monitor?
    bool isMonitoring = false;
    if (!startFoldersMonitoring(isMonitoring, parser, ngPost, postingParams))
        return false; // end of game :( (error has been sent)

    // 15.: get single files to post (-i option), the nzbName and nzbPath
    QList<QFileInfo> filesToUpload;
    if (!getInputFilesToGroupPost(filesToUpload, parser, postingParams))
        return false; // end of game :( (error has been sent)

#ifdef __DEBUG__
    postingParams->dumpParams();
    qDebug() << "[MB_TRACE]filesToUpload: " << filesToUpload;
#endif

    // 16.: start posting the filesToUpload (if we have some)
    if (!filesToUpload.isEmpty())
        prepareAndStartPostingSingleFiles(rarName, rarPass, from, filesToUpload, parser, ngPost, postingParams);

    // 17.: do auto directories
    for (QDir const &dir : autoDirs)
    {
        if (!postingParams->quietMode())
            NgLogger::log(QString("===> Auto dir: %1").arg(dir.absolutePath()), true);
        for (QFileInfo const &fileInfo :
             dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
            ngPost.post(fileInfo,
                        postingParams->_monitorNzbFolders ? QDir(fileInfo.absolutePath()).dirName() : QString());
    }

    if (filesToUpload.isEmpty() && autoDirs.isEmpty() && !isMonitoring)
    {
        NgLogger::criticalError(tr("Nothing to do..."), NgError::ERR_CODE::ERR_NO_INPUT);
        return false; // end of game :( (error has been sent)
    }

    return true;
}

void NgCmdLineLoader::prepareAndStartPostingSingleFiles(QString                  &rarName,
                                                        QString                  &rarPass,
                                                        std::string const        &from,
                                                        QList<QFileInfo>         &filesToUpload,
                                                        QCommandLineParser const &parser,
                                                        NgPost                   &ngPost,
                                                        SharedParams             &postingParams)
{
    // The nzbName will be the one of the first input file
    QString nzbName = ngPost.getNzbName(filesToUpload.front());

    // 16.: is the nzb output path clearly given?
    QString nzbPath = postingParams->nzbPath();
    if (parser.isSet("o"))
    {
        QFileInfo nzb(parser.value(kOptionNames[Opt::OUTPUT]));
        nzbName = ngPost.getNzbName(nzb);
        nzbPath = nzb.absolutePath();
    }
    if (rarName.isEmpty())
        rarName = filesToUpload.front().baseName();

    if (postingParams->doCompress())
    {
        if (postingParams->genName())
            rarName = NgTools::randomFileName(postingParams->lengthName());
        if (postingParams->genPass())
            rarPass = NgTools::randomPass(postingParams->lengthPass());
    }

    // generic meta (keyword, value) data for the nzb headers
    // (ideally different than password or file name as ngPost will set them automatically)
    // can be for example: category=cppCourse
    QMap<QString, QString> metaHeadNzb;
    if (parser.isSet(kOptionNames[Opt::META]))
    {
        for (QString const &meta : parser.values(kOptionNames[Opt::META]))
        {
            QStringList mList = meta.split("=");
            if (mList.size() == 2)
                metaHeadNzb.insert(NgTools::escapeXML(mList[0]), NgTools::escapeXML(mList[1]));
        }
        if (metaHeadNzb.contains("password") && rarPass.isEmpty())
        {
            rarPass = metaHeadNzb["password"]; // we will use the password for compression
            metaHeadNzb.remove("password");    // but won't put it twice in the header of the nzb ;)
        }
    }
    qDebug() << "Start posting job for " << nzbName << " with rarName: " << rarName << " and pass: " << rarPass;
    ngPost.startPostingJob(rarName,
                           rarPass,
                           QFileInfo(QDir(nzbPath), nzbName).absoluteFilePath(),
                           filesToUpload,
                           from,
                           metaHeadNzb);
}

bool NgCmdLineLoader::getInputFilesToGroupPost(QList<QFileInfo>         &filesToUpload,
                                               QCommandLineParser const &parser,
                                               SharedParams             &postingParams)
{
    for (QString const &filePath : parser.values(kOptionNames[Opt::INPUT]))
    {
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists() || !fileInfo.isReadable())
        {
            NgLogger::criticalError(
                    tr("Error: the input file '%1' is not readable...").arg(parser.value("input")),
                    NgError::ERR_CODE::ERR_INPUT_READ);
            return false; // end of game :(
        }
        else
        {
            if (fileInfo.isFile())
            {
                filesToUpload << fileInfo;
                NgLogger::log(tr("+ File to %1: %2 (%3)")
                                      .arg(postingParams->_doCompress ? tr("compress") : tr("post"))
                                      .arg(fileInfo.fileName())
                                      .arg(NgTools::humanSize(fileInfo.size())),
                              true,
                              NgLogger::DebugLevel::Debug);
            }
            else
            { // it's a directory
                if (postingParams->_doCompress)
                { // with compression
                    NgLogger::log(tr("+ Adding folder to Compress: %1").arg(fileInfo.absoluteFilePath()),
                                  true,
                                  NgLogger::DebugLevel::Debug);
                    filesToUpload << fileInfo;
                }
                else
                { // directory without compression
                    QDir dir(fileInfo.absoluteFilePath());
                    for (QFileInfo const &subFile : dir.entryInfoList(QDir::Files, QDir::Name))
                    {
                        // loop on all subFiles (firest level) and keep the readables
                        if (subFile.isReadable())
                        {
                            filesToUpload << subFile;
                            NgLogger::log(tr("+ subFile to post: %1").arg(subFile.fileName()),
                                          true,
                                          NgLogger::DebugLevel::Debug);
                        }
                        else
                        {
                            NgLogger::criticalError(tr("Error: the input subfile '%1' is not readable...")
                                                            .arg(subFile.absoluteFilePath()),
                                                    NgError::ERR_CODE::ERR_INPUT_READ);
                            return false; // end of game :(
                        }
                    } // for subfiles 1 level under directory
                }
            } // directory case
        }     // for all filePaths in input

        if (filesToUpload.isEmpty())
        {
            NgLogger::criticalError(tr("Error: the input folder '%1' has no files... (no recursivity without "
                                       "--compress)")
                                            .arg(fileInfo.absoluteFilePath()),
                                    NgError::ERR_CODE::ERR_INPUT_READ);
            return false; // end of game :(
        }
    }
    return true; // continue the game ;)
}

bool NgCmdLineLoader::startFoldersMonitoring(bool                     &isMonitoring,
                                             QCommandLineParser const &parser,
                                             NgPost                   &ngPost,
                                             SharedParams             &postingParams)
{
    if (parser.isSet(kOptionNames[Opt::MONITOR_DIR]))
    {
        if (!postingParams->_doCompress && (!postingParams->_doPar2 || !postingParams->_monitorIgnoreDir))
        {
            NgLogger::criticalError(
                    tr("Error syntax: --monitor only works with --compress or with --gen_par2 ONLY IF "
                       "MONITOR_IGNORE_DIR is enabled in config (--pack can be used)"),
                    NgError::ERR_CODE::ERR_MONITOR_NO_COMPRESS);
            return false; // end of game :(
        }
        for (QString const &filePath : parser.values(kOptionNames[Opt::MONITOR_DIR]))
        {
            QFileInfo fi(filePath);
            if (!fi.exists() || !fi.isDir())
            {
                NgLogger::criticalError(tr("Error syntax: --monitor only uses folders as argument..."),
                                        NgError::ERR_CODE::ERR_MONITOR_INPUT);
                return false; // end of game :(
            }
            else
            {
                ngPost.startFolderMonitoring(fi.absoluteFilePath());
                isMonitoring = true;
            }
        }
    }
    return true; // continue the game ;)
}

bool NgCmdLineLoader::getAutoDirectories(QList<QDir>              &autoDirs,
                                         QCommandLineParser const &parser,
                                         SharedParams             &postingParams)
{
    if (parser.isSet(kOptionNames[Opt::AUTO_DIR]))
    {
        if (!postingParams->_doCompress && !postingParams->_doPar2)
        { // MB_TODO: not sure why? why ok with par2 but no compression?
            NgLogger::criticalError(
                    tr("Error syntax: --auto only works with --compress or --gen_par2 or --pack with at least "
                       "the keywords COMPRESS or GEN_PAR2 in PACK config"),
                    NgError::ERR_CODE::ERR_AUTO_NO_COMPRESS);
            return false; // end of game :(
        }
        for (QString const &filePath : parser.values(kOptionNames[Opt::AUTO_DIR]))
        {
            QFileInfo fi(filePath);
            if (!fi.exists() || !fi.isDir())
            {
                NgLogger::criticalError(tr("Error syntax: --auto only uses folders as argument..."),
                                        NgError::ERR_CODE::ERR_AUTO_INPUT);
                return false; // end of game :(
            }
            else
            {
                autoDirs << QDir(fi.absoluteFilePath());
                if (!postingParams->_doCompress)
                {
                    // only genPar2 => only accept files
                    // cause no recursivity on subfolders! Eurêka!
                    QStringList subFolders = autoDirs.last().entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                    if (subFolders.size())
                    {
                        NgLogger::criticalError(
                                tr("Error: you can only --auto without compression on folders that DON'T have "
                                   "any "
                                   "subfolders.\nThat's not the case for '%1' which contains folders: %2")
                                        .arg(fi.fileName(), subFolders.join(", ")),
                                NgError::ERR_CODE::ERR_AUTO_INPUT);
                        return false; // end of game :(
                    }
                }
            }
        }
    }
    return true; // continue the game ;)
}

bool NgCmdLineLoader::loadPostingParameters(QCommandLineParser const &parser, SharedParams &postingParams)
{
    // number of threads
    if (parser.isSet(kOptionNames[Opt::THREAD]))
    {
        bool ok;
        postingParams->_nbThreads = parser.value(kOptionNames[Opt::THREAD]).toUShort(&ok);
        if (!ok)
        {
            NgLogger::criticalError(tr("You should give an integer for the number of threads (option -t)"),
                                    NgError::ERR_CODE::ERR_NB_THREAD);
            return false; // end of game :'(
        }
        if (postingParams->_nbThreads < 1)
            postingParams->_nbThreads = 1;
        // the case _nbThreads > QThread::idealThreadCount() is handled in PostingJob::_postFiles
    }

    // number of retries to resend an article that failed posting
    if (parser.isSet(kOptionNames[Opt::NB_RETRY]))
    {
        bool   ok;
        ushort nbRetry = parser.value(kOptionNames[Opt::NB_RETRY]).toUShort(&ok);
        if (ok)
            NntpArticle::setNbMaxRetry(nbRetry);
        else
        {
            NgLogger::criticalError(
                    tr("You should give an unisgned integer for the number of retry for posting an Article "
                       "(option "
                       "-r)"),
                    NgError::ERR_CODE::ERR_NB_RETRY);
            return false; // end of game :'(
        }
    }

    // article size and signature
    if (parser.isSet(kOptionNames[Opt::ARTICLE_SIZE]))
    {
        bool    ok;
        quint64 size = parser.value(kOptionNames[Opt::ARTICLE_SIZE]).toULongLong(&ok);
        if (ok)
            NgConf::kArticleSize = size;
        else
        {
            NgLogger::criticalError(tr("You should give an integer for the article size (option -a)"),
                                    NgError::ERR_CODE::ERR_ARTICLE_SIZE);
            return false; // end of game :'(
        }
    }
    if (parser.isSet(kOptionNames[Opt::MSG_ID]))
        NgConf::kArticleIdSignature = NgTools::escapeXML(parser.value(kOptionNames[Opt::MSG_ID])).toStdString();

    // groups
    if (parser.isSet(kOptionNames[Opt::GROUPS]))
        postingParams->updateGroups(parser.value(kOptionNames[Opt::GROUPS]));

    // obfuscation
    if (parser.isSet(kOptionNames[Opt::OBFUSCATE]))
    {
        postingParams->_obfuscateArticles = true;
        if (!postingParams->quietMode())
            NgLogger::log(tr("Do article obfuscation (the subject of each Article will be a UUID)\n"), true);
    }
    return true; // continue the game ;)
}

bool NgCmdLineLoader::loadServersParameters(QCommandLineParser const &parser, SharedParams &postingParams)
{
    // 1.: NNTP server following the format (<user>:<pass>@@@)?<host>:<port>:<nbCons>:(no)?ssl
    if (parser.isSet(kOptionNames[Opt::SERVER]))
    {
        auto &nntpServers = postingParams->_nntpServers;
        nntpServers.clear(); // we provide them command line so we don't want to use those from config!
        QRegularExpression regExp(kNntpServerStrRegExp, QRegularExpression::CaseInsensitiveOption);
        for (QString const &serverParam : parser.values(kOptionNames[Opt::SERVER]))
        {
            QRegularExpressionMatch match = regExp.match(serverParam);
            if (match.hasMatch())
            {
                bool    auth  = !match.captured(1).isEmpty();
                QString user  = match.captured(2);
                QString pass  = match.captured(3);
                QString host  = match.captured(4);
                ushort  port  = match.captured(5).toUShort();
                int     nbCon = match.captured(6).toInt();
                bool    ssl   = match.captured(7).isEmpty();
#ifdef __DEBUG__
                qDebug() << "NNTP Server: " << user << ":" << pass << "@" << host << ":" << port << ":" << nbCon
                         << ":" << ssl;
#endif
                nntpServers << new NntpServerParams(
                        host, port, auth, user.toStdString(), pass.toStdString(), nbCon, ssl);
            }
            else
            {
                NgLogger::criticalError(tr("Syntax error on server details for %1, the format should be: %2")
                                                .arg(serverParam)
                                                .arg("(<user>:<pass>@@@)?<host>:<port>:<nbCons>:(no)?ssl"),
                                        NgError::ERR_CODE::ERR_SERVER_REGEX);
                return false; // end of game :'(
            }
        }
    }

    // 2.: NNTP server given with separate arguments (host, port,...)
    if (parser.isSet(kOptionNames[Opt::HOST]))
    {
        // first get the host
        QString host = parser.value(kOptionNames[Opt::HOST]);

        auto &nntpServers = postingParams->_nntpServers;
        nntpServers.clear();
        if (!parser.isSet(kOptionNames[Opt::SERVER]))
            nntpServers.clear(); // we clear only those from config (i.e: if not one from 1.)

        // Add the server and check if we can fill it more
        NntpServerParams *server = new NntpServerParams(host);
        nntpServers << server;
        if (parser.isSet(kOptionNames[Opt::SSL]))
        {
            server->useSSL = true;
            server->port   = NntpServerParams::kDefaultSslPort;
        }
        if (parser.isSet(kOptionNames[Opt::PORT]))
        {
            bool   ok;
            ushort port = parser.value(kOptionNames[Opt::PORT]).toUShort(&ok);
            if (ok)
                server->port = port;
            else
            {
                NgLogger::criticalError(tr("You should give an integer for the port (option -P)"),
                                        NgError::ERR_CODE::ERR_SERVER_PORT);
                return false; // end of game :'(
            }
        }
        if (parser.isSet(kOptionNames[Opt::USER]))
        {
            server->auth = true;
            server->user = parser.value(kOptionNames[Opt::USER]).toStdString();

            if (parser.isSet(kOptionNames[Opt::PASS]))
                server->pass = parser.value(kOptionNames[Opt::PASS]).toStdString();
        }

        if (parser.isSet(kOptionNames[Opt::CONNECTION]))
        {
            bool ok;
            int  nbCons = parser.value(kOptionNames[Opt::CONNECTION]).toInt(&ok);
            if (ok)
                server->nbCons = nbCons;
            else
            {
                NgLogger::criticalError(
                        tr("You should give an integer for the number of connections (option -n)"),
                        NgError::ERR_CODE::ERR_SERVER_CONS);
                return false; // end of game :'(
            }
        }
    }

    return true; // continue the game ;)
}

bool NgCmdLineLoader::loadPackingParameters(QCommandLineParser const &parser, SharedParams &postingParams)
{
    // General packing attributes:
    if (parser.isSet(kOptionNames[Opt::PACK]))
        postingParams->enableAutoPacking();
    if (parser.isSet(kOptionNames[Opt::COMPRESS]))
        postingParams->_doCompress = true;
    if (parser.isSet(kOptionNames[Opt::GEN_PAR2]))
        postingParams->_doPar2 = true;
    if (parser.isSet(kOptionNames[Opt::GEN_NAME]))
        postingParams->_genName = true;
    if (parser.isSet(kOptionNames[Opt::GEN_PASS]))
        postingParams->_genPass = true;
    if (parser.isSet(kOptionNames[Opt::RAR_NO_ROOT_FOLDER]))
        postingParams->_rarNoRootFolder = true;

    // compression settings
    if (parser.isSet(kOptionNames[Opt::TMP_DIR]))
        postingParams->_tmpPath = parser.value(kOptionNames[Opt::TMP_DIR]);
    if (parser.isSet(kOptionNames[Opt::RAR_PATH]))
        postingParams->_rarPath = parser.value(kOptionNames[Opt::RAR_PATH]);
    if (parser.isSet(kOptionNames[Opt::RAR_SIZE]))
    {
        bool ok;
        uint nb = parser.value(kOptionNames[Opt::RAR_SIZE]).toUInt(&ok);
        if (ok)
            postingParams->_rarSize = nb;
    }
    if (parser.isSet(kOptionNames[Opt::RAR_MAX]))
    {
        bool ok;
        uint nb = parser.value(kOptionNames[Opt::RAR_MAX]).toUInt(&ok);
        if (ok)
        {
            postingParams->_rarMax    = nb;
            postingParams->_useRarMax = true;
        }
    }
    if (parser.isSet(kOptionNames[Opt::LENGTH_NAME]))
    {
        bool ok;
        uint nb = parser.value(kOptionNames[Opt::LENGTH_NAME]).toUInt(&ok);
        if (ok)
            postingParams->_lengthName = nb;
    }
    if (parser.isSet(kOptionNames[Opt::LENGTH_PASS]))
    {
        bool ok;
        uint nb = parser.value(kOptionNames[Opt::LENGTH_PASS]).toUInt(&ok);
        if (ok)
            postingParams->_lengthPass = nb;
    }

    // par2 redundancy section
    if (parser.isSet(kOptionNames[Opt::PAR2_PCT]))
    {
        bool ok;
        uint nb = parser.value(kOptionNames[Opt::PAR2_PCT]).toUInt(&ok);
        if (ok)
        {
            postingParams->_par2Pct = nb;
            if (nb > 0)
                postingParams->_doPar2 = true;
        }
    }
    if (parser.isSet(kOptionNames[Opt::PAR2_PATH]))
    {
        QString val = parser.value(kOptionNames[Opt::PAR2_PATH]);
        if (!val.isEmpty())
        {
            QFileInfo fi(val);
            if (fi.exists() && fi.isFile() && fi.isExecutable())
                postingParams->_par2Path = val;
            else
            {
                NgLogger::criticalError(tr("The given par2 path is not executable: %1").arg(val),
                                        NgError::ERR_CODE::ERR_PAR2_PATH);
                return false; // end of game :'(
            }
        }
    }
    if (postingParams->_doPar2 && postingParams->_par2Pct == 0 && postingParams->_par2Args.isEmpty())
    {
        NgLogger::criticalError(
                tr("Error: can't generate par2 if the redundancy percentage is null or PAR2_ARGS is not "
                   "provided...\nEither use --par2_pct or set PAR2_PCT or PAR2_ARGS in the config file."),
                NgError::ERR_CODE::ERR_PAR2_ARGS);
        return false; // end of game :'(
    }

    return true; // continue the game ;)
}

void NgCmdLineLoader::syntax(NgPost const &ngPost, char *appName)
{
    QString     app = QFileInfo(appName).fileName();
    QTextStream cout(stdout);
    cout << ngPost.desc() << "\n"
         << tr("Syntax: ") << app
         << " (options)* (-i <file or folder> | --auto <folder> | --monitor <folder>)+\n";
    for (QCommandLineOption const &opt : kCmdOptions)
    {
        if (opt.valueName() == kOptionNames[Opt::SERVER])
            cout << "\n// "
                 << tr("you can provide servers in one string using -S and/or split the parameters for ONE "
                       "SINGLE "
                       "server (this will overwrite the configuration file)")
                 << "\n";
        else if (opt.valueName() == kOptionNames[Opt::TMP_DIR])
            cout << "\n// " << tr("for compression and par2 support") << "\n";
        else if (opt.valueName() == kOptionNames[Opt::AUTO_DIR])
            cout << "\n// " << tr("automated posting (scanning and/or monitoring)") << "\n";
        else if (opt.valueName() == kOptionNames[Opt::INPUT])
            cout << "\n// " << tr("quick posting (several files/folders)") << "\n";
        else if (opt.valueName() == kOptionNames[Opt::OBFUSCATE])
            cout << "\n// " << tr("general options") << "\n";

        if (opt.names().size() == 1)
            cout << QString("\t--%1: %2\n")
                            .arg(opt.names().first(), -17)
                            .arg(tr(opt.description().toLocal8Bit().constData()));
        else
            cout << QString("\t-%1: %2\n")
                            .arg(opt.names().join(" or --"), -18)
                            .arg(tr(opt.description().toLocal8Bit().constData()));
    }

    cout << "\n"
         << tr("Examples:") << "\n"
         << "  - " << tr("with monitoring") << ": " << app
         << " --monitor /data/folder1 --monitor /data/folder2 --auto_compress --rm_posted --disp_progress "
            "files\n"
         << "  - " << tr("with auto post") << ": " << app
         << " --auto /data/folder1 --auto /data/folder2 --compress --gen_par2 --gen_name --gen_pass --rar_size "
            "42 "
            "--disp_progress files\n"
         << "  - " << tr("with compression, filename obfuscation, random password and par2") << ": " << app
         << " -i /tmp/file1 -i /tmp/folder1 -o /nzb/myPost.nzb --compress --gen_name --gen_pass --gen_par2\n"
         << "  - " << tr("with config file") << ": " << app
         << " -c ~/.ngPost -m \"password=qwerty42\" -f ngPost@nowhere.com -i /tmp/file1 -i /tmp/file2 -i "
            "/tmp/folderToPost1 -i /tmp/folderToPost2\n"
         << "  - " << tr("with all params") << ":  " << app
         << " -t 1 -m \"password=qwerty42\" -m \"metaKey=someValue\" -h news.newshosting.com -P 443 -s -u user -p pass -n 30 -f ngPost@nowhere.com \
                -g \"alt.binaries.test,alt.binaries.test2\" -a 64000 -i /tmp/folderToPost -o /tmp/folderToPost.nzb\n"
         << "\n"
         << tr("If you don't provide the output file (nzb file), we will create it in the nzbPath with the "
               "name of "
               "the first file or folder given in the command line.")
         << "\n"
         << tr("so in the second example above, the nzb would be: /tmp/file1.nzb") << "\n"
         << MB_FLUSH;
}
