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
    enum class STATE {IDLE, PENDING, POSTING, DONE, STOPPING};

    Ui::PostingWidget *_ui;
    MainWindow        *_hmi;
    NgPost            *_ngPost;
    const uint         _jobNumber;
    PostingJob        *_postingJob;
    STATE              _state;


    static const QColor  sPostingColor;
    static const QString sPostingIcon;
    static const QColor  sPendingColor;
    static const QString sPendingIcon;
    static const QColor  sDoneOKColor;
    static const QString sDoneOKIcon;
    static const QColor  sDoneKOColor;
    static const QString sDoneKOIcon;

public:
    explicit PostingWidget(NgPost *ngPost, MainWindow *hmi, uint jobNumber);
    ~PostingWidget();

//    void setFilePosted(NntpFile *nntpFile);
    void setIDLE();

    void init();
    inline uint jobNumber() const;

public slots:
    void onFilePosted(QString filePath, int nbArticles, int nbFailed);
    void onArchiveFileNames(QStringList paths);
    void onArticlesNumber(int nbArticles);
    void onPostingJobDone();

private slots:
    void onPostFiles();
    void onAboutClicked();

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


protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

private:
    void _buildFilesList(QFileInfoList &files, bool &hasFolder);

    void _udatePostingParams();


    void _addPath(const QString &path, int currentNbFiles, int isDir = false);
    bool _fileAlreadyInList(const QString &fileName, int currentNbFiles) const;


};

uint PostingWidget::jobNumber() const { return _jobNumber; }

#endif // POSTINGWIDGET_H
