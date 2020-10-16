#include "NgPost.h"
#include <csignal>
#include <iostream>
#include <QCoreApplication>
#include <QLoggingCategory>

#if defined( Q_OS_WIN )
#include <windows.h>
#endif

#ifdef __linux__
void handleSigUsr(int signal);
#endif
void handleShutdown(int signal);

static NgPost *app = nullptr;


#if defined(__USE_TMP_RAM__) && defined(__DEBUG__)
#include "PostingJob.h"
void dispFolderSize(const QFileInfo &folderPath)
{
    qint64 size = NgPost::recursiveSize(folderPath);
    qDebug() << "size " << folderPath.absoluteFilePath()
             << " : " << PostingJob::humanSize(static_cast<double>(size))
             << " (" << size << ")";
}
#endif

int main(int argc, char *argv[])
{
    // disable SSL warnings
    QLoggingCategory::setFilterRules("qt.network.ssl.warning=false");

    signal(SIGINT,  &handleShutdown);// shut down on ctrl-c
    signal(SIGTERM, &handleShutdown);// shut down on killall
#ifdef __linux__
    signal(SIGUSR1, &handleSigUsr); // kill -s SIGUSR1 $(pidof ngPost) to hide/show the GUI
#endif

    //    qDebug() << "argc: " << argc;
    app = new NgPost(argc, argv);
    app->checkForNewVersion();

    int exitCode = 0;
    if (app->useHMI())
    {
#if defined( Q_OS_WIN )
        ::ShowWindow( ::GetConsoleWindow(), SW_HIDE ); //hide console window
#endif
        exitCode = app->startHMI();
    }
    else if (app->parseCommandLine(argc, argv))
    {
        exitCode = app->startEventLoop();

        if (app->nzbCheck())
            exitCode = app->nbMissingArticles();

#ifdef __DEBUG__
        std::cout << app->appName() << " closed properly!\n";
        std::cout.flush();
#endif
    }
    else
    {
#ifdef __DEBUG__
        std::cout << "Nothing to do...\n";
        std::cout.flush();
#endif
    }

    if (app->errCode() != 0)
        exitCode = app->errCode();

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
    app->hideOrShowGUI();
}
#endif
