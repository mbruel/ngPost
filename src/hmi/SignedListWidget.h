#ifndef SIGNEDLISTWIDGET_H
#define SIGNEDLISTWIDGET_H

#include <QListWidget>
class QLabel;
class SignedListWidget : public QListWidget
{
    Q_OBJECT

public:
    SignedListWidget(QWidget *parent = nullptr);

    void setSignature(const QString &str);

    void addItem2(const QString &label);
    void removeItemWidget2(QListWidgetItem *item);
    void clear2();

signals:
    void rightClick();

protected:
    void resizeEvent(QResizeEvent *e) override;
    void mousePressEvent(QMouseEvent * e) override;


private:
    QLabel *_asciiLbl;
    QSize   _sizeAscii;
};



#endif // SIGNEDLISTWIDGET_H
