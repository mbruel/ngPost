//========================================================================
//
// Copyright (C) 2024 Matthieu Bruel <Matthieu.Bruel@gmail.com>
// This file is a part of ngPost : https://github.com/mbruel/ngPost
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3..
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>
//
//========================================================================
#ifndef NGLOGGER_H
#define NGLOGGER_H

#include "Singleton.h"
#include <QObject>
#include <QTextStream>

#include "NgError.h"
#include "ProgressBarShell.h"

class QFile;
#ifdef __USE_HMI__
class MainWindow;
#endif

/**
 * @brief Thread-Safe Qt Logging Class that should be used as a static class
 *    - it has NO non-static public methods
 *    - instance() from Singleton not available to users due to private inheritance
 *    - signals can't be used by users
 *
 * Thread safety is guaranteed by the type of signals/slots connection: Qt::QueuedConnection
 * All logs will happen in the thread where the NgLogger instance is created
 * All logs will be done sequentially in its event loop
 */
class NgLogger : public QObject, private Singleton<NgLogger>
{
    Q_OBJECT
    friend class Singleton<NgLogger>;

    /*!
     * \brief ANSI/VT100 Terminal Control Escape Sequences
     * cf https://www2.ccs.neu.edu/research/gpc/VonaUtils/vona/terminal/vtansi.htm
     */
    inline static const QString kColorReset     = "\x1B[0m";    //!< reset all text attributes
    inline static const QString kColorInfo      = "\x1B[1;30m"; //!< bold black
    inline static const QString kColorDebug     = "\x1B[2;34m"; //!< dim green
    inline static const QString kColorFullDebug = "\x1B[2;33m"; //!< dim yellow
    inline static const QString kColorError     = "\x1B[1;31m"; //!< bold red

public:
    enum class DebugLevel
    {
        None = 0,
        Debug,
        FullDebug,
    };

signals:
    void sigLog(QString msg, bool newline, DebugLevel debugLvl);
    void sigError(QString msg);

private:
    QTextStream _cout; //!< stream for stdout
    QTextStream _cerr; //!< stream for stderr

    QFile       *_logFile;   //!< to save logs in file in case of crash
    QTextStream *_logStream; //!< file stream

    DebugLevel _debugLevel;

    /*!
     * \brief CMD programs (like ngPost) may want a Progress Bar
     * If they want to use a Logger also, for a nice integration
     * the logger should own it to be able to return to the line
     * before writting a new log if the progress bar is running.
     * otherwise there is an overlap...
     *
     * To be able to do that we hook the ProgressCallback of the ProgressBar:
     * we always connect the _progressBar to our NgLogger::_doProgressBarUpdate
     * so it forwards to _progressCallback requested by the user
     * having added a Qt::endl before depending on _lastLogByProgressBar
     */
    ProgressBar::ShellBar        *_progressBar;
    ProgressBar::ProgressCallback _progressCallback;
    bool _lastLogByProgressBar; //!< if true, on normal log we should first return to the line

    void _doProgressBarUpdate(ProgressBar::UpdateBarInfo &currentPos); //!< progressBar hook

#ifdef __USE_HMI__
    MainWindow *_hmi;
#endif

public:
    ~NgLogger() override;

    static bool isDebugMode() { return sInstance->_debugLevel != DebugLevel::None; }
    static bool isFullDebug() { return sInstance->_debugLevel == DebugLevel::FullDebug; }
    static void setDebug(DebugLevel level) { sInstance->_debugLevel = level; }

    //! as NgLogger derives privately from Singleton<NgLogger>
    //! we've to
    static void createInstance() { Singleton<NgLogger>::createInstance(); }

    static void startLogInFile(QString const &logFilePath);
    static bool loggingInFile() { return instance()._logStream != nullptr; }

    // static thred safe methods to hide the use of signals/slots
    static void log(QString msg, bool newline, DebugLevel debugLvl = DebugLevel::None)
    {
        if (instance()._debugLevel < debugLvl)
            return;
        emit sInstance->sigLog(msg, newline, debugLvl);
    }
    static void error(QString err) { emit sInstance->sigError(err); }
    static void error(QStringList const &errors)
    {
        for (auto const &err : errors)
            emit sInstance->sigError(err);
    }

    static void criticalError(QString const &errorMsg, NgError::ERR_CODE errCode)
    {
        NgError::setErrCode(errCode);
        if (errCode == NgError::ERR_CODE::COMPLETED_WITH_ERRORS)
            error(errorMsg);
        else
            error(QString("[%1] %2").arg(NgError::errorName(errCode), errorMsg));
    }

    static void createProgressBar(ProgressBar::ProgressCallback const &callback, bool startIt = true);
    static bool startProgressBar(bool waitEventLoopStarted);
    static void stopProgressBar(bool lastRefresh);

public slots:
    void onLog(QString msg, bool newline, DebugLevel debugLvl);
    void onError(QString error);

private:
    NgLogger();

    QString const &logColor(DebugLevel debugLvl) const
    {
        switch (debugLvl)
        {
        case DebugLevel::None:
            return kColorInfo;
        case DebugLevel::Debug:
            return kColorDebug;
        case DebugLevel::FullDebug:
            return kColorFullDebug;
        }
        return kColorInfo;
    }
};

#endif // NGLOGGER_H
