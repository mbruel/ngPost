//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
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

#include "FoldersMonitorForNewFiles.h"
#include "utils/Macros.h"
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QThread>

ulong FoldersMonitorForNewFiles::sMSleep = 1000; //!< 1sec in case we move file from samba or unrar when the system is quite loaded

FoldersMonitorForNewFiles::FoldersMonitorForNewFiles(const QString &folderPath, QObject *parent) :
    QObject(parent), _monitor(), _stopListening(0x0)
{
    connect(&_monitor, &QFileSystemWatcher::directoryChanged, this, &FoldersMonitorForNewFiles::onDirectoryChanged);
    addFolder(folderPath);
}

FoldersMonitorForNewFiles::~FoldersMonitorForNewFiles()
{
    qDeleteAll(_folders);
}

bool FoldersMonitorForNewFiles::addFolder(const QString &folderPath)
{
    QFileInfo fi(folderPath);
    if (fi.exists() && fi.isDir() && fi.isReadable())
    {
        qDebug() << "monitoring new folder: " << folderPath;
        _folders.insert(folderPath, new FolderScan(folderPath));
        _monitor.addPath(folderPath);
        return true;
    }
    else
        return false;
}

void FoldersMonitorForNewFiles::stopListening()
{
    qDebug() << "[FoldersMonitorForNewFiles::stopListening] stop monitoring!";

    _stopListening.testAndSetOrdered(0x0, 0x1);
    disconnect(&_monitor, &QFileSystemWatcher::directoryChanged, this, &FoldersMonitorForNewFiles::onDirectoryChanged);
}

void FoldersMonitorForNewFiles::onDirectoryChanged(const QString &folderPath)
{
    if (MB_LoadAtomic(_stopListening))
        return;

    FolderScan *folderScan  = _folders[folderPath];
    QDateTime currentUpdate = QFileInfo(folderPath).lastModified();

    qDebug() << "[directoryChanged] " << folderPath
             << " (lastUpdate: " << folderScan->lastUpdate << ", now: " << currentUpdate << ")";

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    PathSet newScan  = QDir(folderPath).entryList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot).toSet();
#else
    QStringList newScanTmpList = QDir(folderPath).entryList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot);
    PathSet newScan(newScanTmpList.begin(), newScanTmpList.end());
#endif

    // iterate new paths
    PathSet newFiles = newScan; // this will detach!
    newFiles.subtract(folderScan->previousScan);
    for (const QString &fileName : newFiles)
    {
        if (MB_LoadAtomic(_stopListening))
            break;

        QString filePath = QString("%1/%2").arg(folderPath).arg(fileName);
        QFileInfo fi(filePath);
        fi.setCaching(false); // really important to be able to check if the file is finished to be written
        if (!fi.exists())
        {
            qCritical() << "[directoryChanged] error file doesn't exist: " << filePath;
            continue;
        }

        qint64 size = _pathSize(fi);
        qDebug() << "[directoryChanged] processing new file: " << filePath
                 << ", size: " << size << ", lastModif: " << fi.lastModified();


        // wait the file is fully written
        ushort nbWait = 0;
        do
        {
            size = _pathSize(fi);
            QThread::msleep(sMSleep);
            ++nbWait;
        } while (fi.exists() && size != _pathSize(fi));


        if (fi.exists())
        {
            qDebug() << "[directoryChanged] after " << nbWait*sMSleep << " msec, "
                     << "ready to process file: " << filePath
                     << ", size: " << size << ", lastModif: " << fi.lastModified();

            if (!MB_LoadAtomic(_stopListening))
                emit sigNewFileToProcess(fi);
#ifdef __DEBUG__
            if (fi.isDir())
            {
                for(QFileInfo &subFile: QDir(fi.absoluteFilePath()).entryInfoList(
                        QDir::Files|QDir::Hidden|QDir::System|QDir::Dirs|QDir::NoDotAndDotDot))
                    qDebug() << "\t- " << subFile.fileName() << ": size: " << _pathSize(subFile);

            }
#endif
        }
        else
            qDebug() << "[directoryChanged] ignoring temporary file: " << filePath;
    }

    folderScan->lastUpdate   = currentUpdate;
    folderScan->previousScan = newScan;
}

qint64 FoldersMonitorForNewFiles::_pathSize(QFileInfo &fileInfo) const
{
    fileInfo.refresh();
    if (fileInfo.isDir())
        return _dirSize(fileInfo.absoluteFilePath());
    else
        return fileInfo.size();
}

qint64 FoldersMonitorForNewFiles::_dirSize(const QString &path) const
{
    qint64 size = 0;
    QDir dir(path);
    for(const QFileInfo &fi: dir.entryInfoList(QDir::Files|QDir::Hidden|QDir::System|QDir::Dirs|QDir::NoDotAndDotDot)) {
        if (fi.isDir())
            size += _dirSize(fi.absoluteFilePath());
        else
            size+= fi.size();
    }
    return size;
}

FolderScan::FolderScan(const QString &folderPath) :
    path(folderPath),
    lastUpdate(QDateTime::currentDateTime()),
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    previousScan(QDir(folderPath).entryList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot).toSet())
#else
    previousScan()
#endif
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QStringList scanList = QDir(folderPath).entryList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot);
    previousScan = PathSet(scanList.begin(), scanList.end());
#endif
}
