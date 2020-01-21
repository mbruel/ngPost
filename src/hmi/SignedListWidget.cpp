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

#include "SignedListWidget.h"
#include <QLabel>
#include <QResizeEvent>

SignedListWidget::SignedListWidget(QWidget *parent) :
    QListWidget(parent),
    _asciiLbl(new QLabel(this)),
    _fileIcon(":/icons/file.png"),
    _folderIcon(":/icons/folder.png")
{}


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
