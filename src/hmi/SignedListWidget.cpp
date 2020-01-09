#include "SignedListWidget.h"
#include <QLabel>
#include <QResizeEvent>

SignedListWidget::SignedListWidget(QWidget *parent) : QListWidget(parent),
    _asciiLbl(new QLabel(this)){}


void SignedListWidget::setSignature(const QString &str)
{
    _asciiLbl->setText(str);
    _sizeAscii = _asciiLbl->sizeHint();
}

void SignedListWidget::addItem2(const QString &label)
{
    addItem(label);
    if (_asciiLbl->isVisible())
        _asciiLbl->hide();
}

//#include <QDebug>
void SignedListWidget::removeItemWidget2(QListWidgetItem *item)
{
//    qDebug() << "[removeItemWidget2] count: " << count();
    if (count() == 1)
        _asciiLbl->show();
    removeItemWidget(item);
}

void SignedListWidget::clear2()
{
    clear();
    _asciiLbl->show();
}

void SignedListWidget::resizeEvent(QResizeEvent *e)
{
    QListView::resizeEvent(e);
    QSize listSize = e->size();
    _asciiLbl->setGeometry(listSize.width() - _sizeAscii.width(),
                           listSize.height() - _sizeAscii.height(),
                           _sizeAscii.width(),
                           _sizeAscii.height());
}

void SignedListWidget::mousePressEvent(QMouseEvent *e)
{
    QListWidget::mousePressEvent(e);
    if (e->button() == Qt::RightButton)
        emit rightClick();
}
