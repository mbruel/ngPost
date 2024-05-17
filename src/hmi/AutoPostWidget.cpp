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

#include "AutoPostWidget.h"
#include "FoldersMonitorForNewFiles.h"
#include "MainWindow.h"
#include "NgPost.h"
#include "PostingWidget.h"
#include "ui_AutoPostWidget.h"
#include <QDebug>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMimeData>

AutoPostWidget::AutoPostWidget(NgPost *ngPost, MainWindow *hmi)
    : QWidget(hmi)
    , _ui(new Ui::AutoPostWidget)
    , _hmi(hmi)
    , _ngPost(ngPost)
    , _isMonitoring(false)
    , _currentPostIdx(1)
{
    _ui->setupUi(this);
    _ui->filesList->setSignature(QString("<pre>%1</pre>").arg(_ngPost->escapeXML(_ngPost->asciiArt())));
    connect(_ui->filesList, &SignedListWidget::rightClick, this, &AutoPostWidget::onSelectFilesClicked);
}

AutoPostWidget::~AutoPostWidget() { delete _ui; }

void AutoPostWidget::init()
{
    _ui->rarMaxCB->setChecked(_ngPost->_useRarMax);

    _ui->keepRarCB->setChecked(_ngPost->_keepRar);

    _ui->compressPathEdit->setText(_ngPost->_tmpPath);
    _ui->rarEdit->setText(_ngPost->_rarPath);

    _ui->rarSizeEdit->setText(QString::number(_ngPost->_rarSize));
    _ui->rarSizeEdit->setValidator(new QIntValidator(1, 1000000, _ui->rarSizeEdit));

    _ui->redundancySB->setRange(0, 100);
    _ui->redundancySB->setValue(static_cast<int>(_ngPost->_par2Pct));
    _ui->redundancySB->setEnabled(_ngPost->_par2Args.isEmpty());

    _ui->autoDirEdit->setText(_ngPost->_inputDir);
    _ui->nameLengthSB->setRange(5, 50);
    _ui->nameLengthSB->setValue(static_cast<int>(_ngPost->_lengthName));
    _ui->passLengthSB->setRange(5, 50);
    _ui->passLengthSB->setValue(static_cast<int>(_ngPost->_lengthPass));

    _ui->filesList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(_ui->compressPathButton, &QAbstractButton::clicked, this, &AutoPostWidget::onCompressPathClicked);
    connect(_ui->rarPathButton, &QAbstractButton::clicked, this, &AutoPostWidget::onRarPathClicked);
    connect(_ui->autoDirButton, &QAbstractButton::clicked, this, &AutoPostWidget::onSelectAutoDirClicked);
    connect(_ui->scanAutoDirButton, &QAbstractButton::clicked, this, &AutoPostWidget::onScanAutoDirClicked);
    connect(_ui->compressCB, &QAbstractButton::toggled, this, &AutoPostWidget::onCompressToggled);
    connect(_ui->par2CB, &QAbstractButton::toggled, this, &AutoPostWidget::onGenPar2Toggled);

    setPackingAuto(_ngPost->_packAuto, _ngPost->_packAutoKeywords);
    if (_ngPost->_keepRar)
        _ui->keepRarCB->setChecked(true);
    _ui->startJobsCB->setChecked(true);

    _ui->latestFilesFirstCB->setChecked(true);

    connect(_ui->postButton, &QAbstractButton::clicked, this, &AutoPostWidget::onGenQuickPosts);
    connect(_ui->aboutButton, &QAbstractButton::clicked, _ngPost, &NgPost::onAboutClicked);
    connect(_ui->donateButton, &QAbstractButton::clicked, _ngPost, &NgPost::onDonation);
    connect(_ui->btcDonate, &QAbstractButton::clicked, _ngPost, &NgPost::onDonationBTC);

    connect(_ui->monitorButton, &QAbstractButton::clicked, this, &AutoPostWidget::onMonitoringClicked);

    connect(_ui->delFilesCB, &QAbstractButton::toggled, this, &AutoPostWidget::onDelFilesToggled);

    _ui->addMonitoringFolderButton->setEnabled(false);
    connect(_ui->addMonitoringFolderButton,
            &QAbstractButton::clicked,
            this,
            &AutoPostWidget::onAddMonitoringFolder);

    _ui->extensionFilterEdit->setText(
            _ngPost->_monitorExtensions.isEmpty() ? "" : _ngPost->_monitorExtensions.join(","));
    _ui->dirAllowedCB->setChecked(!_ngPost->_monitorIgnoreDir);
}

void AutoPostWidget::onGenQuickPosts()
{
    bool compress = _ui->compressCB->isChecked();

    QFileInfoList files;
    for (int i = 0; i < _ui->filesList->count(); ++i)
    {
        QFileInfo fileInfo(_ui->filesList->item(i)->text());
        if (fileInfo.exists())
            files << fileInfo;
        if (!compress && fileInfo.isDir())
        {
            _ngPost->error(tr("You can't use auto posting without compression on folders... (%1)")
                                   .arg(fileInfo.fileName()));
            return;
        }
    }
    if (files.isEmpty())
    {
        QMessageBox::warning(nullptr, tr("Nothing to post..."), tr("There is nothing to post!\n\
Press the Scan button and remove what you don't want to post ;)\n\
(To remove files, select in the list and press DEL or BackSpace)"));
        return;
    }

    _hmi->updateServers();
    _hmi->updateParams();
    udatePostingParams();

    bool startPost = _ui->startJobsCB->isChecked(), useRarMax = _ui->rarMaxCB->isChecked();
    for (QFileInfo const &file : files)
    {
        PostingWidget *quickPostWidget = _hmi->addNewQuickTab(0, { file });
        quickPostWidget->init();
        quickPostWidget->genNameAndPassword(_ngPost->_genName, _ngPost->_genPass, _ngPost->_doPar2, useRarMax);

        if (startPost)
            quickPostWidget->postFiles(false);
    }
}

void AutoPostWidget::onCompressPathClicked()
{
    QString path = QFileDialog::getExistingDirectory(
            this, tr("Select a Folder"), _ui->compressPathEdit->text(), QFileDialog::ShowDirsOnly);

    if (!path.isEmpty())
        _ui->compressPathEdit->setText(path);
}

void AutoPostWidget::onRarPathClicked()
{
    QString path = QFileDialog::getOpenFileName(
            this, tr("Select rar executable"), QFileInfo(_ngPost->_rarPath).absolutePath());

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
            this, tr("Select a Folder"), _ui->autoDirEdit->text(), QFileDialog::ShowDirsOnly);

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
        for (QFileInfo const &file :
             autoDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, sort))
            _ui->filesList->addPathIfNotInList(file.absoluteFilePath(), 0, file.isDir());
    }
    else
        QMessageBox::warning(nullptr,
                             tr("No auto directory selected..."),
                             tr("There is no auto directory!\nPlease select one."));
}

void AutoPostWidget::onMonitoringClicked()
{
    if (_isMonitoring)
    {
        if (_ngPost->hasMonitoringPostingJobs())
        {
            int res = QMessageBox::question(this,
                                            tr("Ongoing Monitoring post"),
                                            tr("There are still ongoing or pending Monitoring Posts.\n We're "
                                               "going to stop all of them.\nAre you sure you want to proceed?"));
            if (res == QMessageBox::No)
                return;
            else
                _ngPost->closeAllMonitoringJobs();
        }
        _ui->filesList->clear2();
        _ngPost->_stopMonitoring();
        _ui->monitorButton->setText(tr("Monitor Folder"));
    }
    else
    {
        _currentPostIdx = 1;
        _ui->filesList->clear2();
        QString   folderPath = _ui->autoDirEdit->text();
        QFileInfo fi(folderPath);
        if (!fi.exists() || !fi.isDir() || !fi.isReadable())
        {
            QMessageBox::warning(this,
                                 tr("Error accessing Auto Dir..."),
                                 tr("The auto directory must exist and be readable..."));
            return;
        }
        if (!_ui->compressCB->isChecked())
        {
            QMessageBox::warning(
                    this,
                    tr("To be implemented..."),
                    tr("You can't monitor a folder without compression using the GUI...\nIt's possible in "
                       "command line if MONITOR_IGNORE_DIR is enabled in your configuration file."));
            return;
        }
        _ui->filesList->addItem(
                new QListWidgetItem(QIcon(":/icons/monitor.png"), tr("Monitoring %1").arg(folderPath)));
        _ngPost->startFolderMonitoring(folderPath);
        _ui->monitorButton->setText(tr("Stop Monitoring"));
    }

    _ui->compressPathEdit->setEnabled(_isMonitoring);
    _ui->compressPathButton->setEnabled(_isMonitoring);
    _ui->rarEdit->setEnabled(_isMonitoring);
    _ui->rarPathButton->setEnabled(_isMonitoring);
    _ui->rarSizeEdit->setEnabled(_isMonitoring);
    _ui->rarMaxCB->setEnabled(_isMonitoring);
    _ui->redundancySB->setEnabled(_isMonitoring);

    _ui->autoDirEdit->setEnabled(_isMonitoring);
    _ui->autoDirButton->setEnabled(_isMonitoring);
    _ui->latestFilesFirstCB->setEnabled(_isMonitoring);
    _ui->scanAutoDirButton->setEnabled(_isMonitoring);

    _ui->genNameCB->setEnabled(_isMonitoring);
    _ui->nameLengthSB->setEnabled(_isMonitoring);
    _ui->genPassCB->setEnabled(_isMonitoring);
    _ui->passLengthSB->setEnabled(_isMonitoring);

    _ui->par2CB->setEnabled(_isMonitoring);
    //    _ui->delFilesCB->setEnabled(_isMonitoring);

    _ui->startJobsCB->setEnabled(_isMonitoring);
    _ui->postButton->setEnabled(_isMonitoring);

    _isMonitoring = !_isMonitoring;
    _ui->addMonitoringFolderButton->setEnabled(_isMonitoring);

    if (_isMonitoring)
        _ui->filesList->setSelectionMode(QAbstractItemView::NoSelection);
    else
        _ui->filesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
}

void AutoPostWidget::newFileToProcess(QFileInfo const &fileInfo)
{
    QListWidgetItem *newItem =
            new QListWidgetItem(QIcon(fileInfo.isDir() ? ":/icons/folder.png" : ":/icons/file.png"),
                                QString("- %1").arg(fileInfo.absoluteFilePath()));
    newItem->setForeground(_hmi->sPendingColor);
    _ui->filesList->addItem(newItem);
}

void AutoPostWidget::onDelFilesToggled(bool checked)
{
    if (checked)
        QMessageBox::warning(this,
                             tr("Deleting files/folders once posted"),
                             tr("You're about to delete files from your computer once they've been posted!\nUse "
                                "it at your own risk!\nIt will be irreversible..."));
    _ngPost->setDelFilesAfterPosted(checked);
}

void AutoPostWidget::onAddMonitoringFolder()
{
    QString path = QFileDialog::getExistingDirectory(
            this, tr("Select a Monitoring Folder to add"), _ui->autoDirEdit->text(), QFileDialog::ShowDirsOnly);

    if (!path.isEmpty())
    {
        QFileInfo newDir(path);
        if (newDir.exists() && newDir.isDir() && newDir.isReadable())
        {
            _ui->filesList->addItem(new QListWidgetItem(QIcon(":/icons/monitor.png"),
                                                        tr("Monitoring %1").arg(newDir.absoluteFilePath())));
            _ngPost->addMonitoringFolder(newDir.absoluteFilePath());
        }
    }
}

void AutoPostWidget::onCompressToggled(bool checked)
{
    _ui->genNameCB->setEnabled(checked);
    _ui->nameLengthSB->setEnabled(checked);
    _ui->genPassCB->setEnabled(checked);
    _ui->passLengthSB->setEnabled(checked);
    _ui->keepRarCB->setEnabled(checked);
    if (!checked && !_ui->par2CB->isChecked())
        _ui->par2CB->setChecked(true);
}

void AutoPostWidget::onGenPar2Toggled(bool checked)
{
    if (!checked && !_ui->compressCB->isChecked())
        _ui->compressCB->setChecked(true);
}

#include "PostingJob.h"
void AutoPostWidget::onMonitorJobStart()
{
    PostingJob *job = static_cast<PostingJob *>(sender());

    QString srcPath = QString("- %1").arg(job->getFirstOriginalFile());
    int     nbFiles = _ui->filesList->count();
    for (int i = _currentPostIdx; i < nbFiles; ++i)
    {
        QListWidgetItem *item = _ui->filesList->item(i);
        if (item->text() == srcPath)
        {
            item->setForeground(_hmi->sPostingColor);
            _currentPostIdx = i;
            break;
        }
    }
}

void AutoPostWidget::onSelectFilesClicked()
{
    if (!_isMonitoring)
    {
        QStringList files =
                QFileDialog::getOpenFileNames(this, tr("Select one or more files"), _ngPost->_inputDir);

        int currentNbFiles = _ui->filesList->count();
        for (QString const &file : files)
            _ui->filesList->addPathIfNotInList(file, currentNbFiles);
    }
}

void AutoPostWidget::udatePostingParams()
{
    // fetch compression settings
    _ngPost->_doCompress = _ui->compressCB->isChecked();
    _ngPost->_genName    = _ngPost->_doCompress ? _ui->genNameCB->isChecked() : false;
    _ngPost->_genPass    = _ngPost->_doCompress ? _ui->genPassCB->isChecked() : false;
    _ngPost->_doPar2     = _ui->par2CB->isChecked();

    _ngPost->_tmpPath    = _ui->compressPathEdit->text();
    _ngPost->_rarPath    = _ui->rarEdit->text();
    _ngPost->_lengthName = static_cast<uint>(_ui->nameLengthSB->value());
    _ngPost->_lengthPass = static_cast<uint>(_ui->passLengthSB->value());
    uint val             = 0;
    bool ok              = true;
    _ngPost->_rarSize    = 0;
    if (!_ui->rarSizeEdit->text().isEmpty())
    {
        val = _ui->rarSizeEdit->text().toUInt(&ok);
        if (ok)
            _ngPost->_rarSize = val;
    }

    // fetch par2 settings
    if (_ngPost->_par2Args.isEmpty())
        _ngPost->_par2Pct = static_cast<uint>(_ui->redundancySB->value());

    QFileInfo inputDir(_ui->autoDirEdit->text());
    if (inputDir.exists() && inputDir.isDir() && inputDir.isWritable())
        _ngPost->_inputDir = inputDir.absoluteFilePath();

    _ngPost->_monitorExtensions.clear();
    if (!_ui->extensionFilterEdit->text().isEmpty())
    {
        for (QString &extension : _ui->extensionFilterEdit->text().split(","))
        {
            if (!extension.trimmed().isEmpty())
                _ngPost->_monitorExtensions << extension.trimmed();
        }
    }

    _ngPost->_monitorIgnoreDir = !_ui->dirAllowedCB->isChecked();

    _ngPost->_keepRar = _ui->keepRarCB->isChecked();
}

void AutoPostWidget::updateFinishedJob(QString const &path, uint nbArticles, uint nbUploaded, uint nbFailed)
{
    QString srcPath = QString("- %1").arg(path);
    int     nbFiles = _ui->filesList->count();
    for (int i = 1; i < nbFiles; ++i)
    {
        QListWidgetItem *item = _ui->filesList->item(i);
        if (item->text() == srcPath)
        {
            QColor color(_hmi->sDoneOKColor);
            if (nbUploaded > 0 && nbFailed == 0)
                item->setText(QString("%1 [%2 ok]").arg(srcPath).arg(nbArticles));
            else
            {
                item->setText(QString("%1 [%2 err / %3]").arg(srcPath).arg(nbFailed).arg(nbArticles));
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

bool AutoPostWidget::deleteFilesOncePosted() const { return _ui->delFilesCB->isChecked(); }

void AutoPostWidget::retranslate()
{
    _ui->retranslateUi(this);
    _ui->rarMaxCB->setToolTip(
            tr("limit the number of archive volume to %1 (cf config RAR_MAX)").arg(_ngPost->_rarMax));
    _ui->redundancySB->setToolTip(tr("Using PAR2_ARGS from config file: %1").arg(_ngPost->_par2Args));
    _ui->donateButton->setToolTip(_ngPost->donationTooltip());
    _ui->btcDonate->setToolTip(_ngPost->donationBtcTooltip());
    _ui->filesList->setToolTip(
            QString("%1<br/><br/>%2<ul><li>%3</li><li>%4</li><li>%5</li></ul>%6")
                    .arg(tr("You can use the <b>Monitor Mode</b>"))
                    .arg(tr("or <b>Generate Posts</b> by adding files:"))
                    .arg(tr("Drag & Drop files/folders"))
                    .arg(tr("Right Click to add Files"))
                    .arg(tr("Click on the Scan Button"))
                    .arg(tr("Bare in mind you can select items in the list and press DEL to remove them")));
}

void AutoPostWidget::setPackingAuto(bool enabled, QStringList const &keys)
{
    bool compress = false, genName = false, genPass = false, doPar2 = false;
    if (enabled)
    {
        for (auto it = keys.cbegin(), itEnd = keys.cend(); it != itEnd; ++it)
        {
            QString keyWord = (*it).toLower();
            if (keyWord == NgPost::optionName(NgPost::Opt::COMPRESS))
                compress = true;
            else if (keyWord == NgPost::optionName(NgPost::Opt::GEN_NAME))
                genName = true;
            else if (keyWord == NgPost::optionName(NgPost::Opt::GEN_PASS))
                genPass = true;
            else if (keyWord == NgPost::optionName(NgPost::Opt::GEN_PAR2))
                doPar2 = true;
        }
    }
    _ui->compressCB->setChecked(compress);
    _ui->genNameCB->setChecked(genName);
    _ui->genPassCB->setChecked(genPass);
    _ui->par2CB->setChecked(doPar2);
    onCompressToggled(compress);
}

void AutoPostWidget::handleKeyEvent(QKeyEvent *keyEvent)
{
    if (!_isMonitoring)
    {
        qDebug() << "[AutoPostWidget::handleKeyEvent] key event: " << keyEvent->key();
        if (keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace)
        {
            for (QListWidgetItem *item : _ui->filesList->selectedItems())
            {
                qDebug() << "[AutoPostWidget::handleKeyEvent] remove item: " << item->text();
                _ui->filesList->removeItemWidget2(item);
                delete item;
            }
        }
    }
}

void AutoPostWidget::handleDropEvent(QDropEvent *e)
{
    if (!_isMonitoring)
    {
        int currentNbFiles = _ui->filesList->count();
        for (QUrl const &url : e->mimeData()->urls())
        {
            QString fileName = url.toLocalFile();
            _ui->filesList->addPathIfNotInList(fileName, currentNbFiles, QFileInfo(fileName).isDir());
        }
    }
}
