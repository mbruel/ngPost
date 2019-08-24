#include <QCoreApplication>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <csignal>
#include <iostream>
#include "NgPost.h"

void handleShutdown(int signal)
{
    Q_UNUSED(signal);
    std::cout << "Closing the application...\n";
    std::cout.flush();
    qApp->quit();
}


int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    {
        // disable SSL warnings
        QLoggingCategory::setFilterRules("qt.network.ssl.warning=false");

        // to avoid the warning : "Type conversion already registered from type QSharedPointer<QNetworkSession> to type QObject*"
        // we instanciate and destruct the QNetworkAccessManager in the main Thread
        QNetworkAccessManager tmp;
    }

    signal(SIGINT,  &handleShutdown);// shut down on ctrl-c
    signal(SIGTERM, &handleShutdown);// shut down on killall

    NgPost ngPost;
    if (ngPost.parseCommandLine(argc, argv))
    {
        ngPost.startPosting();
        return a.exec();;
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
