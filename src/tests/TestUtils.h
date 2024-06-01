#ifndef TESTUTILS_H
#define TESTUTILS_H
#include "utils/PureStaticClass.h"
#include <QObject>
#include <QString>
#include <QThread>

class NgPost;
class NntpConnection;

class ConnectionHandler : public QObject
{
    Q_OBJECT
private:
    QThread         _thread;
    NntpConnection *_nntpCon;
    bool            _testDone;

public:
    ConnectionHandler(NgPost const &ngPost, QObject *parent = nullptr);
    ~ConnectionHandler();

    void start();                           //!< start the thread and thus the nntp connection
    bool isTestDone() { return _testDone; } //!< functor for QTest::qWaitFor to stop the test

public slots:
    void onNntpConDisconnected(); //!< we stop the test
    void onNntpConnected();       //!< some happy log
    void onNntpHelloReceived();   //!< some happy log
    void onNntpAuthenticated();   //!< some happy log
    void onStopTest();
};

class TestUtils : public PureStaticClass
{
    inline static const QString     xsnewsConfig = "://config/ngPost.xsnews.partner";
    inline static const QString     xsnewsAddr   = "reader.xsnews.nl";
    static constexpr ushort         xsnewsPort   = 443;
    inline static const std::string xsnewsUser   = "xsn_66598d1cc90f9";

    static ConnectionHandler *sConnectionHandler;

public:
    static QString const &partnerConfig() { return xsnewsConfig; }

    static void               loadXSNewsPartnerConf(NgPost &ngPost);
    static ConnectionHandler *loadXSNewsPartnerConfAndCheckConnection(NgPost &ngPost);
};

#endif // TESTUTILS_H
