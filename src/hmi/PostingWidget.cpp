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

#include "PostingWidget.h"
#include "ui_PostingWidget.h"
#include "MainWindow.h"
#include "AboutNgPost.h"
#include "NgPost.h"
#include "PostingJob.h"
#include "nntp/NntpFile.h"

#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QKeyEvent>
#include <QClipboard>
#include <QMimeData>



PostingWidget::PostingWidget(NgPost *ngPost, MainWindow *hmi, uint jobNumber) :
    QWidget(hmi),
    _ui(new Ui::PostingWidget),
    _hmi(hmi),
    _ngPost(ngPost),
    _jobNumber(jobNumber),
    _postingJob(nullptr),
    _state(STATE::IDLE),
    _postingFinished(false)
{
    _ui->setupUi(this);

    connect(_ui->postButton, &QAbstractButton::clicked, this, &PostingWidget::onPostFiles);
    connect(_ui->nzbPassCB,  &QAbstractButton::toggled, this, &PostingWidget::onNzbPassToggled);
    connect(_ui->genPass,    &QAbstractButton::clicked, this, &PostingWidget::onGenNzbPassword);
}

PostingWidget::~PostingWidget()
{
    delete _ui;
}

void PostingWidget::onFilePosted(QString filePath, uint nbArticles, uint nbFailed)
{
    int nbFiles = _ui->filesList->count();
    for (int i = 0 ; i < nbFiles ; ++i)
    {
        QListWidgetItem *item = _ui->filesList->item(i);
        if (item->text() == filePath)
        {
            QColor color(_hmi->sDoneOKColor);
            if (nbFailed == 0)
                item->setText(QString("%1 [%2 ok]").arg(filePath).arg(nbArticles));
            else
            {
                item->setText(QString("%1 [%2 err / %3]").arg(filePath).arg(nbFailed).arg(nbArticles));
                if (nbFailed == nbArticles)
                    color = _hmi->sDoneKOColor;
                else
                    color = _hmi->sArticlesFailedColor;
            }
            item->setForeground(color);
            break;
        }
    }
}

void PostingWidget::onArchiveFileNames(QStringList paths)
{
    _ui->filesList->clear();
    for (const QString & path : paths)
        _ui->filesList->addPath(path);
}

void PostingWidget::onArticlesNumber(int nbArticles)
{
    Q_UNUSED(nbArticles);
    _hmi->setJobLabel(static_cast<int>(_jobNumber));
}

void PostingWidget::onPostingJobDone()
{
    if (_postingJob->nbArticlesTotal() > 0)
    {
        if (_postingJob->nbArticlesFailed() > 0)
            _hmi->updateJobTab(this, _hmi->sDoneKOColor, QIcon(_hmi->sDoneKOIcon));
        else
            _hmi->updateJobTab(this, _hmi->sDoneOKColor, QIcon(_hmi->sDoneOKIcon));
    }
    else
        _hmi->clearJobTab(this);

    _postingJob = nullptr; //!< we don't own it, NgPost will delete it
    _postingFinished = true;
    setIDLE();
}

void PostingWidget::onPostFiles()
{
    if (_state == STATE::IDLE)
    {
        if (_ui->filesList->count() == 0)
        {
            _hmi->logError(tr("There are no selected files to post..."));
            return;
        }

        QFileInfoList files;
        bool hasFolder = false;
        _buildFilesList(files, hasFolder);
        if (files.isEmpty())
        {
            _hmi->logError(tr("There are no existing files to post..."));
            return;
        }

        if (hasFolder && !_ui->compressCB->isChecked())
        {
            _hmi->logError(tr("You can't post folders without using compression..."));
            return;
        }


        _hmi->updateServers();
        _hmi->updateParams();
        udatePostingParams();

        // check if the nzb file name already exist
        QString nzbPath = _ngPost->nzbPath();
        if (!nzbPath.endsWith(".nzb"))
            nzbPath += ".nzb";
        QFileInfo fiNzb(nzbPath);
        if (fiNzb.exists())
        {
            int overwrite = QMessageBox::question(nullptr,
                                                  tr("Overwrite existing nzb file?"),
                                                  tr("The nzb file '%1' already exists.\nWould you like to overwrite it ?").arg(nzbPath),
                                                  QMessageBox::Yes,
                                                  QMessageBox::No);
            if (overwrite == QMessageBox::No)
                return;
        }

        _postingFinished = false;
        _state = STATE::POSTING;
        _postingJob = new PostingJob(_ngPost, nzbPath, files, this,
                                     _ngPost->_grpList, _ngPost->_groups, _ngPost->_from,
                                     _ngPost->_obfuscateArticles, _ngPost->_obfuscateFileName,
                                     _ngPost->_tmpPath, _ngPost->_rarPath,
                                     _ngPost->_rarSize, _ngPost->_useRarMax, _ngPost->_par2Pct,
                                     _ngPost->_doCompress, _ngPost->_doPar2,
                                     _ngPost->_rarName, _ngPost->_rarPass,
                                     _ngPost->_keepRar);

        bool hasStarted = _ngPost->startPostingJob(_postingJob);

        QString buttonTxt;
        QColor  tabColor;
        QString tabIcon;
        if (hasStarted)
        {
            buttonTxt = tr("Stop Posting");
            tabColor  = _hmi->sPostingColor;
            tabIcon   = _hmi->sPostingIcon;
        }
        else
        {
            buttonTxt = tr("Cancel Posting");
            tabColor  = _hmi->sPendingColor;
            tabIcon   = _hmi->sPendingIcon;
        }
        _ui->postButton->setText(buttonTxt);
        _hmi->updateJobTab(this, tabColor, QIcon(tabIcon), _postingJob->nzbName());
    }
    else  if (_state == STATE::POSTING)
    {
        _state = STATE::STOPPING;
        emit _postingJob->stopPosting();
    }
}


void PostingWidget::onNzbPassToggled(bool checked)
{
    _ui->nzbPassEdit->setEnabled(checked);
    _ui->passLengthSB->setEnabled(checked);
    _ui->genPass->setEnabled(checked);
}

void PostingWidget::onGenNzbPassword()
{
    _ui->nzbPassEdit->setText(_ngPost->randomPass(static_cast<uint>(_ui->passLengthSB->value())));
}

void PostingWidget::onSelectFilesClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(
                this,
                tr("Select one or more files to Post"),
                _ngPost->_inputDir);

    int currentNbFiles = _ui->filesList->count();
    for (const QString &file : files)
        addPath(file, currentNbFiles);
}

void PostingWidget::onSelectFolderClicked()
{
    QString folder = QFileDialog::getExistingDirectory(
                this,
                tr("Select a Folder"),
                _ngPost->_inputDir,
                QFileDialog::ShowDirsOnly);

    if (!folder.isEmpty())
        addPath(folder, _ui->filesList->count(), true);
}

void PostingWidget::onClearFilesClicked()
{
    _ui->filesList->clear2();
    _ui->nzbFileEdit->clear();
    _ui->compressNameEdit->clear();
    _hmi->clearJobTab(this);
}

void PostingWidget::onCompressCB(bool checked)
{
    _ui->compressNameEdit->setEnabled(checked);
    _ui->nameLengthSB->setEnabled(checked);
    _ui->genCompressName->setEnabled(checked);
    _ui->par2CB->setEnabled(checked);
}

void PostingWidget::onGenCompressName()
{
    _ui->compressNameEdit->setText(_ngPost->randomPass(static_cast<uint>(_ui->nameLengthSB->value())));
}

void PostingWidget::onCompressPathClicked()
{
    QString path = QFileDialog::getExistingDirectory(
                this,
                tr("Select a Folder"),
                _ui->compressPathEdit->text(),
                QFileDialog::ShowDirsOnly);

    if (!path.isEmpty())
        _ui->compressPathEdit->setText(path);
}

void PostingWidget::onNzbFileClicked()
{
    QString path = QFileDialog::getSaveFileName(
                this,
                tr("Create nzb file"),
                _ngPost->_nzbPath,
                "*.nzb"
                );

    if (!path.isEmpty())
        _ui->nzbFileEdit->setText(path);
}

void PostingWidget::onRarPathClicked()
{
    QString path = QFileDialog::getOpenFileName(
                this,
                tr("Select rar executable"),
                QFileInfo(_ngPost->_rarPath).absolutePath()
                );

    if (!path.isEmpty())
    {
        QFileInfo fi(path);
        if (fi.isFile() && fi.isExecutable())
            _ui->rarEdit->setText(path);
        else
            _ngPost->error(tr("the selected file is not executable..."));
    }
}

void PostingWidget::handleKeyEvent(QKeyEvent *keyEvent)
{
    qDebug() << "[PostingWidget::handleKeyEvent] key event: " << keyEvent->key();

    if(keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace)
    {
        for (QListWidgetItem *item : _ui->filesList->selectedItems())
        {
            qDebug() << "[PostingWidget::handleKeyEvent] remove item: " << item->text();
            _ui->filesList->removeItemWidget2(item);
            delete item;
        }
    }
    else if (keyEvent->matches(QKeySequence::Paste))
    {
        const QClipboard *clipboard = QApplication::clipboard();
        const QMimeData *mimeData = clipboard->mimeData();
        if (mimeData->hasImage()) {
            qDebug() << "[PostingWidget::handleKeyEvent] try to paste image...";
        } else if (mimeData->hasHtml()) {
            qDebug() << "[PostingWidget::handleKeyEvent] try to paste html: ";
        } else if (mimeData->hasText()) {
            QString txt = mimeData->text();
            qDebug() << "[PostingWidget::handleKeyEvent] paste text: " << txt;
            int currentNbFiles = _ui->filesList->count();
            for (const QString &path : txt.split(QRegularExpression("\n|\r|\r\n")))
            {
                QFileInfo fileInfo(path);
                if (!fileInfo.exists())
                    qDebug() << "[PostingWidget::handleKeyEvent] NOT A FILE: " << path;
                else
                    addPath(path, currentNbFiles, fileInfo.isDir());
//                        else if (fileInfo.isDir())
//                        {
//                            QDir dir(fileInfo.absoluteFilePath());
//                            for (const QFileInfo &subFile : dir.entryInfoList(QDir::Files, QDir::Name))
//                            {
//                                if (subFile.isReadable())
//                                    _addFile(subFile.absoluteFilePath(), currentNbFiles);
//                            }
//                        }
            }

        } else if (mimeData->hasUrls()) {
            qDebug() << "[PostingWidget::handleKeyEvent] paste urls...";

        } else {
            qDebug() << "[PostingWidget::handleKeyEvent] unknown type...";
        }
    }

}


void PostingWidget::handleDropEvent(QDropEvent *e)
{
    int currentNbFiles = _ui->filesList->count();
    for (const QUrl &url : e->mimeData()->urls())
    {
        QString fileName = url.toLocalFile();
        addPath(fileName, currentNbFiles, QFileInfo(fileName).isDir());
    }
}

void PostingWidget::_buildFilesList(QFileInfoList &files, bool &hasFolder)
{
    for (int i = 0 ; i < _ui->filesList->count() ; ++i)
    {
        QFileInfo fileInfo(_ui->filesList->item(i)->text());
        if (fileInfo.exists())
        {
            files << fileInfo;
            if (fileInfo.isDir())
                hasFolder = true;
        }
    }
}

void PostingWidget::init()
{
    _ui->rarMaxCB->setChecked(_ngPost->_useRarMax);

    _ui->nzbPassCB->setChecked(false);
    onNzbPassToggled(false);

    _ui->filesList->setSignature(QString("<pre>%1</pre>").arg(_ngPost->escapeXML(_ngPost->asciiArt())));
    connect(_ui->filesList, &SignedListWidget::rightClick, this, &PostingWidget::onSelectFilesClicked);

    _ui->compressPathEdit->setText(_ngPost->_tmpPath);
    _ui->rarEdit->setText(_ngPost->_rarPath);

    _ui->rarSizeEdit->setText(QString::number(_ngPost->_rarSize));
    _ui->rarSizeEdit->setValidator(new QIntValidator(1, 1000000, _ui->rarSizeEdit));

    _ui->keepRarCB->setChecked(_ngPost->_keepRar);

    if (_ngPost->_par2Args.isEmpty())
    {
        _ui->redundancySB->setRange(0, 100);
        _ui->redundancySB->setValue(static_cast<int>(_ngPost->_par2Pct));
    }
    else
    {
        _ui->redundancySB->setEnabled(false);
    }

    _ui->nameLengthSB->setRange(5, 50);
    _ui->nameLengthSB->setValue(static_cast<int>(_ngPost->_lengthName));
    _ui->passLengthSB->setRange(5, 50);
    _ui->passLengthSB->setValue(static_cast<int>(_ngPost->_lengthPass));

    _ui->filesList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(_ui->selectFilesButton, &QAbstractButton::clicked, this, &PostingWidget::onSelectFilesClicked);
    connect(_ui->selectFolderButton,&QAbstractButton::clicked, this, &PostingWidget::onSelectFolderClicked);
    connect(_ui->clearFilesButton,  &QAbstractButton::clicked, this, &PostingWidget::onClearFilesClicked);

    connect(_ui->compressCB,        &QAbstractButton::toggled, this, &PostingWidget::onCompressCB);
    connect(_ui->genCompressName,   &QAbstractButton::clicked, this, &PostingWidget::onGenCompressName);


    connect(_ui->compressPathButton,&QAbstractButton::clicked, this, &PostingWidget::onCompressPathClicked);
    connect(_ui->rarPathButton,     &QAbstractButton::clicked, this, &PostingWidget::onRarPathClicked);

    connect(_ui->nzbFileButton,     &QAbstractButton::clicked, this, &PostingWidget::onNzbFileClicked);

    connect(_ui->aboutButton,       &QAbstractButton::clicked, _ngPost, &NgPost::onAboutClicked);
    connect(_ui->donateButton,      &QAbstractButton::clicked, _ngPost, &NgPost::onDonation);


    onCompressCB(false);
}

void PostingWidget::genNameAndPassword(bool genName, bool genPass, bool doPar2, bool useRarMax)
{
    _ui->compressCB->setChecked(true);
    if (genName)
        onGenCompressName();
    if (genPass)
    {
        _ui->nzbPassCB->setChecked(true);
        onGenNzbPassword();
    }
    _ui->rarMaxCB->setChecked(useRarMax);

    if (doPar2)
        _ui->par2CB->setChecked(true);

    _ui->keepRarCB->setChecked(_ngPost->_keepRar);
}



void PostingWidget::udatePostingParams()
{
    if (!_ui->nzbFileEdit->text().isEmpty())
    {
        QFileInfo nzb(_ui->nzbFileEdit->text());
        if (!nzb.absolutePath().isEmpty())
            _ngPost->_nzbPath = nzb.absolutePath();
        _ngPost->setNzbName(nzb);
    }

    // fetch compression settings
    _ngPost->_tmpPath    = _ui->compressPathEdit->text();
    _ngPost->_doCompress = _ui->compressCB->isChecked();
    _ngPost->_rarPath    = _ui->rarEdit->text();
    _ngPost->_rarName    = _ui->compressNameEdit->text();
    if (_ui->nzbPassCB->isEnabled())
        _ngPost->_rarPass = _ui->nzbPassEdit->text().toLocal8Bit();
    else
        _ngPost->_rarPass = QString();
    _ngPost->_lengthName = static_cast<uint>(_ui->nameLengthSB->value());
    _ngPost->_lengthPass = static_cast<uint>(_ui->passLengthSB->value());
    uint val = 0;
    bool ok  = true;
    _ngPost->_rarSize = 0;
    if (!_ui->rarSizeEdit->text().isEmpty())
    {
        val = _ui->rarSizeEdit->text().toUInt(&ok);
        if (ok)
            _ngPost->_rarSize = val;
    }
    _ngPost->_useRarMax = _ui->rarMaxCB->isChecked();

    // fetch par2 settings
    _ngPost->_doPar2  = _ui->par2CB->isChecked();
    _ngPost->_par2Pct = 0;
    if (_ui->par2CB->isChecked())
        _ngPost->_par2Pct = static_cast<uint>(_ui->redundancySB->value());

    _ngPost->_keepRar = _ui->keepRarCB->isChecked();
}

void PostingWidget::retranslate()
{
    _ui->retranslateUi(this);
    _ui->rarMaxCB->setToolTip(tr("limit the number of archive volume to %1 (cf config RAR_MAX)").arg(_ngPost->_rarMax));
    _ui->redundancySB->setToolTip(tr("Using PAR2_ARGS from config file: %1").arg(_ngPost->_par2Args));
    _ui->donateButton->setToolTip(_ngPost->donationTooltip());
}

void PostingWidget::addPath(const QString &path, int currentNbFiles, int isDir)
{
    for (int i = 0 ; i < currentNbFiles ; ++i)
    {
        if (_ui->filesList->item(i)->text() == path)
        {
            qDebug() << "[MainWindow::_addFile] we already have the file " << path;
            return;
        }
    }
    _ui->filesList->addPath(path, isDir);

    QFileInfo fileInfo(path);
    if (_ui->nzbFileEdit->text().isEmpty())
    {
        _ngPost->setNzbName(fileInfo);
        _ui->nzbFileEdit->setText(QString("%1.nzb").arg(_ngPost->nzbPath()));
    }
    if (_ui->compressNameEdit->text().isEmpty())
        _ui->compressNameEdit->setText(_ngPost->_nzbName);
}

bool PostingWidget::_fileAlreadyInList(const QString &fileName, int currentNbFiles) const
{
    for (int i = 0 ; i < currentNbFiles ; ++i)
    {
        if (_ui->filesList->item(i)->text() == fileName)
            return true;
    }
    return false;
}

void PostingWidget::setIDLE()
{
    _ui->postButton->setText(tr("Post Files"));
    _state = STATE::IDLE;
}

void PostingWidget::setPosting()
{
    _hmi->updateJobTab(this, _hmi->sPostingColor, QIcon(_hmi->sPostingIcon), _postingJob->nzbName());
    _ui->postButton->setText(tr("Stop Posting"));
    _state = STATE::POSTING;
}
