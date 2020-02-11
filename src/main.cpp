#include <csignal>
#include <iostream>
#include <QCoreApplication>
#include <QLoggingCategory>
#include "NgPost.h"

#if defined( Q_OS_WIN )
#include <windows.h>
#endif
void handleShutdown(int signal)
{
    Q_UNUSED(signal);
    std::cout << "Closing the application...\n";
    std::cout.flush();
    qApp->quit();
}

int main(int argc, char *argv[])
{
    // disable SSL warnings
    QLoggingCategory::setFilterRules("qt.network.ssl.warning=false");

    signal(SIGINT,  &handleShutdown);// shut down on ctrl-c
    signal(SIGTERM, &handleShutdown);// shut down on killall

//    qDebug() << "argc: " << argc;
    NgPost ngPost(argc, argv);
    ngPost.checkForNewVersion();

    if (ngPost.useHMI())
    {
#if defined( Q_OS_WIN )
    ::ShowWindow( ::GetConsoleWindow(), SW_HIDE ); //hide console window
#endif
        return ngPost.startHMI();
    }
    else if (ngPost.parseCommandLine(argc, argv))
    {
        ngPost.startEventLoop();
        qCritical()<< "ngPost closed propertly!\n";
        return 0;
    }
    else
    {
#ifdef __DEBUG__
        std::cout << "Nothing to do...\n";
        std::cout.flush();
#endif
        return 0;
    }
}
