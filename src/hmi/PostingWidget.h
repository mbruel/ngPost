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

#ifndef POSTINGWIDGET_H
#define POSTINGWIDGET_H

#include <QWidget>
class NgPost;
class NntpFile;
class MainWindow;
class PostingJob;
#include <QFileInfoList>

namespace Ui {
class PostingWidget;
}

class PostingWidget : public QWidget
{
    Q_OBJECT

private:
    enum class STATE {IDLE, POSTING, STOPPING};

    Ui::PostingWidget *_ui;
    MainWindow        *_hmi;
    NgPost            *_ngPost;
    const uint         _jobNumber;
    PostingJob        *_postingJob;
    STATE              _state;
    bool               _postingFinished;

public:
    explicit PostingWidget(NgPost *ngPost, MainWindow *hmi, uint jobNumber);
    ~PostingWidget();

    void setIDLE();
    void setPosting();

    void init();
    void genNameAndPassword(bool genName, bool genPass, bool doPar2, bool useRarMax);
    inline uint jobNumber() const;

    void handleDropEvent(QDropEvent *e);
    void handleKeyEvent(QKeyEvent *keyEvent);

    void addPath(const QString &path, int currentNbFiles, int isDir = false);

    inline bool isPosting() const;
    inline bool isPostingFinished() const;
    inline MainWindow *hmi() const;

    void udatePostingParams();

    void retranslate();


public slots: // for PostingJob
    void onFilePosted(QString filePath, uint nbArticles, uint nbFailed);
    void onArchiveFileNames(QStringList paths);
    void onArticlesNumber(int nbArticles);
    void onPostingJobDone();

    void onPostFiles(); //!< for the post button but also can be used by the AutoPostWidget

private slots: // for the HMI

    void onNzbPassToggled(bool checked);
    void onGenNzbPassword();


    void onSelectFilesClicked();
    void onSelectFolderClicked();
    void onClearFilesClicked();
    void onCompressCB(bool checked);
    void onGenCompressName();
    void onCompressPathClicked();
    void onNzbFileClicked();
    void onRarPathClicked();


private:
    void _buildFilesList(QFileInfoList &files, bool &hasFolder);
    bool _fileAlreadyInList(const QString &fileName, int currentNbFiles) const;


};

uint PostingWidget::jobNumber() const { return _jobNumber; }
bool PostingWidget::isPosting() const { return _postingJob != nullptr; }
bool PostingWidget::isPostingFinished() const { return _postingFinished; }

MainWindow *PostingWidget::hmi() const { return _hmi; }

#endif // POSTINGWIDGET_H
