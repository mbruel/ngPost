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
#ifndef NGCONF_H
#define NGCONF_H

#include <QCommandLineOption>
#include <QCoreApplication>
#include <QMap>
#include <QRegularExpression>
#include <QString>

namespace NgConf
{

constexpr char const *kAppName     = "ngPost";
inline const QString  kVersion     = QString::number(APP_VERSION);
inline QString        kConfVersion = QString();

inline const QRegularExpression kAppVersionRegExp =
        QRegularExpression("^DEFINES \\+= \"APP_VERSION=\\\\\"((\\d+)\\.(\\d+))\\\\\"\"$");

inline const QString kProFileURL = "https://raw.githubusercontent.com/mbruel/ngPost/master/src/ngPost.pri";
inline const QString kDonationURL =
        "https://www.paypal.com/cgi-bin/"
        "webscr?cmd=_donations&business=W2C236U6JNTUA&item_name=ngPost&currency_code=EUR";
inline const QString kDonationBtcURL = "https://github.com/mbruel/ngPost#donations";

inline const QString  kMainThreadName       = "MainThread";
constexpr char const *kFolderMonitoringName = QT_TRANSLATE_NOOP("NgPost", "Auto Posting");
constexpr char const *kQuickJobName         = QT_TRANSLATE_NOOP("NgPost", "Quick Post");
constexpr char const *kDonationTooltip =
        QT_TRANSLATE_NOOP("NgPost",
                          "Donations are welcome, I spent quite some time to develop this app "
                          "and make a sexy GUI although I'm not using it ;)");
constexpr char const *kDonationBtcTooltip = QT_TRANSLATE_NOOP(
        "NgPost", "Feel free to donate in BTC, click here to see my address on the GitHub section");

inline const QString kDefaultTxtEditorLinux = "gedit";
inline const QString kDefaultTxtEditorMacOS = "open";

constexpr char const *kDefaultShutdownCmdLinux   = "sudo -n /sbin/poweroff";
constexpr char const *kDefaultShutdownCmdWindows = "shutdown /s /f /t 0";
constexpr char const *kDefaultShutdownCmdMacOS   = "sudo -n shutdown -h now";

constexpr int kProgressbarBarWidth = 50;
constexpr int kDefaultRefreshRate  = 500; //!< how often shall we refresh the progressbar bar?

constexpr int kNbPreparedArticlePerConnection = 3;

constexpr int kDefaultResumeWaitInSec     = 30;
constexpr int kDefaultNumberOfConnections = 15;
constexpr int kDefaultSocketTimeOut       = 30000;
constexpr int kMinSocketTimeOut           = 5000;
constexpr int kDefaultArticleSize         = 716800;

constexpr char const *kDefaultSpace          = "  ";
constexpr char const *kDefaultMsgIdSignature = "ngPost";

#if defined(WIN32) || defined(__MINGW64__)
constexpr char const *kDefaultNzbPath = ""; //!< local folder
constexpr char const *kDefaultConfig  = "ngPost.conf";
#else
constexpr char const *kDefaultNzbPath         = "/tmp";
constexpr char const *kDefaultConfig          = ".ngPost";
#endif
constexpr char const *kDefaultLogFile = "ngPost.log";
constexpr char const *kDbHistoryFile  = "ngPost.sqlite";

constexpr uint kDefaultLengthName = 17;
constexpr uint kDefaultLengthPass = 13;
constexpr uint kDefaultRarMax     = 99;

constexpr char kCmdArgsSeparator = ' ';

constexpr char const *k7zip = "7z";
#if defined(__APPLE__) || defined(__MACH__)
constexpr char const *kDefaultRarExtraOptions = "-ep1 -m0 -x.DS_Store";
#else
constexpr char const *kDefaultRarExtraOptions = "-ep1 -m0";
#endif
constexpr char const *kDefault7zOptions = "-mx0 -mhe=on";

#ifdef __USE_TMP_RAM__
constexpr double kRamRatioMin = 1.10;
constexpr double kRamRatioMax = 2.;
#endif

constexpr char kDefaultFieldSeparator = ';';

inline std::string       kArticleIdSignature = kDefaultMsgIdSignature;
inline const std::string kRandomAlphabet     = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
inline const QString     kFileNameAlphabet = "._-ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
inline const QString     kPasswordAlphabet =
        "!#$%&()*+,-./:;=?@[]^_`{|}~ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

inline const QStringList kDefaultGroups = { "alt.binaries.test", "alt.binaries.misc" };

inline quint64       kArticleSize = kDefaultArticleSize;
inline const QString kSpace       = kDefaultSpace;

enum class Opt
{
    HELP = 0,
    LANG,
    VERSION,
    CONF,
    CONF_VERSION,
    SHUTDOWN_CMD,
    CHECK,
    QUIET,
    PROXY_SOCKS5,
    DISP_PROGRESS,
    DEBUG,
    DEBUG_FULL,
    POST_HISTORY,
    FIELD_SEPARATOR,
    NZB_RM_ACCENTS,
    RESUME_WAIT,
    NO_RESUME_AUTO,
    SOCK_TIMEOUT,
    PREPARE_PACKING,
    INPUT,
    OUTPUT,
    NZB_PATH,
    THREAD,
    NZB_UPLOAD_URL,
    NZB_POST_CMD,
    MONITOR_FOLDERS,
    MONITOR_EXT,
    MONITOR_IGNORE_DIR,
    MONITOR_SEC_DELAY_SCAN,
    MSG_ID,
    META,
    ARTICLE_SIZE,
    FROM,
    GROUPS,
    NB_RETRY,
    GEN_FROM,
    OBFUSCATE,
    INPUT_DIR,
    AUTO_DIR,
    MONITOR_DIR,
    DEL_AUTO,
    TMP_DIR,
    RAR_PATH,
    RAR_EXTRA,
    RAR_SIZE,
    RAR_MAX,
    KEEP_RAR,
#ifdef __USE_TMP_RAM__
    TMP_RAM,
    TMP_RAM_RATIO,
#endif
    PAR2_PCT,
    PAR2_PATH,
    PAR2_ARGS,
    PACK,
    COMPRESS,
    GEN_PAR2,
    GEN_NAME,
    GEN_PASS,
    LENGTH_NAME,
    LENGTH_PASS,
    RAR_NAME,
    RAR_PASS,
    RAR_NO_ROOT_FOLDER,
    AUTO_CLOSE_TABS,
    AUTO_COMPRESS,
    GROUP_POLICY,
    LOG_IN_FILE,
    SERVER,
    HOST,
    PORT,
    SSL,
    USER,
    PASS,
    CONNECTION,
    ENABLED,
    NZBCHECK
};

inline QMap<Opt, QString> const kOptionNames = {
    {Opt::PROXY_SOCKS5,            "proxy_socks5"          },
    { Opt::HELP,                   "help"                  },
    { Opt::LANG,                   "lang"                  },
    { Opt::VERSION,                "version"               },
    { Opt::CONF,                   "conf"                  },
    { Opt::CONF_VERSION,           "conf_version"          },
    { Opt::SHUTDOWN_CMD,           "shutdown_cmd"          },
    { Opt::DISP_PROGRESS,          "disp_progress"         },
    { Opt::DEBUG,                  "debug"                 },
    { Opt::DEBUG_FULL,             "fulldebug"             },
    { Opt::POST_HISTORY,           "post_history"          },
    { Opt::FIELD_SEPARATOR,        "field_separator"       },
    { Opt::NZB_UPLOAD_URL,         "nzb_upload_url"        },
    { Opt::NZB_POST_CMD,           "nzb_post_cmd"          },
    { Opt::NZB_RM_ACCENTS,         "nzb_rm_accents"        },
    { Opt::AUTO_CLOSE_TABS,        "auto_close_tabs"       },
    { Opt::RAR_NO_ROOT_FOLDER,     "rar_no_root_folder"    },
    { Opt::LOG_IN_FILE,            "log_in_file"           },

    { Opt::NO_RESUME_AUTO,         "no_resume_auto"        },
    { Opt::RESUME_WAIT,            "resume_wait"           },
    { Opt::SOCK_TIMEOUT,           "sock_timeout"          },
    { Opt::PREPARE_PACKING,        "prepare_packing"       },
    { Opt::CHECK,                  "check"                 },
    { Opt::QUIET,                  "quiet"                 },

    { Opt::INPUT,                  "input"                 },
    { Opt::AUTO_DIR,               "auto"                  },
    { Opt::MONITOR_DIR,            "monitor"               },
    { Opt::DEL_AUTO,               "rm_posted"             },
    { Opt::OUTPUT,                 "output"                },
    { Opt::NZB_PATH,               "nzbpath"               },
    { Opt::THREAD,                 "thread"                },

    { Opt::MONITOR_FOLDERS,        "monitor_nzb_folders"   },
    { Opt::MONITOR_EXT,            "monitor_extensions"    },
    { Opt::MONITOR_IGNORE_DIR,     "monitor_ignore_dir"    },
    { Opt::MONITOR_SEC_DELAY_SCAN, "monitor_sec_delay_scan"},

    { Opt::MSG_ID,                 "msg_id"                },
    { Opt::META,                   "meta"                  },
    { Opt::ARTICLE_SIZE,           "article_size"          },
    { Opt::FROM,                   "from"                  },
    { Opt::GROUPS,                 "groups"                },
    { Opt::NB_RETRY,               "retry"                 },
    { Opt::GEN_FROM,               "gen_from"              },

    { Opt::OBFUSCATE,              "obfuscate"             },
    { Opt::INPUT_DIR,              "inputdir"              },
    { Opt::GROUP_POLICY,           "group_policy"          },

    { Opt::TMP_DIR,                "tmp_dir"               },
    { Opt::RAR_PATH,               "rar_path"              },
    { Opt::RAR_EXTRA,              "rar_extra"             },
    { Opt::RAR_SIZE,               "rar_size"              },
    { Opt::RAR_MAX,                "rar_max"               },
    { Opt::KEEP_RAR,               "keep_rar"              },
#ifdef __USE_TMP_RAM__
    { Opt::TMP_RAM,                "tmp_ram"               },
    { Opt::TMP_RAM_RATIO,          "tmp_ram_ratio"         },
#endif

    { Opt::PAR2_PCT,               "par2_pct"              },
    { Opt::PAR2_PATH,              "par2_path"             },
    { Opt::PAR2_ARGS,              "par2_args"             },

    { Opt::AUTO_COMPRESS,          "auto_compress"         },
    { Opt::PACK,                   "pack"                  },
    { Opt::COMPRESS,               "compress"              },
    { Opt::GEN_PAR2,               "gen_par2"              },
    { Opt::GEN_NAME,               "gen_name"              },
    { Opt::GEN_PASS,               "gen_pass"              },
    { Opt::RAR_NAME,               "rar_name"              },
    { Opt::RAR_PASS,               "rar_pass"              },
    { Opt::LENGTH_NAME,            "length_name"           },
    { Opt::LENGTH_PASS,            "length_pass"           },

    { Opt::SERVER,                 "server"                },
    { Opt::HOST,                   "host"                  },
    { Opt::PORT,                   "port"                  },
    { Opt::SSL,                    "ssl"                   },
    { Opt::USER,                   "user"                  },
    { Opt::PASS,                   "pass"                  },
    { Opt::CONNECTION,             "connection"            },
    { Opt::ENABLED,                "enabled"               },
    { Opt::NZBCHECK,               "nzbcheck"              }
};

inline QList<QCommandLineOption> const kCmdOptions = {
    { kOptionNames[Opt::HELP], QCoreApplication::translate("NgPost", "Help: display syntax") },
    { { "v", kOptionNames[Opt::VERSION] }, QCoreApplication::translate("NgPost", "app version") },
    { { "c", kOptionNames[Opt::CONF] },
     QCoreApplication::translate("NgPost",
     "use configuration file (if not provided, we try to load $HOME/.ngPost)"),
     kOptionNames[Opt::CONF] },
    { kOptionNames[Opt::DISP_PROGRESS],
     QCoreApplication::translate("NgPost", "display cmd progressbar: NONE (default), BAR or FILES"),
     kOptionNames[Opt::DISP_PROGRESS] },
    { { "d", kOptionNames[Opt::DEBUG] }, QCoreApplication::translate("NgPost", "display extra information") },
    { kOptionNames[Opt::DEBUG_FULL], QCoreApplication::translate("NgPost", "display full debug information") },
    { { "l", kOptionNames[Opt::LANG] },
     QCoreApplication::translate("NgPost", "application language"),
     kOptionNames[Opt::LANG] },

    { kOptionNames[Opt::CHECK],
     QCoreApplication::translate(
              "NgPost", "check nzb file (if articles are available on Usenet) cf https://github.com/mbruel/nzbCheck"),
     kOptionNames[Opt::CHECK] },
    { { "q", kOptionNames[Opt::QUIET] },
     QCoreApplication::translate("NgPost", "quiet mode (no output on stdout)") },

 // automated posting (scanning and/or monitoring)
    { kOptionNames[Opt::AUTO_DIR],
     QCoreApplication::translate(
              "NgPost", "parse directory and post every file/folder separately. You must use --compress, "
              "should add --gen_par2, --gen_name and --gen_pass"),
     kOptionNames[Opt::AUTO_DIR] },
    { kOptionNames[Opt::MONITOR_DIR],
     QCoreApplication::translate(
              "NgPost", "monitor directory and post every new file/folder. You must use --compress, should "
              "add --gen_par2, --gen_name and --gen_pass"),
     kOptionNames[Opt::MONITOR_DIR] },
    { kOptionNames[Opt::DEL_AUTO],
     QCoreApplication::translate(
              "NgPost", "delete file/folder once posted. You must use --auto or --monitor with this option.") },

 // quick posting (several files/folders)
    { { "i", kOptionNames[Opt::INPUT] },
     QCoreApplication::translate(
              "NgPost", "input file to upload (single file or directory), you can use it multiple times"),
     kOptionNames[Opt::INPUT] },
    { { "o", kOptionNames[Opt::OUTPUT] },
     QCoreApplication::translate("NgPost", "output file path (nzb)"),
     kOptionNames[Opt::OUTPUT] },

 // general options
    { { "x", kOptionNames[Opt::OBFUSCATE] },
     QCoreApplication::translate("NgPost",
     "obfuscate the subjects of the articles (CAREFUL you won't find your post "
                                  "if you lose the nzb file)") },
    { { "g", kOptionNames[Opt::GROUPS] },
     QCoreApplication::translate("NgPost", "newsgroups where to post the files (coma separated without space)"),
     kOptionNames[Opt::GROUPS] },
    { { "m", kOptionNames[Opt::META] },
     QCoreApplication::translate("NgPost", "extra meta data in header (typically \"password=qwerty42\")"),
     kOptionNames[Opt::META] },
    { { "f", kOptionNames[Opt::FROM] },
     QCoreApplication::translate("NgPost", "poster email (random one if not provided)"),
     kOptionNames[Opt::FROM] },
    { { "a", kOptionNames[Opt::ARTICLE_SIZE] },
     QCoreApplication::translate("NgPost", "article size (default one: %1)").arg(kDefaultArticleSize),
     kOptionNames[Opt::ARTICLE_SIZE] },
    { { "z", kOptionNames[Opt::MSG_ID] },
     QCoreApplication::translate("NgPost", "msg id signature, after the @ (default one: ngPost)"),
     kOptionNames[Opt::MSG_ID] },
    { { "r", kOptionNames[Opt::NB_RETRY] },
     QCoreApplication::translate("NgPost", "number of time we retry to an Article that failed (default: 5)"),
     kOptionNames[Opt::NB_RETRY] },
    { { "t", kOptionNames[Opt::THREAD] },
     QCoreApplication::translate("NgPost",
     "number of Threads (the connections will be distributed amongs them)"),
     kOptionNames[Opt::THREAD] },
    { kOptionNames[Opt::GEN_FROM],
     QCoreApplication::translate("NgPost", "generate a new random email for each Post (--auto or --monitor)") },

 // for compression and par2 support
    { kOptionNames[Opt::TMP_DIR],
     QCoreApplication::translate("NgPost",
     "temporary folder where the compressed files and par2 will be stored"),
     kOptionNames[Opt::TMP_DIR] },
    { kOptionNames[Opt::RAR_PATH],
     QCoreApplication::translate("NgPost", "RAR absolute file path (external application)"),
     kOptionNames[Opt::RAR_PATH] },
    { kOptionNames[Opt::RAR_SIZE],
     QCoreApplication::translate("NgPost", "size in MB of the RAR volumes (0 by default meaning NO split)"),
     kOptionNames[Opt::RAR_SIZE] },
    { kOptionNames[Opt::RAR_MAX],
     QCoreApplication::translate("NgPost", "maximum number of archive volumes"),
     kOptionNames[Opt::RAR_MAX] },
    { kOptionNames[Opt::PAR2_PCT],
     QCoreApplication::translate("NgPost",
     "par2 redundancy percentage (0 by default meaning NO par2 generation)"),
     kOptionNames[Opt::PAR2_PCT] },
    { kOptionNames[Opt::PAR2_PATH],
     QCoreApplication::translate("NgPost", "par2 absolute file path (in case of self compilation of ngPost)"),
     kOptionNames[Opt::PAR2_PCT] },

    { kOptionNames[Opt::PACK],
     QCoreApplication::translate("NgPost",
     "Pack posts using config PACK definition with a subset of (COMPRESS, "
                                  "GEN_NAME, GEN_PASS, GEN_PAR2)") },
    { kOptionNames[Opt::COMPRESS], QCoreApplication::translate("NgPost", "compress inputs using RAR or 7z") },
    { kOptionNames[Opt::GEN_PAR2],
     QCoreApplication::translate("NgPost", "generate par2 (to be used with --compress)") },
    { kOptionNames[Opt::RAR_NAME],
     QCoreApplication::translate("NgPost", "provide the RAR file name (to be used with --compress)"),
     kOptionNames[Opt::RAR_NAME] },
    { kOptionNames[Opt::RAR_PASS],
     QCoreApplication::translate("NgPost", "provide the RAR password (to be used with --compress)"),
     kOptionNames[Opt::RAR_PASS] },
    { kOptionNames[Opt::GEN_NAME],
     QCoreApplication::translate("NgPost", "generate random RAR name (to be used with --compress)") },
    { kOptionNames[Opt::GEN_PASS],
     QCoreApplication::translate("NgPost", "generate random RAR password (to be used with --compress)") },
    { kOptionNames[Opt::LENGTH_NAME],
     QCoreApplication::translate("NgPost",
     "length of the random RAR name (to be used with --gen_name), default: 17"),
     kOptionNames[Opt::LENGTH_NAME] },
    { kOptionNames[Opt::LENGTH_PASS],
     QCoreApplication::translate("NgPost",
     "length of the random RAR password (to be used with --gen_pass), default: 13"),
     kOptionNames[Opt::LENGTH_PASS] },
    { kOptionNames[Opt::RAR_NO_ROOT_FOLDER],
     QCoreApplication::translate("NgPost", "Remove root (parent) folder when compressing Folders using RAR") },

    { { "S", kOptionNames[Opt::SERVER] },
     QCoreApplication::translate(
              "NgPost", "NNTP server following the format (<user>:<pass>@@@)?<host>:<port>:<nbCons>:(no)?ssl"),
     kOptionNames[Opt::SERVER] },
 // without config file, you can provide all the parameters to connect to ONE SINGLE server
    { { "h", kOptionNames[Opt::HOST] },
     QCoreApplication::translate("NgPost", "NNTP server hostname (or IP)"),
     kOptionNames[Opt::HOST] },
    { { "P", kOptionNames[Opt::PORT] },
     QCoreApplication::translate("NgPost", "NNTP server port"),
     kOptionNames[Opt::PORT] },
    { { "s", kOptionNames[Opt::SSL] }, QCoreApplication::translate("NgPost", "use SSL") },
    { { "u", kOptionNames[Opt::USER] },
     QCoreApplication::translate("NgPost", "NNTP server username"),
     kOptionNames[Opt::USER] },
    { { "p", kOptionNames[Opt::PASS] },
     QCoreApplication::translate("NgPost", "NNTP server password"),
     kOptionNames[Opt::PASS] },
    { { "n", kOptionNames[Opt::CONNECTION] },
     QCoreApplication::translate("NgPost", "number of NNTP connections"),
     kOptionNames[Opt::CONNECTION] },
};

enum class PostCmdPlaceHolders
{
    nzbPath,
    nzbName,
    rarName,
    rarPass,
    groups,
    nbArticles,
    nbArticlesFailed,
    sizeInByte,
    nbFiles
};
inline QMap<PostCmdPlaceHolders, QString> const kPostCmdPlaceHolders = {
    {PostCmdPlaceHolders::nzbPath,           "__nzbPath__"         },
    { PostCmdPlaceHolders::nzbName,          "__nzbName__"         },
    { PostCmdPlaceHolders::rarName,          "__rarName__"         },
    { PostCmdPlaceHolders::rarPass,          "__rarPass__"         },
    { PostCmdPlaceHolders::nbArticles,       "__nbArticles__"      },
    { PostCmdPlaceHolders::nbArticlesFailed, "__nbArticlesFailed__"},
    { PostCmdPlaceHolders::sizeInByte,       "__sizeInByte__"      },
    { PostCmdPlaceHolders::nbFiles,          "__nbFiles__"         },
    { PostCmdPlaceHolders::groups,           "__groups__"          }
};

enum class DISP_PROGRESS
{
    NONE = 0,
    BAR,
    FILES
};
inline QMap<DISP_PROGRESS, QString> const kDisplayProgress = {
    {DISP_PROGRESS::NONE,   "none" },
    { DISP_PROGRESS::BAR,   "bar"  },
    { DISP_PROGRESS::FILES, "files"}
};

enum class GROUP_POLICY
{
    ALL = 0,
    EACH_POST,
    EACH_FILE
};
inline QMap<GROUP_POLICY, QString> const kGroupPolicies = {
    {GROUP_POLICY::ALL,        "all"      },
    { GROUP_POLICY::EACH_POST, "each_post"},
    { GROUP_POLICY::EACH_FILE, "each_file"}
};

inline const QString kNgPostASCII = QString("\
                   __________               __\n\
       ____    ____\\______   \\____  _______/  |_\n\
      /    \\  / ___\\|     ___/  _ \\/  ___/\\   __\\\n\
     |   |  \\/ /_/  >    |  (  <_> )___ \\  |  |\n\
     |___|  /\\___  /|____|   \\____/____  > |__|\n\
          \\//_____/                    \\/\n\
");

} // namespace NgConf

#endif // NGCONF_H
