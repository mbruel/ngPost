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


#include <QFileSystemWatcher>
#include <QDateTime>
#include <QFileInfoList>
#include <QMap>
#include <QSet>
using AtomicBool = QAtomicInteger<unsigned short>; // 16 bit only (faster than using 8 bit variable...)
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

    friend class NgPost;

private:
    QFileSystemWatcher          _monitor;        //!< monitor
    QMap<QString, FolderScan*>  _folders; //!< track files processed (their date might be > _lastCheck)
    AtomicBool                  _stopListening;

    static ulong sMSleep;


public:
    FoldersMonitorForNewFiles(const QString &folderPath, QObject *parent = nullptr);
    ~FoldersMonitorForNewFiles();

    bool addFolder(const QString &folderPath);
    void stopListening();

signals:
    void newFileToProcess(const QFileInfo &fileInfo);


public slots:
    void onDirectoryChanged(const QString &folderPath);


private:
    qint64 _pathSize(QFileInfo &fileInfo) const;
    qint64 _dirSize(const QString &path) const;
};

#endif // FOLDERSMONITORFORNEWFILES_H
