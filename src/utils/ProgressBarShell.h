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

    ushort nbDigitsEndBar() const
    { // The loop method should be the fastest than the 2 others in comment
        //  return  static_cast<int>(std::log10(end)) + 1.0;
        //  return QString::number(end).length();
        uint   num      = end;
        ushort nbDigits = 0;
        while (num != 0)
        {
            num /= 10;
            ++nbDigits;
        }
        return nbDigits;
    };
};

using ProgressCallback = std::function<void(UpdateBarInfo &currentPos)>;

class ShellBar : public QObject
{
    Q_OBJECT

    static constexpr ushort kProgressBarWidth   = 50;
    static constexpr int    kDefaultRefreshRate = 500; //!< how often shall we refresh the progressbar?

    inline static const QString kColorBar      = "\x1B[1;30;46m"; //!< bold black with magenta background
    inline static const QString kColorText     = "\x1B[1;30";     //!< bold black
    inline static const QString kColorReset    = "\x1B[0m";       //!< reset all text attributes
    inline static const QChar   kPaddingNumber = QChar(' ');

private slots:
    void onRefresh();

private:
    ProgressCallback _progressCallback;
    UpdateBarInfo    _currentPos;

    QTextStream _cout; //!< stream for stdout

    ushort _barWidth;
    int    _refreshRateMs;

    QTimer _progressTimer; //!< timer to refresh the refresh bar

#ifdef __PROGRESS_BAR_HOOKED_BY_LOGGER__
    bool _waitEventLoopStarted;
#endif

public:
    ShellBar(ProgressCallback const &progressCallback,
             ushort                  barWidth      = kProgressBarWidth,
             int                     refreshRateMs = kDefaultRefreshRate,
             QObject                *parent        = nullptr);
    ~ShellBar();

    void setProgressCallback(ProgressCallback const &progressCallback);

#ifdef __PROGRESS_BAR_HOOKED_BY_LOGGER__
    void start(bool waitEventLoopStarted);
#else
    void start();
#endif

    void stop(bool lastRefresh);
};

} // namespace ProgressBar
#endif // PROGRESSBARSHELL_H
