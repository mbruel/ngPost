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

#ifndef FILEUPLOADER_H
#define FILEUPLOADER_H
#include <QFile>
#include <QFileInfo>
class QNetworkAccessManager;
class QNetworkReply;

class FileUploader : public QObject
{
    Q_OBJECT

private:
    QNetworkAccessManager *_netMgr;
    QNetworkReply         *_reply;
    QFileInfo              _nzbFilePath;
    QFile                  _nzbFile;

public:
    FileUploader(QNetworkAccessManager *netMgr, const QString &nzbFilePath);
    ~FileUploader();

    void startUpload(const QUrl &serverUrl);

signals:
    void readyToDie();

private slots:
    void onUploadFinished();

};

#endif // FILEUPLOADER_H
