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

#include "AutoPostWidget.h"
#include "ui_AutoPostWidget.h"
#include "PostingWidget.h"
#include "MainWindow.h"
#include "NgPost.h"
#include <QFileDialog>
#include <QDebug>
#include <QMessageBox>
#include <QKeyEvent>
AutoPostWidget::AutoPostWidget(NgPost *ngPost, MainWindow *hmi) :
    QWidget(hmi),
    _ui(new Ui::AutoPostWidget),
    _hmi(hmi),
    _ngPost(ngPost)
{
    _ui->setupUi(this);
    _ui->filesList->setSignature(QString("<pre>%1</pre>").arg(_ngPost->escapeXML(_ngPost->asciiArt())));
}

AutoPostWidget::~AutoPostWidget()
{
    delete _ui;
}

void AutoPostWidget::init()
{
    _ui->donateButton->setToolTip(_ngPost->sDonationTooltip);
    _ui->rarMaxCB->setToolTip(tr("limit the number of archive volume to %1 (cf config RAR_MAX)").arg(_ngPost->_rarMax));
    _ui->rarMaxCB->setChecked(_ngPost->_useRarMax);


    _ui->compressPathEdit->setText(_ngPost->_tmpPath);
    _ui->rarEdit->setText(_ngPost->_rarPath);

    _ui->rarSizeEdit->setText(QString::number(_ngPost->_rarSize));
    _ui->rarSizeEdit->setValidator(new QIntValidator(1, 1000000, _ui->rarSizeEdit));

    if (_ngPost->_par2Args.isEmpty())
    {
        _ui->redundancySB->setRange(0, 100);
        _ui->redundancySB->setValue(static_cast<int>(_ngPost->_par2Pct));
    }
    else
    {
        _ui->redundancySB->setEnabled(false);
        _ui->redundancySB->setToolTip(tr("Using PAR2_ARGS from config file: %1").arg(_ngPost->_par2Args));
    }


    _ui->autoDirEdit->setText(_ngPost->_inputDir);
    _ui->nameLengthSB->setRange(5, 50);
    _ui->nameLengthSB->setValue(static_cast<int>(_ngPost->_lengthName));
    _ui->passLengthSB->setRange(5, 50);
    _ui->passLengthSB->setValue(static_cast<int>(_ngPost->_lengthPass));

    _ui->filesList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(_ui->compressPathButton,&QAbstractButton::clicked, this, &AutoPostWidget::onCompressPathClicked);
    connect(_ui->rarPathButton,     &QAbstractButton::clicked, this, &AutoPostWidget::onRarPathClicked);
    connect(_ui->autoDirButton,     &QAbstractButton::clicked, this, &AutoPostWidget::onSelectAutoDirClicked);
    connect(_ui->scanAutoDirButton, &QAbstractButton::clicked, this, &AutoPostWidget::onScanAutoDirClicked);

    _ui->genNameCB->setChecked(true);
    _ui->genPassCB->setChecked(true);
    _ui->par2CB->setChecked(true);
    _ui->delFilesCB->setEnabled(false);
    _ui->startJobsCB->setChecked(true);

    _ui->latestFilesFirstCB->setChecked(true);

    connect(_ui->postButton,   &QAbstractButton::clicked, this,    &AutoPostWidget::onGenQuickPosts);
    connect(_ui->aboutButton,  &QAbstractButton::clicked, _ngPost, &NgPost::onAboutClicked);
    connect(_ui->donateButton, &QAbstractButton::clicked, _ngPost, &NgPost::onDonation);

}


void AutoPostWidget::onGenQuickPosts()
{
    QFileInfoList files;
    for (int i = 0 ; i < _ui->filesList->count() ; ++i)
    {
        QFileInfo fileInfo(_ui->filesList->item(i)->text());
        if (fileInfo.exists())
            files << fileInfo;
    }
    if (files.isEmpty())
    {
        QMessageBox::warning(nullptr,
                             tr("Nothing to post..."),
                             tr("There is nothing to post!\n\
Press the Scan button and remove what you don't want to post ;)\n\
(To remove files, select in the list and press DEL or BackSpace)"));
        return;
    }

    _hmi->updateServers();
    _hmi->updateParams();
    _udatePostingParams();

    bool genName = _ui->genNameCB->isChecked(),
         genPass = _ui->genPassCB->isChecked(),
          doPar2 = _ui->par2CB->isChecked(),
       startPost = _ui->startJobsCB->isChecked(),
       useRarMax = _ui->rarMaxCB->isChecked();
    for (const QFileInfo &file : files)
    {
        PostingWidget *quickPostWidget = _hmi->addNewQuickTab(0, {file});
        quickPostWidget->init();
        quickPostWidget->genNameAndPassword(genName, genPass, doPar2, useRarMax);

        if (startPost)
            quickPostWidget->onPostFiles();
    }
}


void AutoPostWidget::onCompressPathClicked()
{
    QString path = QFileDialog::getExistingDirectory(
                this,
                tr("Select a Folder"),
                _ui->compressPathEdit->text(),
                QFileDialog::ShowDirsOnly);

    if (!path.isEmpty())
        _ui->compressPathEdit->setText(path);
}

void AutoPostWidget::onRarPathClicked()
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

void AutoPostWidget::onSelectAutoDirClicked()
{
    QString path = QFileDialog::getExistingDirectory(
                this,
                tr("Select a Folder"),
                _ui->autoDirEdit->text(),
                QFileDialog::ShowDirsOnly);

    if (!path.isEmpty())
        _ui->autoDirEdit->setText(path);
}

void AutoPostWidget::onScanAutoDirClicked()
{
    QDir autoDir(_ui->autoDirEdit->text());
    if (autoDir.exists())
    {
        _ui->filesList->clear2();
        QDir::SortFlags sort = _ui->latestFilesFirstCB->isChecked() ? QDir::Time : QDir::Name;
        for (const QFileInfo &file : autoDir.entryInfoList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot, sort))
            _addPath(file.absoluteFilePath(), 0, file.isDir());
    }
    else
        QMessageBox::warning(nullptr,
                             tr("No auto directory selected..."),
                             tr("There is no auto directory!\nPlease select one."));
}


void AutoPostWidget::_addPath(const QString &path, int currentNbFiles, int isDir)
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
}

bool AutoPostWidget::_fileAlreadyInList(const QString &fileName, int currentNbFiles) const
{
    for (int i = 0 ; i < currentNbFiles ; ++i)
    {
        if (_ui->filesList->item(i)->text() == fileName)
            return true;
    }
    return false;
}

void AutoPostWidget::_udatePostingParams()
{
    // fetch compression settings
    _ngPost->_tmpPath    = _ui->compressPathEdit->text();
    _ngPost->_rarPath    = _ui->rarEdit->text();
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

    // fetch par2 settings
    _ngPost->_par2Pct = 0;
    if (_ui->par2CB->isChecked())
        _ngPost->_par2Pct = static_cast<uint>(_ui->redundancySB->value());
}


void AutoPostWidget::handleKeyEvent(QKeyEvent *keyEvent)
{
    qDebug() << "[AutoPostWidget::handleKeyEvent] key event: " << keyEvent->key();
    if(keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace)
    {
        for (QListWidgetItem *item : _ui->filesList->selectedItems())
        {
            qDebug() << "[AutoPostWidget::handleKeyEvent] remove item: " << item->text();
            _ui->filesList->removeItemWidget2(item);
            delete item;
        }
    }
}
