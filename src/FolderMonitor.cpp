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

#include "FolderMonitor.h"
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QThread>
FolderMonitor::FolderMonitor(const QString &folderPath, QObject *parent) :
    QObject(parent), _monitor(),
    _lastCheck(QFileInfo(folderPath).lastModified()),
    _processedFiles()
{
    addFolderToMonitor(folderPath);
    connect(&_monitor, &QFileSystemWatcher::directoryChanged, this, &FolderMonitor::onDirectoryChanged);
}

void FolderMonitor::onDirectoryChanged(const QString &folderPath)
{
    QDateTime checkTime = QFileInfo(folderPath).lastModified();

    qDebug() << "[directoryChanged] " << folderPath
             << " (lastcheck: " << _lastCheck << ", now: " << checkTime << ")"
             << " _processedFiles.size() = "  << _processedFiles.size();

    QDir dir(folderPath);
    for (QFileInfo & fi : dir.entryInfoList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot, QDir::Time))
    {
        QString   filePath = fi.absoluteFilePath();
        QDateTime modif    = fi.lastModified();
        fi.setCaching(false); // really important to be able to check if the file is finished to be written

        if (modif > _lastCheck && !_processedFiles.contains(filePath))
        {
            qDebug() << "[directoryChanged] check file: " << filePath
                     << ", size: " << fi.size() << ", date: " << modif;

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
                qDebug() << "[directoryChanged] ready to process file: " << filePath
                         << " after " << nbWait*sMSleep << " msec"
                         << ", size: " << fi.size() << ", date: " << modif;
                emit newFileToProcess(fi);
            }
            else
                qDebug() << "[directoryChanged] ignoring tmp file: " << filePath;

            _processedFiles << filePath;
        }
        else // files were already checked
            break;
    }

    _lastCheck = checkTime;

    // clean _processedFiles
    auto it = _processedFiles.begin();
    while (it != _processedFiles.end())
    {
        if (QFileInfo(*it).lastModified() < checkTime)
        {
            qDebug() << "[directoryChanged] remove processed file: " << *it;
            it = _processedFiles.erase(it);
        }
        else
            ++it;
    }
}
