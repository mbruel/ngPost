#ifndef PROGRESSBARSHELL_H
#define PROGRESSBARSHELL_H

#include <functional>
#include <QObject>
#include <QTextStream>
#include <QTimer>

namespace ProgressBar
{

struct UpdateBarInfo
{
    uint    pos; //!< actual position
    uint    end; //!< end of bar (100%) that could change... fixed most of the time
    QString msg; //!< optional message written after the bar

    UpdateBarInfo() : pos(0), end(0) { }
    UpdateBarInfo(uint p, uint e, QString const &m) : pos(p), end(e), msg(m) { }

    void update(uint p, uint e, QString const &m)
    {
        pos = p;
        end = e;
        msg = m;
    }
};

using ProgressCallback = std::function<void(UpdateBarInfo &currentPos)>;

class ShellBar : public QObject
{
    Q_OBJECT

    static constexpr ushort kProgressBarWidth   = 50;
    static constexpr int    kDefaultRefreshRate = 500; //!< how often shall we refresh the progressbar?

private slots:
    void onRefresh();

private:
    ProgressCallback _progressCallback;
    UpdateBarInfo    _currentPos;

    QTextStream _cout; //!< stream for stdout

    ushort _barWidth;
    int    _refreshRateMs;

    QTimer _progressTimer; //!< timer to refresh the refresh bar

public:
    ShellBar(ProgressCallback const &progressCallback,
             ushort                  barWidth      = kProgressBarWidth,
             int                     refreshRateMs = kDefaultRefreshRate,
             QObject                *parent        = nullptr);
    ~ShellBar();

    void start();
    void stop(bool lastRefresh = false);
};

} // namespace ProgressBar
#endif // PROGRESSBARSHELL_H
