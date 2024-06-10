//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
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

#include <csignal>
#include <iostream>
#include <QCoreApplication>
#include <QLoggingCategory>

#if defined(__USE_HMI__) && defined(Q_OS_WIN)
#  include <windows.h>
#endif

#include "NgPost.h"
#include "utils/NgLogger.h"
#include "utils/NgTools.h"

#ifdef __linux__
void handleSigUsr(int signal);
#endif
void handleShutdown(int signal);

static NgPost *app = nullptr;

#if defined(__USE_TMP_RAM__) && defined(__DEBUG__)
void dispFolderSize(QFileInfo const &folderPath)
{
    qint64 size = NgTools::recursivePathSize(folderPath);
    qDebug() << "size " << folderPath.absoluteFilePath() << " : "
             << NgTools::humanSize(static_cast<double>(size)) << " (" << size << ")";
}
#endif

#include "utils/Macros.h"
int main(int argc, char *argv[])
{
#ifdef __DEBUG__
    std::cout << "C++ compiler version: " << TOSTRING(__cplusplus) << std::endl;
#endif
    // disable SSL warnings
    QLoggingCategory::setFilterRules("qt.network.ssl.warning=false");

    signal(SIGINT, &handleShutdown);  // shut down on ctrl-c
    signal(SIGTERM, &handleShutdown); // shut down on killall
#ifdef __linux__
    signal(SIGUSR1, &handleSigUsr); // kill -s SIGUSR1 $(pidof ngPost) to hide/show the GUI
#endif

    // Initialigze the logger!
    NgLogger::createInstance();
    NgLogger::setDebug(NgLogger::DebugLevel::None);

    //    qDebug() << "argc: " << argc;
    app = new NgPost(argc, argv);
    app->checkForNewVersion();

    int exitCode = 0;
#ifdef __USE_HMI__
    if (app->useHMI())
    {
#  if defined(Q_OS_WIN)
        ::ShowWindow(::GetConsoleWindow(), SW_HIDE); // hide console window
#  endif
        app->checkSupportSSL();
        exitCode = app->startHMI();
    }
    else if (app->parseCommandLine(argc, argv))
#else
    if (app->parseCommandLine(argc, argv))
#endif
    {
        //        app->checkForMigration();
        if (app->checkSupportSSL())
        {
            if (app->initHistoryDatabase())
            {
                exitCode = app->startEventLoop();
                if (app->nzbCheck())
                {
                    exitCode = app->nbMissingArticles();
                    qApp->processEvents(); // to see the last summary log (in queue...)
                }
            }
            else
            {
                std::cerr << "Error initializing history database...";
                // Process events without starting the event loop to print the logs/errors
                qApp->processEvents();
            }
        }
#ifdef __DEBUG__
        std::cout << app->appName() << " closed properly!\n";
        std::cout.flush();
#endif
    }
    else
    {
        // Process events without starting the event loop to print the logs/errors
        qApp->processEvents();

#ifdef __DEBUG__
        std::cout << "Nothing to do...\n";
        std::cout.flush();
#endif
    }

    if (NgError::errCode() != NgError::ERR_CODE::NONE)
        exitCode = static_cast<int>(NgError::errCode());

    delete app;
    return exitCode;
}

void handleShutdown(int signal)
{
    Q_UNUSED(signal)
    std::cout << "Closing the application...\n";
    std::cout.flush();
    qApp->quit();
}

#ifdef __linux__
void handleSigUsr(int signal)
{
    Q_UNUSED(signal)
    std::cout << "intercept SIGUSR1 :)\n";
    std::cout.flush();
#  ifdef __USE_HMI__
    app->hideOrShowGUI();
#  endif
}
#endif
