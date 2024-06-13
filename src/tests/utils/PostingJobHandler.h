#ifndef POSTINGJOBHANDLER_H
#define POSTINGJOBHANDLER_H
#include <QThread>
class PostingJob;
class NgPost;
class PostingJobHandler : public QObject
{
    Q_OBJECT
signals:
    void sendTestArticleInGoodThread();

private:
    QThread     _thread;
    PostingJob *_job;
    bool        _testDone;
    bool        _isAutenticated;
    bool        _testPosting;

public:
    PostingJobHandler(PostingJob *job, QObject *parent = nullptr);
    ~PostingJobHandler();

    void start();                                           //!< start the thread and thus the nntp connection
    bool isTestDone() const { return _testDone; }           //!< functor for QTest::qWaitFor to stop the test
    bool isAutenticated() const { return _isAutenticated; } //!< functor for QTest::qWaitFor to stop the test

    PostingJob *postingJob() const { return _job; }
    QThread    *thread() { return &_thread; }

public slots:
    void onPostingStarted();  //!< we stop the test
    void onPostingFinished(); //!< some happy log
};

#endif // POSTINGJOBHANDLER_H
