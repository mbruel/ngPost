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

#ifndef FOLDERMONITOR_H
#define FOLDERMONITOR_H

#include <QFileSystemWatcher>
#include <QDateTime>
class QFileInfo;
class FolderMonitor : public QObject
{
    Q_OBJECT

private:
    QFileSystemWatcher _monitor;        //!< monitor
    QDateTime          _lastCheck;      //!< last check date
    QStringList        _processedFiles; //!< track files processed (their date might be > _lastCheck)

    static const ulong sMSleep = 100;

public:
    FolderMonitor(const QString &folderPath, QObject *parent = nullptr);
    inline void addFolderToMonitor(const QString &folderPath);

signals:
    void newFileToProcess(const QFileInfo &fileInfo);


public slots:
    void onDirectoryChanged(const QString &folderPath);
};

void FolderMonitor::addFolderToMonitor(const QString &folderPath)
{
    _monitor.addPath(folderPath);
}

#endif // FOLDERMONITOR_H
