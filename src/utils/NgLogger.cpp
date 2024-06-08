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
#include "NgLogger.h"

#include <QDateTime>
#include <QFile>

#include "Macros.h"
#include "NgTools.h"
#include <QDebug> // MB_TODO to be removed"

NgLogger::NgLogger()
    : QObject(nullptr)
    , Singleton<NgLogger>()
    , _cout(stdout)
    , _cerr(stderr)
    , _logFile(nullptr)
    , _logStream(nullptr)
    , _debugLevel(DebugLevel::Debug)
    // #ifdef __DEBUG__
    //     , _debugLevel(DebugLevel::FullDebug)
    // #else
    //     , _debugLevel(DebugLevel::None)
    // #endif
    , _progressBar(nullptr)
    , _progressCallback(nullptr)
    , _lastLogByProgressBar(false)
{
#if defined(__DEBUG__) && defined(LOG_CONSTRUCTORS)
    qDebug() << "Creation of the singleton NgLogger";
#endif
    qRegisterMetaType<NgLogger::DebugLevel>("DebugLevel");
}

void NgLogger::connectSignalSlots()
{
    // Qt::QueuedConnection for thread safety !
    connect(this, &NgLogger::sigLog, this, &NgLogger::onLog, Qt::QueuedConnection);
    connect(this, &NgLogger::sigError, this, &NgLogger::onError, Qt::QueuedConnection);
}

NgLogger::~NgLogger()
{
#if defined(__DEBUG__) && defined(LOG_CONSTRUCTORS)
    qDebug() << "Destruction of the singleton NgLogger";
#endif
    if (_logFile)
    {
        delete _logStream;
        delete _logFile;
    }
    if (_progressBar)
        _progressBar->deleteLater();
}

void NgLogger::startLogInFile(QString const &logFilePath)
{
    if (sInstance->_logFile)
    {
        error(tr("A log file has already been set: %1").arg(sInstance->_logFile->fileName()));
        return;
    }
    sInstance->_logFile = new QFile(logFilePath);
    if (sInstance->_logFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        sInstance->_logStream = new QTextStream(sInstance->_logFile);
        log(tr("[%1] ngPost starts logging").arg(NgTools::currentDateTime()), true);
    }
    else
    {
        delete sInstance->_logFile;
        sInstance->_logFile = nullptr;
        error(tr("Error opening log file: '%1'").arg(logFilePath));
    }
}

void NgLogger::createProgressBar(ProgressBar::ProgressCallback const &callback, bool startIt)
{
    if (sQuietMode)
        return;

    if (sInstance->_progressBar)
    {
        sInstance->_progressBar->clear();
        sInstance->_progressBar->deleteLater();
    }

    sInstance->_progressBar      = new ::ProgressBar::ShellBar([](ProgressBar::UpdateBarInfo &currentPos)
                                                          { sInstance->_doProgressBarUpdate(currentPos); },
                                                          NgConf::kProgressBarWidth,
                                                          NgConf::kDefaultRefreshRate);
    sInstance->_progressCallback = callback;
    if (startIt)
        startProgressBar(true);
}

void NgLogger::_doProgressBarUpdate(ProgressBar::UpdateBarInfo &currentPos)
{
    if (sQuietMode)
        return;

    if (!_lastLogByProgressBar)
        _cout << Qt::endl;
    _progressCallback(currentPos);
    _lastLogByProgressBar = true;
}

bool NgLogger::startProgressBar(bool waitEventLoopStarted)
{
    if (sQuietMode)
        return false;

    if (!sInstance->_progressBar)
    {
        error(tr("No progess bar... a call to createProgressBar is needed."));
        return false;
    }
    sInstance->_progressBar->start(waitEventLoopStarted);
    return true;
}

void NgLogger::stopProgressBar(bool lastRefresh)
{
    if (sQuietMode)
        return;

    if (sInstance->_progressBar)
        sInstance->_progressBar->stop(lastRefresh);
}

void NgLogger::onLog(QString msg, bool newline, DebugLevel debugLvl)
{
#ifdef __USE_HMI__
    if (_hmi)
        _hmi->log(aMsg, newline);
    else
#endif
    {
        if (_lastLogByProgressBar)
            _cout << Qt::endl;

        _cout << logColor(debugLvl) << msg;
        if (newline)
            _cout << Qt::endl;
        _cout << kColorReset << MB_FLUSH;
        _lastLogByProgressBar = false;
    }

    if (_logStream && newline)
        *_logStream << msg << Qt::endl << MB_FLUSH; // force flush in case of crash
}

void NgLogger::onError(QString error)
{
#ifdef __USE_HMI__
    if (_hmi)
        _hmi->logError(error);
    else
#endif
    {
        if (_lastLogByProgressBar)
            _cout << Qt::endl;

        _cerr << kColorError << error << Qt::endl << kColorReset << MB_FLUSH;
        _lastLogByProgressBar = false;
    }

    if (_logStream)
        *_logStream << "ERR: " << error << Qt::endl << MB_FLUSH; // force flush in case of crash
}
