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

#include <QNetworkReply>
int main(int argc, char *argv[])
{
    // disable SSL warnings
    QLoggingCategory::setFilterRules("qt.network.ssl.warning=false");

    QNetworkAccessManager netMgr;
    QUrl proFileURL(NgPost::proFileUrl());
    QNetworkRequest req(proFileURL);
    req.setRawHeader( "User-Agent" , "ngPost C++ app" );

    signal(SIGINT,  &handleShutdown);// shut down on ctrl-c
    signal(SIGTERM, &handleShutdown);// shut down on killall

//    qDebug() << "argc: " << argc;
    NgPost ngPost(argc, argv);

    QNetworkReply *reply = netMgr.get(req);
    QObject::connect(reply, &QNetworkReply::finished, &ngPost, &NgPost::onCheckForNewVersion);

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
