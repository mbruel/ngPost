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

inline const QString kThreadNameMain    = "MainThread";
inline const QString kThreadNameMonitor = "MonitorThread";
inline const QString kThreadNameBuilder = "BuilderThread #%1";
inline const QString kThreadNameNntp    = "NntpThread #%1";

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

constexpr int kProgressBarWidth   = 50;
constexpr int kDefaultRefreshRate = 500; //!< how often shall we refresh the progressbar bar?
#ifdef __COMPUTE_IMMEDIATE_SPEED__
constexpr int kImmediateSpeedDurationMs = 3000;
#endif

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
inline const QString     kFileNameAlphabet = "_-ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
inline const QString     kPasswordAlphabet =
        "$%;,+-=[^_`]~@!#ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

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
