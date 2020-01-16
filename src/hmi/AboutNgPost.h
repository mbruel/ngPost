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
private:
    static constexpr const char *sTextColor = "#25aae1"; //!< same blue that the logo

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
