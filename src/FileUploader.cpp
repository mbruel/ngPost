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

#include "FileUploader.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>

FileUploader::FileUploader(QNetworkAccessManager *netMgr, const QString &nzbFilePath):
    QObject(), _netMgr(netMgr), _reply(nullptr),
    _nzbFilePath(nzbFilePath), _nzbFile(nzbFilePath)
{}

FileUploader::~FileUploader()
{
    if (_nzbFile.isOpen())
    {
        qDebug() << "Deleting FileUploader for " << _nzbFile.fileName();
        _nzbFile.close();
    }

    if (_reply)
        delete _reply;
}

void FileUploader::startUpload(const QUrl &serverUrl)
{
    if (_nzbFile.open(QIODevice::ReadOnly))
    {
        QUrl nzbUrl(QString("%1/%2").arg(serverUrl.url()).arg(_nzbFilePath.fileName()));
        qDebug() << "FileUploader url: " << nzbUrl.url();

        _reply = _netMgr->put(QNetworkRequest(nzbUrl), &_nzbFile);
        QObject::connect(_reply, &QNetworkReply::finished, this, &FileUploader::onUploadFinished);
    }
    else
    {
        qCritical() << "Error FileUploader: can't open file " << _nzbFile.fileName();
        emit readyToDie();
    }
}

void FileUploader::onUploadFinished()
{
    if (_reply->error())
        qCritical() << "Error FileUploader with: " << _nzbFile.fileName() << " : " << _reply->errorString();
    else
        qDebug() << "FileUploader done: " << _nzbFile.fileName();

    emit readyToDie();
}
