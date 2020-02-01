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

#ifndef AUTOPOSTWIDGET_H
#define AUTOPOSTWIDGET_H

#include <QWidget>
class NgPost;
class MainWindow;
class QFileInfo;


namespace Ui {
class AutoPostWidget;
}

class AutoPostWidget : public QWidget
{
    Q_OBJECT
private:
    Ui::AutoPostWidget *_ui;
    MainWindow         *_hmi;
    NgPost             *_ngPost;
    bool                _isMonitoring;
    int                 _currentPostIdx;

public:
    explicit AutoPostWidget(NgPost *ngPost, MainWindow *hmi);
    ~AutoPostWidget();

    void init();

    void handleKeyEvent(QKeyEvent *keyEvent);

    void udatePostingParams();

    void updateFinishedJob(const QString &path, uint nbArticles, uint nbFailed);

    bool deleteFilesOncePosted() const;

public slots:
    void onMonitorJobStart();

private slots:
    void onGenQuickPosts();
    void onCompressPathClicked();
    void onRarPathClicked();
    void onSelectAutoDirClicked();
    void onScanAutoDirClicked();
    void onMonitoringClicked();
    void onNewFileToProcess(const QFileInfo &fileInfo);
    void onDelFilesToggled(bool checked);



private:    
    void _addPath(const QString &path, int currentNbFiles, int isDir = false);
    bool _fileAlreadyInList(const QString &fileName, int currentNbFiles) const;

};

#endif // AUTOPOSTWIDGET_H
