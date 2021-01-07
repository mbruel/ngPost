//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
// This file is a part of ngPost : https://github.com/mbruel/ngPost
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3..
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>
//
//========================================================================

#include "SignedListWidget.h"
#include <QLabel>
#include <QResizeEvent>

SignedListWidget::SignedListWidget(QWidget *parent) :
    QListWidget(parent),
    _asciiLbl(new QLabel(this)),
    _fileIcon(":/icons/file.png"),
    _folderIcon(":/icons/folder.png")
{
//    installEventFilter(this);
}


void SignedListWidget::setSignature(const QString &str)
{
    _asciiLbl->setText(str);
    _sizeAscii = _asciiLbl->sizeHint();
}

void SignedListWidget::addPath(const QString &path, bool isDir)
{
    QListWidgetItem *item = new QListWidgetItem(isDir ? _folderIcon : _fileIcon, path);
    addItem(item);
    if (_asciiLbl->isVisible())
        _asciiLbl->hide();
}

bool SignedListWidget::addPathIfNotInList(const QString &path, int lastIndexToCheck, bool isDir)
{
    for (int i = 0 ; i < lastIndexToCheck ; ++i)
    {
        if (item(i)->text() == path)
            return false;
    }

    addPath(path, isDir);
    return true;
}

//#include <QDebug>
void SignedListWidget::removeItemWidget2(QListWidgetItem *item)
{
//    qDebug() << "[removeItemWidget2] count: " << count();
    if (count() == 1)
    {
        removeItemWidget(item);
        _asciiLbl->show();
        emit empty();
    }
    else
        removeItemWidget(item);
}

void SignedListWidget::clear2()
{
    clear();
    _asciiLbl->show();
}

void SignedListWidget::onDeleteSelectedItems()
{
    for (QListWidgetItem *item : selectedItems())
    {
        removeItemWidget2(item);
        delete item;
    }
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
    {
        emit rightClick();
        e->accept();
    }
}

bool SignedListWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if(keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace)
            onDeleteSelectedItems();
    }
    return QListWidget::eventFilter(obj, event);       
}
