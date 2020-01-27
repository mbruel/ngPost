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

#ifndef FOLDERSMONITORFORNEWFILES_H
#define FOLDERSMONITORFORNEWFILES_H


#include <QFileSystemWatcher>
#include <QDateTime>
#include <QFileInfoList>
#include <QMap>
#include <QSet>

using PathSet = QSet<QString>;

class FolderScan
{
public:
    const QString path;
    QDateTime     lastUpdate;
    PathSet       previousScan;

    FolderScan(const QString &folderPath);
    FolderScan(const FolderScan &other) = delete;
    FolderScan &operator=(const FolderScan &) = delete;

    ~FolderScan() = default;
};

class FoldersMonitorForNewFiles : public QObject
{
    Q_OBJECT

private:
    QFileSystemWatcher          _monitor;        //!< monitor
    QMap<QString, FolderScan*>   _folders; //!< track files processed (their date might be > _lastCheck)

    static const ulong sMSleep = 100;


public:
    FoldersMonitorForNewFiles(const QString &folderPath, QObject *parent = nullptr);
    ~FoldersMonitorForNewFiles();

    bool addFolder(const QString &folderPath);

signals:
    void newFileToProcess(const QFileInfo &fileInfo);


public slots:
    void onDirectoryChanged(const QString &folderPath);
};

#endif // FOLDERSMONITORFORNEWFILES_H
