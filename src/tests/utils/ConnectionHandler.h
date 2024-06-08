#ifndef CONNECTIONHANDLER_H
#define CONNECTIONHANDLER_H
#include <QThread>
class NntpConnection;
class NgPost;
class ConnectionHandler : public QObject
{
    Q_OBJECT
signals:
    void sendTestArticleInGoodThread();

private:
    QThread         _thread;
    NntpConnection *_nntpCon;
    bool            _testDone;
    bool            _isAutenticated;
    bool            _testPosting;

public:
    ConnectionHandler(NgPost const &ngPost, bool testPosting, QObject *parent = nullptr);
    ~ConnectionHandler();

    void start();                                           //!< start the thread and thus the nntp connection
    bool isTestDone() const { return _testDone; }           //!< functor for QTest::qWaitFor to stop the test
    bool isAutenticated() const { return _isAutenticated; } //!< functor for QTest::qWaitFor to stop the test

    NntpConnection *nntpConnection() const { return _nntpCon; }

public slots:
    void onNntpConDisconnected(); //!< we stop the test
    void onNntpConnected();       //!< some happy log
    void onNntpHelloReceived();   //!< some happy log
    void onNntpAuthenticated();   //!< some happy log
    void onPostingNotAllowed();
    void onStopTest();

private:
    void onSendTestArticleInGoodThread();
};

#endif // CONNECTIONHANDLER_H
