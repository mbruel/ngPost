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

    void addPath(const QString &path, bool isDir = false);
    bool addPathIfNotInList(const QString &path, int lastIndexToCheck, bool isDir = false);

    void removeItemWidget2(QListWidgetItem *item);

signals:
    void rightClick();
    void empty();

public slots:
    void clear2();
    void onDeleteSelectedItems();



protected:
    void resizeEvent(QResizeEvent *e) override;
    void mousePressEvent(QMouseEvent * e) override;    
    bool eventFilter(QObject *obj, QEvent *event) override;


private:
    QLabel *_asciiLbl;
    QSize   _sizeAscii;
    QIcon   _fileIcon;
    QIcon   _folderIcon;
};



#endif // SIGNEDLISTWIDGET_H
