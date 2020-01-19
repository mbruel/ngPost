#ifndef POSTINGWIDGET_H
#define POSTINGWIDGET_H

#include <QWidget>
class NgPost;
class NntpFile;
class MainWindow;

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
    STATE              _state;


public:
    explicit PostingWidget(NgPost *ngPost, MainWindow *hmi);
    ~PostingWidget();

    void setFilePosted(NntpFile *nntpFile);
    void setIDLE();

    void init();


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

    int  _createNntpFiles();
    void _udatePostingParams();


    void _addPath(const QString &path, int currentNbFiles, int isDir = false);
    bool _fileAlreadyInList(const QString &fileName, int currentNbFiles) const;
    bool _thereAreFolders() const;



};

#endif // POSTINGWIDGET_H
