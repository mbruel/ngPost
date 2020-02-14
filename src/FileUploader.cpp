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
#include <QHttpMultiPart>
FileUploader::FileUploader(QNetworkAccessManager *netMgr, const QString &nzbFilePath):
    QObject(), _netMgr(netMgr), _reply(nullptr),
    _nzbFilePath(nzbFilePath), _nzbFile(nzbFilePath), _nzbUrl()
{}

FileUploader::~FileUploader()
{
    if (_nzbFile.isOpen())
    {
#ifdef __DEBUG__
        qDebug() << "Deleting FileUploader for " << _nzbFile.fileName();
#endif
        _nzbFile.close();
    }

    if (_reply)
        delete _reply;
}

void FileUploader::startUpload(const QUrl &serverUrl)
{
    if (_nzbFile.open(QIODevice::ReadOnly))
    {
        QString protocol = serverUrl.scheme(); // always lowercase
        if (protocol == "ftp")
        {
            _nzbUrl = QUrl(QString("%1/%2").arg(serverUrl.url()).arg(_nzbFilePath.fileName()));
    #ifdef __DEBUG__
            qDebug() << "FileUploader FTP url: " << _nzbUrl.url();
    #endif

            _reply = _netMgr->put(QNetworkRequest(_nzbUrl), &_nzbFile);
        }
        else if (protocol.startsWith("http"))
        {
            _nzbUrl = serverUrl;

#ifdef __DEBUG__
        qDebug() << "FileUploader POST on url: " << _nzbUrl.url();
#endif

             QNetworkRequest req(_nzbUrl);
            req.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));
            _reply = _netMgr->post(req, &_nzbFile);

            //https://forum.qt.io/topic/56708/solved-qnetworkaccessmanager-adding-a-multipart-form-data-to-a-post-request/9
//            QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

//            QHttpPart textPart;
//            textPart.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
//            textPart.setBody("my text");
//            //    textPart.setBody(QByteArray); //How to set parameters like with QUrlQuery
//            postData.addQueryItem("access_token", access_token);

//            QHttpPart fileDataPart;
//            fileDataPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"file\"; filename=\"test.nzb\""));
//            fileDataPart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));

//            fileDataPart.setBodyDevice(&_nzbFile);

//            _reply = _netMgr->post(QNetworkRequest(_nzbUrl), multiPart);
        }
        else
        {
            emit error(tr("Error uploading nzb to %1: Protocol not supported").arg(url()));
            emit readyToDie();
        }
        if (_reply)
            QObject::connect(_reply, &QNetworkReply::finished, this, &FileUploader::onUploadFinished);
    }
    else
    {
        emit error(tr("Error uploading file: can't open file ").arg(_nzbFile.fileName()));
        emit readyToDie();
    }
}

void FileUploader::onUploadFinished()
{
    qDebug() << "FileUploader reply: " << _reply->readAll();
    if (_reply->error())
        emit error(tr("Error uploading nzb to %1: %2").arg(url()).arg(_reply->errorString()));
    else
        emit log(tr("nzb %1 uploaded to %2\n").arg(_nzbFilePath.fileName()).arg(url()));

    emit readyToDie();
}

