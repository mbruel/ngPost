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
#ifndef NGCMDLINELOADER_H
#define NGCMDLINELOADER_H

#include "utils/PureStaticClass.h"
#include <QCoreApplication>
#include <QStringList>
class NgPost;
class QCommandLineParser;
#include "PostingParams.h"

class NgCmdLineLoader : public PureStaticClass
{
    Q_DECLARE_TR_FUNCTIONS(NgCmdLineLoader); // tr() without QObject using QCoreApplication::translate

    static QList<QCommandLineOption> const kCmdOptions;
    static constexpr char const           *kNntpServerStrRegExp =
            "^(([^:]+):([^@]+)@@@)?([\\w\\.\\-_]+):(\\d+):(\\d+):(no)?ssl$";

public:
    /*!
     * \brief load command line many potential arguments (cf
     * \param full path of the configuration file
     * \param SharedParams that will be overwritten (previous unset parameters are kept)
     * \return list of errors (empty if all ok)
     */
    static bool loadCmdLine(char *appName, NgPost &ngPost, SharedParams &postingParams);

private:
    static void syntax(NgPost const &ngPost, char *appName);

    static void prepareAndStartPostingSingleFiles(QString                  &rarName,
                                                  QString                  &rarPass,
                                                  std::string const        &from,
                                                  QList<QFileInfo>         &filesToUpload,
                                                  QCommandLineParser const &parser,
                                                  NgPost                   &ngPost,
                                                  SharedParams             &postingParams);

    static bool getInputFilesToGroupPost(QList<QFileInfo>         &filesToUpload,
                                         QCommandLineParser const &parser,
                                         SharedParams             &postingParams);

    static bool startFoldersMonitoring(bool                     &isMonitoring,
                                       QCommandLineParser const &parser,
                                       NgPost                   &ngPost,
                                       SharedParams             &postingParams);

    static bool
    getAutoDirectories(QList<QDir> &autoDirs, QCommandLineParser const &parser, SharedParams &postingParams);

    static bool loadServersParameters(QCommandLineParser const &parser, SharedParams &postingParams);

    static bool loadPackingParameters(QCommandLineParser const &parser, SharedParams &postingParams);

    static bool loadPostingParameters(QCommandLineParser const &parser, SharedParams &postingParams);
};

#endif // NGCMDLINELOADER_H
