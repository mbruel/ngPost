//========================================================================
//
// Copyright (C) 2019 Matthieu Bruel <Matthieu.Bruel@gmail.com>
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileInfoList>
class NgPost;
class NntpServerParams;
class NntpFile;
class PostingWidget;
class AutoPostWidget;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    enum class STATE {IDLE, POSTING, STOPPING};

    Ui::MainWindow *_ui;
    NgPost         *_ngPost;
    STATE           _state;
    PostingWidget  *_quickJobTab;
    AutoPostWidget *_autoPostTab;

    static const bool sDefaultServerSSL   = true;
    static const int  sDefaultConnections = 5;
    static const int  sDefaultServerPort  = 563;
    static const int  sDeleteColumnWidth  = 30;

    static const QStringList  sServerListHeaders;
    static const QVector<int> sServerListSizes;



public:
    explicit MainWindow(NgPost *ngPost, QWidget *parent = nullptr);
    ~MainWindow();

    void init();


    void setProgressBarRange(int start, int end);
    void updateProgressBar(uint nbArticlesTotal, uint nbArticlesUploaded, const QString &avgSpeed);

    void updateServers();
    void updateParams();

    PostingWidget *addNewQuickTab(int lastTabIdx, const QFileInfoList &files = QFileInfoList());

    void setTab(QWidget *postWidget);
    void clearJobTab(QWidget *postWidget);
    void updateJobTab(QWidget *postWidget, const QColor &color, const QIcon &icon, const QString &tooltip = "");

    void setJobLabel(uint jobNumber);

    void log(const QString &aMsg, bool newline = true) const; //!< log function for QString
    void logError(const QString &error) const; //!< log function for QString


    bool hasFinishedPosts() const;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

    void closeEvent(QCloseEvent *event) override;



private slots:
    void onAddServer();
    void onDelServer();

    void onObfucateToggled(bool checked);

    void onTabContextMenu(const QPoint &point);
    void onCloseAllFinishedQuickTabs();



    void onGenPoster();

    void onDebugToggled(bool checked);

    void onSaveConfig();

    void onJobTabClicked(int index);
    void onCloseJob(int index);

    void toBeImplemented();

private:
    void _initServerBox();
    void _initPostingBox();

    void _addServer(NntpServerParams *serverParam);
    int  _serverRow(QObject *delButton);
    PostingWidget *_getPostWidget(int tabIndex) const;



    static const QString sGroupBoxStyle;
};

#endif // MAINWINDOW_H
