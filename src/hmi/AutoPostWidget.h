#ifndef AUTOPOSTWIDGET_H
#define AUTOPOSTWIDGET_H

#include <QWidget>
class NgPost;
class MainWindow;
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


public:
    explicit AutoPostWidget(NgPost *ngPost, MainWindow *hmi);
    ~AutoPostWidget();

    void init();

    void handleKeyEvent(QKeyEvent *keyEvent);

private slots:
    void onGenQuickPosts();
    void onCompressPathClicked();
    void onRarPathClicked();
    void onSelectAutoDirClicked();
    void onScanAutoDirClicked();


private:    
    void _addPath(const QString &path, int currentNbFiles, int isDir = false);
    bool _fileAlreadyInList(const QString &fileName, int currentNbFiles) const;


};

#endif // AUTOPOSTWIDGET_H
