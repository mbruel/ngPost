#include "PostingWidget.h"
#include "ui_PostingWidget.h"
#include "MainWindow.h"
#include "AboutNgPost.h"
#include "NgPost.h"
#include "nntp/NntpFile.h"

#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QKeyEvent>
#include <QClipboard>
#include <QMimeData>

PostingWidget::PostingWidget(NgPost *ngPost, MainWindow *hmi) :
    QWidget(hmi),
    _ui(new Ui::PostingWidget), _hmi(hmi),
    _ngPost(ngPost),
    _state(STATE::IDLE)
{
    _ui->setupUi(this);

    init();

    connect(_ui->postButton, &QAbstractButton::clicked, this, &PostingWidget::onPostFiles);
    connect(_ui->nzbPassCB,  &QAbstractButton::toggled, this, &PostingWidget::onNzbPassToggled);
    connect(_ui->genPass,    &QAbstractButton::clicked, this, &PostingWidget::onGenNzbPassword);

}

PostingWidget::~PostingWidget()
{
    delete _ui;
}

void PostingWidget::setFilePosted(NntpFile *nntpFile)
{
    QString filePath = nntpFile->path();
    int nbFiles = _ui->filesList->count();
    for (int i = 0 ; i < nbFiles ; ++i)
    {
        QListWidgetItem *item = _ui->filesList->item(i);
        if (item->text() == filePath)
        {
            Qt::GlobalColor color = Qt::darkGreen;
            int nbArticles = nntpFile->nbArticles(), nbFailed = nntpFile->nbFailedArticles();
            if (nbFailed == 0)
                item->setText(QString("%1 [%2 ok]").arg(filePath).arg(nbArticles));
            else
            {
                item->setText(QString("%1 [%2 err / %3]").arg(filePath).arg(nbFailed).arg(nbArticles));
                if (nbFailed == nbArticles)
                    color = Qt::darkRed;
                else
                    color = Qt::darkYellow;
            }
            item->setForeground(color);
            break;
        }
    }
}

void PostingWidget::onPostFiles()
{
    if (_state == STATE::IDLE)
    {
        if (_thereAreFolders() && !_ui->compressCB->isChecked())
        {
            _ngPost->error(tr("You can't post folders without using compression..."));
            return;
        }


        _hmi->updateServers();
        _hmi->updateParams();
        _udatePostingParams();

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

        _state = STATE::POSTING;
        int nbFiles = 0;
        if (_ui->compressCB->isChecked())
        {
            QStringList filesToCompress;
            for (int i = 0 ; i < _ui->filesList->count() ; ++i)
            {
                QString fileName(_ui->filesList->item(i)->text());
                QFileInfo fileInfo(fileName);
                if (fileInfo.exists())
                    filesToCompress << fileName;
            }

            if ( _ngPost->compressFiles(filesToCompress) == 0 ){

                _ui->filesList->clear();
                for (const QFileInfo & file : _ngPost->_compressDir->entryInfoList(QDir::Files, QDir::Name))
                {
                    _ui->filesList->addPath(file.absoluteFilePath());
                    if (_ngPost->debugMode())
                        _ngPost->_log(QString("  - %1").arg(file.fileName()));

                }

                nbFiles = _createNntpFiles();
            }
        }
        else
            nbFiles = _createNntpFiles();

        if (nbFiles > 0)
        {
            _ui->postButton->setText(tr("Stop Posting"));
            _hmi->setProgressBarRange(0, _ngPost->_nbArticlesTotal);
            _hmi->updateProgressBar();
            if (!_ngPost->startPosting())
                setIDLE();
        }
        else
        {
            _ngPost->_error(tr("No existing file to post..."));
            setIDLE();
        }
    }
    else  if (_state == STATE::POSTING)
    {
        _state = STATE::STOPPING;
        _ngPost->stopPosting();
    }
}

void PostingWidget::onAboutClicked()
{
    AboutNgPost about(_ngPost);
    about.exec();
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
    for (QString &file : files)
        _addPath(file, currentNbFiles);
}

void PostingWidget::onSelectFolderClicked()
{
    QString folder = QFileDialog::getExistingDirectory(
                this,
                tr("Select a Folder"),
                _ngPost->_inputDir,
                QFileDialog::ShowDirsOnly);

    if (!folder.isEmpty())
        _addPath(folder, _ui->filesList->count(), true);
}

void PostingWidget::onClearFilesClicked()
{
    _ui->filesList->clear2();
    _ui->nzbFileEdit->clear();
    _ui->compressNameEdit->clear();
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
                _ngPost->_tmpPath,
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

bool PostingWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        if(obj == _ui->filesList)
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if(keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace)
            {
                for (QListWidgetItem *item : _ui->filesList->selectedItems())
                {
                    qDebug() << "[MainWindow::eventFilter] remove item: " << item->text();
                    _ui->filesList->removeItemWidget2(item);
                    delete item;
                }
            }
            else if (keyEvent->matches(QKeySequence::Paste))
            {
                const QClipboard *clipboard = QApplication::clipboard();
                const QMimeData *mimeData = clipboard->mimeData();
                if (mimeData->hasImage()) {
                    qDebug() << "[MainWindow::eventFilter] try to paste image...";
                } else if (mimeData->hasHtml()) {
                    qDebug() << "[MainWindow::eventFilter] try to paste html: ";
                } else if (mimeData->hasText()) {
                    QString txt = mimeData->text();
                    qDebug() << "[MainWindow::eventFilter] paste text: " << txt;
                    int currentNbFiles = _ui->filesList->count();
                    for (const QString &path : txt.split(QRegularExpression("\n|\r|\r\n")))
                    {
                        QFileInfo fileInfo(path);
                        if (!fileInfo.exists())
                            qDebug() << "[MainWindow::eventFilter] NOT A FILE: " << path;
                        else
                            _addPath(path, currentNbFiles, fileInfo.isDir());
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
                    qDebug() << "[MainWindow::eventFilter] paste urls...";

                } else {
                    qDebug() << "[MainWindow::eventFilter] unknown type...";
                }
            }
        }

    }
    return QObject::eventFilter(obj, event);
}

void PostingWidget::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
}

void PostingWidget::dropEvent(QDropEvent *e)
{
    int currentNbFiles = _ui->filesList->count();
    for (const QUrl &url : e->mimeData()->urls())
    {
        QString fileName = url.toLocalFile();
        _addPath(fileName, currentNbFiles, QFileInfo(fileName).isDir());
    }
}

void PostingWidget::init()
{
    _ui->nzbPassCB->setChecked(false);
    onNzbPassToggled(false);

    _ui->fileListLbl->hide();
    _ui->filesList->setSignature(QString("<pre>%1</pre>").arg(_ngPost->escapeXML(_ngPost->asciiArt())));
    connect(_ui->filesList, &SignedListWidget::rightClick, this, &PostingWidget::onSelectFilesClicked);

    _ui->compressPathEdit->setText(_ngPost->_tmpPath);
    _ui->rarEdit->setText(_ngPost->_rarPath);

    _ui->rarSizeEdit->setText(QString::number(_ngPost->_rarSize));
    _ui->rarSizeEdit->setValidator(new QIntValidator(1, 1000000, _ui->rarSizeEdit));

    if (_ngPost->_par2Args.isEmpty())
    {
        _ui->redundancyEdit->setText(QString::number(_ngPost->_par2Pct));
        _ui->redundancyEdit->setValidator(new QIntValidator(1, 100, _ui->redundancyEdit));
    }
    else
    {
        _ui->redundancyEdit->setEnabled(false);
        _ui->redundancyEdit->setToolTip(tr("Using PAR2_ARGS from config file: %1").arg(_ngPost->_par2Args));
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


    // MB_TODO
    connect(_ui->compressPathButton,&QAbstractButton::clicked, this, &PostingWidget::onCompressPathClicked);
    connect(_ui->rarPathButton,     &QAbstractButton::clicked, this, &PostingWidget::onRarPathClicked);

    connect(_ui->nzbFileButton,     &QAbstractButton::clicked, this, &PostingWidget::onNzbFileClicked);

    connect(_ui->aboutButton,       &QAbstractButton::clicked, this,    &PostingWidget::onAboutClicked);
    connect(_ui->donateButton,      &QAbstractButton::clicked, _ngPost, &NgPost::onDonation);



    onCompressCB(false);
}

int PostingWidget::_createNntpFiles()
{
    QList<QFileInfo> filesToUpload;
    for (int i = 0 ; i < _ui->filesList->count() ; ++i)
    {
        QFileInfo fileInfo(_ui->filesList->item(i)->text());
        if (fileInfo.exists() && fileInfo.isFile())
            filesToUpload << fileInfo;
    }
    _ngPost->_initPosting(filesToUpload);
    return filesToUpload.size();
}

void PostingWidget::_udatePostingParams()
{
    if (!_ui->nzbFileEdit->text().isEmpty())
    {
        QFileInfo nzb(_ui->nzbFileEdit->text());
        if (!nzb.absolutePath().isEmpty())
            _ngPost->_nzbPath = nzb.absolutePath();
        _ngPost->_nzbName = nzb.completeBaseName();
    }

    if (_ui->nzbPassEdit->text().isEmpty())
        _ngPost->_meta.remove("password");
    else
        _ngPost->_meta["password"] = _ui->nzbPassEdit->text();

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

    // fetch par2 settings
    _ngPost->_doPar2  = _ui->par2CB->isChecked();
    _ngPost->_par2Pct = 0;
    if (_ui->par2CB->isChecked() && !_ui->redundancyEdit->text().isEmpty())
    {
        val = _ui->redundancyEdit->text().toUInt(&ok);
        if (ok)
            _ngPost->_par2Pct = val;
    }
}

void PostingWidget::_addPath(const QString &path, int currentNbFiles, int isDir)
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

    if (_ui->nzbFileEdit->text().isEmpty())
    {
        QFileInfo fileInfo(path);
        _ngPost->_nzbName = QString("%1.nzb").arg(fileInfo.completeBaseName());
        _ui->nzbFileEdit->setText(_ngPost->nzbPath());
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

bool PostingWidget::_thereAreFolders() const
{
    int currentNbFiles = _ui->filesList->count();
    for (int i = 0 ; i < currentNbFiles ; ++i)
    {
        if (QFileInfo(_ui->filesList->item(i)->text()).isDir())
            return true;
    }
    return false;
}

void PostingWidget::setIDLE()
{
    _ui->postButton->setText(tr("Post Files"));
    _state = STATE::IDLE;
}
