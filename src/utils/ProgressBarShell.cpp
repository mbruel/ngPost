#include "ProgressBarShell.h"

#include <cmath> //std::floor

namespace ProgressBar
{

ShellBar::ShellBar(ProgressCallback const &progressCallback, ushort barWidth, int refreshRateMs, QObject *parent)
    : QObject(parent)
    , _progressCallback(progressCallback)
    , _cout(stdout)
    , _barWidth(barWidth)
    , _refreshRateMs(refreshRateMs)
{
}

ShellBar::~ShellBar()
{
    if (_progressTimer.isActive())
        _progressTimer.stop();
}

void ShellBar::start()
{
    connect(&_progressTimer, &QTimer::timeout, this, &ShellBar::onRefresh, Qt::DirectConnection);
    onRefresh();
    _progressTimer.start(_refreshRateMs);
}

void ShellBar::stop(bool lastRefresh)
{
    if (_progressTimer.isActive())
    {
        _progressTimer.stop();
        disconnect(&_progressTimer, &QTimer::timeout, this, &ShellBar::onRefresh);
    }
    if (lastRefresh)
        onRefresh();
    _cout << Qt::endl << Qt::endl << Qt::flush;
}

void ShellBar::onRefresh()
{
    // update current position
    _progressCallback(_currentPos);

    float progress = static_cast<float>(_currentPos.pos);
    progress /= _currentPos.end;

    _cout << "\r[";
    ushort positionInBar = static_cast<ushort>(std::floor(progress * kProgressBarWidth));
    for (ushort i = 0; i < kProgressBarWidth; ++i)
    {
        if (i < positionInBar)
            _cout << "=";
        else if (i == positionInBar)
            _cout << ">";
        else
            _cout << " ";
    }
    _cout << "] " << static_cast<int>(progress) * 100 << " %"
          << " (" << _currentPos.pos << " / " << _currentPos.end << ") " << _currentPos.msg << Qt::flush;

    if (_currentPos.pos < _currentPos.end)
        _progressTimer.start(_refreshRateMs);
}

} // namespace ProgressBar
