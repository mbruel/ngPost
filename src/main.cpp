#include <QCoreApplication>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <csignal>
#include <iostream>
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
    {
        // disable SSL warnings
        QLoggingCategory::setFilterRules("qt.network.ssl.warning=false");

        // to avoid the warning : "Type conversion already registered from type QSharedPointer<QNetworkSession> to type QObject*"
        // we instanciate and destruct the QNetworkAccessManager in the main Thread
        QNetworkAccessManager tmp;
    }

    signal(SIGINT,  &handleShutdown);// shut down on ctrl-c
    signal(SIGTERM, &handleShutdown);// shut down on killall

//    qDebug() << "argc: " << argc;
    NgPost ngPost(argc, argv);
    if (ngPost.useHMI())
    {
#if defined( Q_OS_WIN )
    ::ShowWindow( ::GetConsoleWindow(), SW_HIDE ); //hide console window
#endif
        return ngPost.startHMI();
    }
    else if (ngPost.parseCommandLine(argc, argv))
    {
        ngPost.startPosting();
        return ngPost.startEventLoop();
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
