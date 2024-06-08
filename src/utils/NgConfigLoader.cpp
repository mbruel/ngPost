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
#include "NgConfigLoader.h"

#include <QFileInfo>
#include <QUrl>

#include "FoldersMonitorForNewFiles.h"
#include "NgConf.h"
#include "NgPost.h"
#include "NgTools.h"
#include "nntp/NntpArticle.h" // for NNTP::Article::setNbMaxRetry
#include "nntp/ServerParams.h"

using namespace NgConf;

QStringList NgConfigLoader::loadConfig(NgPost &ngPost, QString const &configPath, SharedParams &mainParams)
{
    QFileInfo fileInfo(configPath);
    if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable())
        return { tr("The config file '%1' is not readable...").arg(configPath) };

    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return { tr("The config file '%1' could not be opened...").arg(configPath) };

    QStringList         errors;
    NNTP::ServerParams *serverParams = nullptr;                  // hold servers found (one by one)
    auto               &_nntpServers = mainParams->_nntpServers; // to fill the SharedParams servers

    QTextStream stream(&file);
    int         lineNumber = 0;
    while (!stream.atEnd())
    {
        // parse the config (line by line)
        QString line = stream.readLine().trimmed();
        ++lineNumber;
        if (line.isEmpty() || line.startsWith('#') || line.startsWith('/'))
            continue; // Jump empty or comment line

        else if (line == "[server]")
        { // only section. all other lines formed as KEY = VALUE
            serverParams = new NNTP::ServerParams();
            _nntpServers << serverParams;
            continue; // next line!
        }
        else
        {
            QStringList args = line.split('=');
            if (args.size() >= 2)
            {
                const QString opt = args.takeFirst().trimmed().toLower(), val = args.at(0).trimmed();
                bool          ok = false;

                // Finishing getting Server Paramaters
                if (opt == kOptionNames[Opt::HOST])
                    serverParams->host = val;
                else if (opt == kOptionNames[Opt::PORT])

                    setUShort(serverParams->port, val, errors, lineNumber, opt.toUpper());
                else if (opt == kOptionNames[Opt::SSL])
                {
                    setBoolean(serverParams->useSSL, val);
                    if (serverParams->useSSL && serverParams->port == NNTP::ServerParams::kDefaultPort)
                        serverParams->port = NNTP::ServerParams::kDefaultSslPort;
                }
                else if (opt == kOptionNames[Opt::ENABLED])
                    setBoolean(serverParams->enabled, val);
                else if (opt == kOptionNames[Opt::NZBCHECK])
                    setBoolean(serverParams->nzbCheck, val);

                else if (opt == kOptionNames[Opt::USER])
                {
                    serverParams->user = val.toStdString();
                    serverParams->auth = true;
                }
                else if (opt == kOptionNames[Opt::PASS])
                {
                    serverParams->pass = val.toStdString();
                    serverParams->auth = true;
                }
                else if (opt == kOptionNames[Opt::CONNECTION])
                    setUShort(serverParams->nbCons, val, errors, lineNumber, opt.toUpper());

                // one line keywords
                if (opt == kOptionNames[Opt::THREAD])
                {
                    int nb = val.toInt(&ok);
                    if (ok)
                    {
                        if (nb <= 1)
                            mainParams->_nbThreads = 1;
                        else if (nb < mainParams->_nbThreads)
                            mainParams->_nbThreads = nb;
                    }
                }
                else if (opt == kOptionNames[Opt::SOCK_TIMEOUT])
                {
                    int nb = val.toInt(&ok);
                    if (ok)
                    {
                        int timeout = nb * 1000; // value is in seconds in Config
                        if (timeout > kMinSocketTimeOut)
                            mainParams->_socketTimeOut = timeout;
                    }
                }

                else if (opt == kOptionNames[Opt::CONF_VERSION])
                {
                    static QRegularExpression versionReg("\\d+\\.\\d+");
                    if (versionReg.match(val).hasMatch())
                    {
                        kConfVersion = val;
                        qDebug() << "Config version : " << val;
                    }
                    else
                        addError(errors, lineNumber, tr("Format error for %1 : %2").arg(opt.toUpper()).arg(val));
                }
                else if (opt == kOptionNames[Opt::NZB_PATH])
                {
                    QFileInfo nzbFI(val);
                    if (nzbFI.exists() && nzbFI.isDir() && nzbFI.isWritable())
                    {
                        mainParams->_nzbPath     = val;
                        mainParams->_nzbPathConf = val;
                    }
                    else
                        addError(errors, lineNumber, tr("the nzbPath '%1' is not writable...").arg(val));
                }
                else if (opt == kOptionNames[Opt::NZB_UPLOAD_URL])
                { // MB_DEPRECATED!?!
                    QString urlNzbUploadStr = val;
                    // MB_TODO: not sure what we were doing here... Shall be removed?!?
                    for (int i = 2; i < args.size(); ++i)
                        urlNzbUploadStr += QString("=%1").arg(args.at(i).trimmed());

                    QUrl       *urlNzbUpload     = new QUrl(urlNzbUploadStr);
                    QStringList allowedProtocols = { "ftp", "http", "https" };
                    if (allowedProtocols.contains(urlNzbUpload->scheme()))
                    {
                        mainParams->_urlNzbUpload    = urlNzbUpload;
                        mainParams->_urlNzbUploadStr = urlNzbUploadStr;
                    }
                    else
                    {
                        addError(errors,
                                 lineNumber,
                                 tr("Unsupported protocol for NZB_UPLOAD_URL (%1). You can only use: %2\n")
                                         .arg(urlNzbUpload->toString(QUrl::RemoveUserInfo))
                                         .arg(allowedProtocols.join(", ")));
                        delete urlNzbUpload;
                    }
                }
                else if (opt == kOptionNames[Opt::RESUME_WAIT])
                {
                    ushort nb = val.toUShort(&ok);
                    if (ok && nb > kDefaultResumeWaitInSec)
                        mainParams->_waitDurationBeforeAutoResume = nb;
                }
                else if (opt == kOptionNames[Opt::NO_RESUME_AUTO])
                    setBoolean(mainParams->_tryResumePostWhenConnectionLost, val, true);
                else if (opt == kOptionNames[Opt::PREPARE_PACKING])
                    setBoolean(mainParams->_preparePacking, val);
                else if (opt == kOptionNames[Opt::MONITOR_FOLDERS])
                    setBoolean(mainParams->_monitorNzbFolders, val);
                else if (opt == kOptionNames[Opt::MONITOR_IGNORE_DIR])
                    setBoolean(mainParams->_monitorIgnoreDir, val);
                else if (opt == kOptionNames[Opt::MONITOR_SEC_DELAY_SCAN])
                {
                    ushort nb = val.toUShort(&ok);
                    if (ok && nb > 1 && nb <= 120)
                    {
                        mainParams->_monitorSecDelayScan   = nb;
                        FoldersMonitorForNewFiles::sMSleep = nb * 1000;
                    }
                }
                else if (opt == kOptionNames[Opt::NZB_RM_ACCENTS])
                    setBoolean(mainParams->_removeAccentsOnNzbFileName, val);
                else if (opt == kOptionNames[Opt::AUTO_CLOSE_TABS])
                    setBoolean(mainParams->_autoCloseTabs, val);

                else if (opt == kOptionNames[Opt::LOG_IN_FILE] && isBooleanTrue(val))
                    ngPost.startLogInFile();

                else if (opt == kOptionNames[Opt::MONITOR_EXT])
                {
                    for (QString const &extension : val.split(","))
                        mainParams->_monitorExtensions << extension.trimmed().toLower();
                }
                else if (opt == kOptionNames[Opt::OBFUSCATE])
                {
                    if (val.toLower().startsWith("article"))
                    {
                        mainParams->_obfuscateArticles = true;
                        NgLogger::log(
                                tr("Doing article obfuscation (the subject of each Article will be a UUID)"),
                                true);
                    }
                }
                else if (opt == kOptionNames[Opt::GROUP_POLICY])
                {
                    QString valLower = val.toLower();
                    if (valLower == kGroupPolicies[GROUP_POLICY::EACH_POST])
                    {
                        mainParams->_groupPolicy = GROUP_POLICY::EACH_POST;
                        if (!ngPost.quietMode())
                            NgLogger::log(tr("Group Policy EACH_POST: each post on a different group"), true);
                    }
                    else if (val == kGroupPolicies[GROUP_POLICY::EACH_FILE])
                    {
                        mainParams->_groupPolicy = GROUP_POLICY::EACH_FILE;
                        if (!ngPost.quietMode())
                            NgLogger::log(tr("Group Policy EACH_FILE: each file on a different group"), true);
                    }
                    else if (!ngPost.quietMode())
                        NgLogger::log(tr("Group Policy ALL: posting everything on all newsgroup"), true);
                }
                else if (opt == kOptionNames[Opt::DISP_PROGRESS])
                    mainParams->setDisplayProgress(val);
                else if (opt == kOptionNames[Opt::MSG_ID])
                {
                    kArticleIdSignature = val.toStdString();
                    NgLogger::log(tr("Using personal signature for articles: %1").arg(val), true);
                }
                else if (opt == kOptionNames[Opt::ARTICLE_SIZE])
                    setUInt64(kArticleSize, val, errors, lineNumber, opt.toUpper());
                else if (opt == kOptionNames[Opt::NB_RETRY])
                {
                    ushort nb = val.toUShort(&ok);
                    if (ok)
                        NNTP::Article::setNbMaxRetry(nb);
                }
                else if (opt == kOptionNames[Opt::FROM])
                {
                    QString            email = val;
                    QRegularExpression regex("\\w+@\\w+\\.\\w+");
                    if (!regex.match(email).hasMatch())
                        email += "@ngPost.com";

                    email                 = NgTools::escapeXML(email);
                    mainParams->_from     = email.toStdString();
                    mainParams->_saveFrom = true;
                }
                else if (opt == kOptionNames[Opt::GEN_FROM])
                {
                    setBoolean(mainParams->_genFrom, val);
                    if (!ngPost.quietMode())
                        NgLogger::log(tr("Generate new random poster for each post"), true);
                }
                else if (opt == kOptionNames[Opt::GROUPS])
                    mainParams->updateGroups(val);

                else if (opt == kOptionNames[Opt::LANG])
                    ngPost.changeLanguage(val.toLower());

                else if (opt == kOptionNames[Opt::SHUTDOWN_CMD])
                    ngPost.setShutdownCmd(val);

                else if (opt == kOptionNames[Opt::PROXY_SOCKS5])
                    ngPost.setProxy(val);

                else if (opt == kOptionNames[Opt::NZB_POST_CMD])
                    mainParams->_nzbPostCmd << args.join("=").trimmed();

                else if (opt == kOptionNames[Opt::INPUT_DIR])
                {
                    QFileInfo fi(val);
                    if (fi.exists() && fi.isDir())
                        mainParams->_inputDir = val;
                    else
                        addError(errors, lineNumber, tr("Error %1 should be a directory").arg(opt.toUpper()));
                }

                // MB_TODO the post history that is now migrating in SQLite :)

                //                else if (opt == kOptionNames[Opt::POST_HISTORY])
                //                {
                //                    _postHistoryFile = val;
                //                    QFileInfo fi(val);
                //                    if (fi.isDir())
                //                        err += tr("the post history '%1' can't be a directory...\n").arg(val);
                //                    else
                //                    {
                //                        if (fi.exists())
                //                        {
                //                            if (!fi.isWritable())
                //                                err += tr("the post history file '%1' is not
                //                                writable...\n").arg(val);
                //                        }
                //                        else if (!QFileInfo(fi.absolutePath()).isWritable())
                //                            err += tr("the post history file '%1' is not
                //                            writable...\n").arg(val);
                //                    }
                //                }

                //                else if (opt == kOptionNames[Opt::FIELD_SEPARATOR])
                //                    _historyFieldSeparator = val;

                // compression section

#ifdef __USE_TMP_RAM__
                else if (opt == kOptionNames[Opt::TMP_RAM])
                {
                    QString err = mainParams->setRamPathAndTestStorage(val);
                    if (!err.isEmpty())
                        addError(errors, lineNumber, tr("Error with %1: %2").arg(line, err));
                }
                else if (opt == kOptionNames[Opt::TMP_RAM_RATIO])
                {
                    double ratio = val.toDouble(&ok);
                    if (!ok || ratio < kRamRatioMin || ratio > kRamRatioMax)
                        errors << QString("%1 %2\n")
                                          .arg(kOptionNames[Opt::TMP_RAM_RATIO].toUpper())
                                          .arg(tr("should be a ratio between %1 and %2")
                                                       .arg(kRamRatioMin)
                                                       .arg(kRamRatioMax));
                    else
                        mainParams->_ramRatio = ratio;
                }
#endif
                else if (opt == kOptionNames[Opt::TMP_DIR])
                    mainParams->_tmpPath = val;
                else if (opt == kOptionNames[Opt::RAR_PATH])
                    mainParams->_rarPath = val;
                else if (opt == kOptionNames[Opt::RAR_PASS])
                {
                    mainParams->_rarPassFixed = val;
                    //                    mainParams->rarPass      = val;
                }
                else if (opt == kOptionNames[Opt::RAR_EXTRA])
                    mainParams->_rarArgs = val.split(kCmdArgsSeparator);
                else if (opt == kOptionNames[Opt::RAR_SIZE])

                    setUShort(mainParams->_rarSize, val, errors, lineNumber, opt.toUpper());

                else if (opt == kOptionNames[Opt::RAR_MAX])
                {
                    if (setUShort(mainParams->_rarMax, val, errors, lineNumber, opt.toUpper()))
                        mainParams->_useRarMax = true;
                }
                else if (opt == kOptionNames[Opt::KEEP_RAR])
                    setBoolean(mainParams->_keepRar, val);
                else if (opt == kOptionNames[Opt::AUTO_COMPRESS])
                {
                    if (ngPost.useHMI())
                        NgLogger::log(
                                tr("obsolete keyword AUTO_COMPRESS, you should use PACK instead, please click "
                                   "SAVE to "
                                   "update your conf and then go check it."),
                                true);
                    else
                        NgLogger::log(
                                tr("obsolete keyword AUTO_COMPRESS, you should use PACK instead, please refer "
                                   "to "
                                   "the conf "
                                   "example: %1")
                                        .arg("https://github.com/mbruel/ngPost/blob/master/ngPost.conf#L140"),
                                true);
                }
                else if (opt == kOptionNames[Opt::PACK])
                {
                    QStringList       packKeywords    = val.toLower().split(",");
                    const QStringList allowedKeywords = { kOptionNames[Opt::COMPRESS],
                                                          kOptionNames[Opt::GEN_NAME],
                                                          kOptionNames[Opt::GEN_PASS],
                                                          kOptionNames[Opt::GEN_PAR2] };
                    QStringList       wrongKeywords;
                    for (auto it = packKeywords.cbegin(), itEnd = packKeywords.cend(); it != itEnd; ++it)
                    {
                        QString keyWord = (*it).trimmed();
                        if (allowedKeywords.contains(keyWord))
                            mainParams->_packAutoKeywords << keyWord;
                        else
                            wrongKeywords << keyWord.toUpper();
                    }

                    if (wrongKeywords.size())
                        addError(errors,
                                 lineNumber,
                                 tr("Wrong keywords for PACK: %1. It should be a subset of (%2)")
                                         .arg(wrongKeywords.join(", "), allowedKeywords.join(", ").toUpper()));
                    else if (ngPost.useHMI())
                        mainParams->enableAutoPacking();
                }
                else if (opt == kOptionNames[Opt::RAR_NO_ROOT_FOLDER])
                    setBoolean(mainParams->_rarNoRootFolder, val);

                else if (opt == kOptionNames[Opt::PAR2_PCT])
                {
                    uint nb = val.toUInt(&ok);
                    if (ok)
                        mainParams->_par2Pct = nb;
                }
                else if (opt == kOptionNames[Opt::PAR2_PATH])
                {
                    if (!val.isEmpty())
                    {
                        QFileInfo fi(val);
                        if (fi.exists() && fi.isFile() && fi.isExecutable())
                        {
                            mainParams->_par2Path       = val;
                            mainParams->_par2PathConfig = val;
                        }
                        else
                            addError(errors,
                                     lineNumber,
                                     tr("%1 is not executable : %2").arg(opt.toUpper()).arg(val));
                    }
                }
                else if (opt == kOptionNames[Opt::PAR2_ARGS])
                    mainParams->_par2Args = val;
                else if (opt == kOptionNames[Opt::LENGTH_NAME])
                    setUShort(mainParams->_lengthName, val, errors, lineNumber, opt.toUpper());
                else if (opt == kOptionNames[Opt::LENGTH_PASS])
                    setUShort(mainParams->_lengthPass, val, errors, lineNumber, opt.toUpper());
            }
        }
    }
    file.close();

    // MB_TODO: question shall we?
    mainParams->setEmptyCompressionArguments(); // just in case nothing in config file and to set use7z!

    //    if (err.isEmpty() && !_postHistoryFile.isEmpty())
    //    {
    //        QFile file(_postHistoryFile);
    //        if (!file.exists() && file.open(QIODevice::WriteOnly | QIODevice::Text))
    //        {
    //            QTextStream stream(&file);
    //            stream << tr("date") << _historyFieldSeparator << tr("nzb name") << _historyFieldSeparator <<
    //            tr("size")
    //                   << _historyFieldSeparator << tr("avg. speed") << _historyFieldSeparator << tr("archive
    //                   name")
    //                   << _historyFieldSeparator << tr("archive pass") << _historyFieldSeparator <<
    //                   tr("groups")
    //                   << _historyFieldSeparator << tr("from") << "\n";

    //            file.close();
    //        }
    //    }

    return errors;
}
