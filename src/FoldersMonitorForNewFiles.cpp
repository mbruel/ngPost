//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
//
// This file is a part of ngPost : https://github.com/mbruel/ngPost
//
// ngPost is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; version 3.0 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// You should have received a copy of the GNU Lesser General Public
// License along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301,
// USA.
//
//========================================================================

#include "FoldersMonitorForNewFiles.h"
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QThread>
FoldersMonitorForNewFiles::FoldersMonitorForNewFiles(const QString &folderPath, QObject *parent) :
    QObject(parent), _monitor()
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

void FoldersMonitorForNewFiles::onDirectoryChanged(const QString &folderPath)
{
    FolderScan *folderScan = _folders[folderPath];

    QDateTime currentUpdate = QFileInfo(folderPath).lastModified();

    qDebug() << "[directoryChanged] " << folderPath
             << " (lastUpdate: " << folderScan->lastUpdate << ", now: " << currentUpdate << ")";

    PathSet newScan  = QDir(folderPath).entryList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot).toSet();

    // iterate new paths
    PathSet newFiles = newScan; // this will detach!
    newFiles.subtract(folderScan->previousScan);
    for (const QString &fileName : newFiles)
    {
        QString filePath = QString("%1/%2").arg(folderPath).arg(fileName);
        QFileInfo fi(filePath);
        fi.setCaching(false); // really important to be able to check if the file is finished to be written
        if (!fi.exists())
        {
            qCritical() << "[directoryChanged] error file doesn't exist: " << filePath;
            continue;
        }

        QDateTime modif = fi.lastModified();
        qDebug() << "[directoryChanged] processing new file: " << filePath
                 << ", size: " << fi.size() << ", lastModif: " << modif;


        // wait the file is fully written
        ushort nbWait = 0;
        do
        {
            modif = fi.lastModified();
            QThread::msleep(sMSleep);
            ++nbWait;
        } while (modif != fi.lastModified() && fi.exists());


        if (fi.exists())
        {
            qDebug() << "[directoryChanged] after " << nbWait*sMSleep << " msec, "
                     << "ready to process file: " << filePath
                     << ", size: " << fi.size() << ", lastModif: " << modif;

            emit newFileToProcess(fi);
        }
        else
            qDebug() << "[directoryChanged] ignoring temporary file: " << filePath;
    }

    folderScan->lastUpdate   = currentUpdate;
    folderScan->previousScan = newScan;

}

FolderScan::FolderScan(const QString &folderPath) :
    path(folderPath),
    lastUpdate(QDateTime::currentDateTime()),
    previousScan(QDir(folderPath).entryList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot).toSet())
{}
