#include "ProgressBarShell.h"

#include <cmath> //std::floor

#ifdef __PROGRESS_BAR_HOOKED_BY_LOGGER__
#  include <QCoreApplication>
#  include <QThread>
#endif

namespace ProgressBar
{

ShellBar::ShellBar(ProgressCallback const &progressCallback, uint barWidth, int refreshRateMs, QObject *parent)
    : QObject(parent)
    , _progressCallback(progressCallback)
    , _cout(stdout)
    , _barWidth(barWidth)
    , _refreshRateMs(refreshRateMs)
#ifdef __PROGRESS_BAR_HOOKED_BY_LOGGER__
    , _waitEventLoopStarted(false)
#endif

{
}

ShellBar::~ShellBar()
{
    if (_progressTimer.isActive())
        _progressTimer.stop();
}

void ShellBar::setProgressCallback(ProgressCallback const &progressCallback)
{
    stop(false);
    _currentPos       = { 0, 0, QString() };
    _progressCallback = progressCallback;
}

#ifdef __PROGRESS_BAR_HOOKED_BY_LOGGER__
void ShellBar::start(bool waitEventLoopStarted)
{
    _waitEventLoopStarted = waitEventLoopStarted;
    // QueuedConnection to make sure we won't print the bar before
    // the queued log events that where stored before the event loop was started
    connect(&_progressTimer, &QTimer::timeout, this, &ShellBar::onRefresh, Qt::QueuedConnection);
#else
void ShellBar::start()
{
    connect(&_progressTimer, &QTimer::timeout, this, &ShellBar::onRefresh, Qt::DirectConnection);
    onRefresh();
#endif
    _progressTimer.start(_refreshRateMs);
}

void ShellBar::clear() { setProgressCallback(nullptr); }

void ShellBar::stop(bool lastRefresh)
{
    disconnect(&_progressTimer, &QTimer::timeout, this, &ShellBar::onRefresh);
    if (_progressTimer.isActive())
        _progressTimer.stop();
    if (lastRefresh)
    {
        onRefresh();
        _cout << Qt::endl << Qt::endl << Qt::flush;
    }
}

void ShellBar::onRefresh()
{
#ifdef __PROGRESS_BAR_HOOKED_BY_LOGGER__
    if (_waitEventLoopStarted && !QThread::currentThread()->isRunning())
        return; // no display while the
    else
    {
        qApp->processEvents();         // write first the logs in event queue before it was started
        _waitEventLoopStarted = false; // no need to check if the event loop is running anymore
    }
#endif

    // update current position
    _progressCallback(_currentPos);

    float progress = static_cast<float>(_currentPos.pos);
    progress /= _currentPos.end;

    _cout << kColorBar << "\r[";
    uint positionInBar = static_cast<uint>(std::floor(progress * kProgressBarWidth));
    for (uint i = 0; i < kProgressBarWidth; ++i)
    {
        if (i < positionInBar)
            _cout << "=";
        else if (i == positionInBar)
            _cout << ">";
        else
            _cout << " ";
    }
    _cout << "]" << kColorReset
          << QString(" %1 % (%2 / %3) ")
                     .arg(static_cast<int>(progress * 100), 3, 10, kPaddingNumber)
                     .arg(_currentPos.pos, _currentPos.nbDigitsEndBar(), 10, kPaddingNumber)
                     .arg(_currentPos.end)
          << _currentPos.msg << Qt::flush;

    if (_currentPos.pos < _currentPos.end)
        _progressTimer.start(_refreshRateMs);
}

} // namespace ProgressBar