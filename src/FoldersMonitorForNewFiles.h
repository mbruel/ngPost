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
#ifndef FOLDERSMONITORFORNEWFILES_H
#define FOLDERSMONITORFORNEWFILES_H

#include <QDateTime>
#include <QFileInfoList>
#include <QFileSystemWatcher>
#include <QMap>
#include <QSet>
using AtomicBool = QAtomicInteger<unsigned short>; // 16 bit only (faster than using 8 bit variable...)
using PathSet    = QSet<QString>;

class FolderScan
{
public:
    const QString path;
    QDateTime     lastUpdate;
    PathSet       previousScan;

    FolderScan(QString const &folderPath);
    FolderScan(FolderScan const &other)       = delete;
    FolderScan &operator=(FolderScan const &) = delete;

    ~FolderScan() = default;
};

class FoldersMonitorForNewFiles : public QObject
{
    Q_OBJECT

    friend class NgPost;         // MB_TODO: is it still necessary?
    friend class NgConfigLoader; // to update sMSleep

signals:
    void newFileToProcess(QFileInfo const &fileInfo);

private:
    QFileSystemWatcher          _monitor; //!< monitor
    QMap<QString, FolderScan *> _folders; //!< track files processed (their date might be > _lastCheck)
    AtomicBool                  _stopListening;

    static ulong sMSleep;

public:
    FoldersMonitorForNewFiles(QString const &folderPath, QObject *parent = nullptr);
    ~FoldersMonitorForNewFiles();

    bool addFolder(QString const &folderPath);
    void stopListening();

public slots:
    void onDirectoryChanged(QString const &folderPath);

private:
    qint64 _pathSize(QFileInfo &fileInfo) const;
    qint64 _dirSize(QString const &path) const;
};

#endif // FOLDERSMONITORFORNEWFILES_H
