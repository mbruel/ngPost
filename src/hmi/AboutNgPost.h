#ifndef ABOUTNGPOST_H
#define ABOUTNGPOST_H

#include <QDialog>
class NgPost;

namespace Ui {
class AboutNgPost;
}

class AboutNgPost : public QDialog
{
    Q_OBJECT

public:
    explicit AboutNgPost(NgPost *ngPost, QWidget *parent = nullptr);
    ~AboutNgPost();

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;

private:
    Ui::AboutNgPost *ui;
};

#endif // ABOUTNGPOST_H
