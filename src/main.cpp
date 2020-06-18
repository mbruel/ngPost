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
