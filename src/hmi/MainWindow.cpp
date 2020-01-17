//========================================================================
//
// Copyright (C) 2019 Matthieu Bruel <Matthieu.Bruel@gmail.com>
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

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "NgPost.h"
#include <QDebug>
#include <QMimeData>
#include <QDragEnterEvent>
#include "nntp/NntpServerParams.h"
#include "nntp/NntpArticle.h"
#include <QProgressBar>
#include <QLabel>
#include <QDesktopWidget>
#include <QMessageBox>


const QStringList  MainWindow::sServerListHeaders = {
    tr("Host (name or IP)"),
    tr("Port"),
    tr("SSL"),
    tr("Connections"),
    tr("Username"),
    tr("Password"),
    "" // for the delete button
};
const QVector<int> MainWindow::sServerListSizes   = {200, 50, 30, 100, 150, 150, sDeleteColumnWidth};

MainWindow::MainWindow(NgPost *ngPost, QWidget *parent) :
    QMainWindow(parent),
    _ui(new Ui::MainWindow),
    _ngPost(ngPost),
    _state(STATE::IDLE)
{
    qApp->installEventFilter(this);
    setAcceptDrops(true);


    _ui->setupUi(this);

    _ui->serverBox->setTitle(tr("Servers"));
    _ui->fileBox->setTitle(tr("Files"));
    _ui->postingBox->setTitle(tr("Parameters"));
    _ui->logBox->setTitle(tr("Posting Log"));

    _ui->serverBox->setStyleSheet(sGroupBoxStyle);
    _ui->fileBox->setStyleSheet(sGroupBoxStyle);
    _ui->postingBox->setStyleSheet(sGroupBoxStyle);
    _ui->logBox->setStyleSheet(sGroupBoxStyle);

    _ui->hSplitter->setStretchFactor(0, 1);
    _ui->hSplitter->setStretchFactor(1, 1);

    _ui->vSplitter->setStretchFactor(0, 1);
    _ui->vSplitter->setStretchFactor(1, 3);
    _ui->vSplitter->setCollapsible(1, false);

    _ui->postSplitter->setStretchFactor(0, 3);
    _ui->postSplitter->setStretchFactor(1, 1);
    _ui->postSplitter->setCollapsible(0, false);


    connect(_ui->postButton, &QAbstractButton::clicked, this, &MainWindow::onPostFiles);

    _ui->progressBar->setRange(0, 100);
    updateProgressBar();

    statusBar()->hide();

    QSize screenSize = QDesktopWidget().availableGeometry(this).size();
    resize(screenSize * 0.8);
    setWindowIcon(QIcon(":/icons/ngPost.png"));
    setGeometry((screenSize.width() - width())/2,  (screenSize.height() - height())/2, width(), height());

    connect(_ui->clearLogButton, &QAbstractButton::clicked, _ui->logBrowser, &QTextEdit::clear);
    connect(_ui->debugBox,       &QAbstractButton::toggled, this, &MainWindow::onDebugToggled);
    _ui->debugBox->setChecked(_ngPost->debugMode());
}

MainWindow::~MainWindow()
{
    delete _ui;
}

void MainWindow::init()
{
    _initServerBox();
    _initFilesBox();
    _initPostingBox();
}

void MainWindow::setIDLE()
{
    _ui->postButton->setText(tr("Post Files"));
    _state = STATE::IDLE;
}

void MainWindow::updateProgressBar()
{
    qDebug() << "[MainWindow::updateProgressBar] _nbArticlesUploaded: " << _ngPost->_nbArticlesUploaded;
    _ui->progressBar->setValue(_ngPost->_nbArticlesUploaded);
    _ui->uploadLbl->setText(QString("(%1 / %2) avg speed: %3").arg(
                                _ngPost->_nbArticlesUploaded).arg(
                                _ngPost->_nbArticlesTotal).arg(
                                _ngPost->avgSpeed()));
}

#include "nntp/NntpFile.h"
void MainWindow::setFilePosted(NntpFile *nntpFile)
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

void MainWindow::log(const QString &aMsg, bool newline) const
{
    if (newline)
        _ui->logBrowser->append(aMsg);
    else
    {
        _ui->logBrowser->insertPlainText(aMsg);
        _ui->logBrowser->moveCursor(QTextCursor::End);
    }
}

void MainWindow::logError(const QString &error) const
{
    _ui->logBrowser->append(QString("<font color='red'>%1</font><br/>\n").arg(error));
}

#include <QDir>
void MainWindow::onPostFiles()
{
    if (_state == STATE::IDLE)
    {
        if (_thereAreFolders() && !_ui->compressCB->isChecked())
        {
            _ngPost->error(tr("You can't post folders without using compression..."));
            return;
        }


        _updateServers();
        _updateParams();

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
            _ui->progressBar->setRange(0, _ngPost->_nbArticlesTotal);
            updateProgressBar();
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


#include "CheckBoxCenterWidget.h"
void MainWindow::onAddServer()
{
    _addServer(nullptr);
}

void MainWindow::onDelServer()
{
    QObject *delButton = sender();
    int row = _serverRow(delButton);
    if (row < _ui->serversTable->rowCount())
        _ui->serversTable->removeRow(row);

    NntpServerParams *serverParam = static_cast<NntpServerParams*>(delButton->property("server").value<void*>());
    if (serverParam)
    {
        _ngPost->removeNntpServer(serverParam);
        delete serverParam;
    }
}

void MainWindow::onObfucateToggled(bool checked)
{
    _ui->fromEdit->setEnabled(!checked);
    _ui->genPoster->setEnabled(!checked);
}



void MainWindow::_initServerBox()
{
    _ui->serversTable->verticalHeader()->hide();
    _ui->serversTable->setColumnCount(sServerListHeaders.size());
    _ui->serversTable->setHorizontalHeaderLabels(sServerListHeaders);

    int width = 2, col = 0;
    for (int size : sServerListSizes)
    {
        _ui->serversTable->setColumnWidth(col++, size);
        width += size;
    }
//    _ui->serversTable->setMaximumWidth(width);

    connect(_ui->addServerButton,   &QAbstractButton::clicked, this, &MainWindow::onAddServer);

    for (NntpServerParams *srv : _ngPost->_nntpServers)
        _addServer(srv);
}

void MainWindow::_initFilesBox()
{
    _ui->fileListLbl->hide();
    _ui->filesList->setSignature(QString("<pre>%1</pre>").arg(_ngPost->escapeXML(_ngPost->asciiArt())));
    connect(_ui->filesList, &SignedListWidget::rightClick, this, &MainWindow::onSelectFilesClicked);

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

    connect(_ui->selectFilesButton, &QAbstractButton::clicked, this, &MainWindow::onSelectFilesClicked);
    connect(_ui->selectFolderButton,&QAbstractButton::clicked, this, &MainWindow::onSelectFolderClicked);
    connect(_ui->clearFilesButton,  &QAbstractButton::clicked, this, &MainWindow::onClearFilesClicked);

    connect(_ui->compressCB,        &QAbstractButton::toggled, this, &MainWindow::onCompressCB);
    connect(_ui->genCompressName,   &QAbstractButton::clicked, this, &MainWindow::onGenCompressName);


    // MB_TODO
    connect(_ui->compressPathButton,&QAbstractButton::clicked, this, &MainWindow::onCompressPathClicked);
    connect(_ui->rarPathButton,     &QAbstractButton::clicked, this, &MainWindow::onRarPathClicked);

    connect(_ui->nzbFileButton,     &QAbstractButton::clicked, this, &MainWindow::onNzbFileClicked);

    connect(_ui->aboutButton,       &QAbstractButton::clicked, this, &MainWindow::onAboutClicked);
    connect(_ui->donateButton,      &QAbstractButton::clicked, _ngPost, &NgPost::onDonation);



    onCompressCB(false);
}

void MainWindow::_initPostingBox()
{
    //MB_TODO
    connect(_ui->saveButton,        &QAbstractButton::clicked, this, &MainWindow::onSaveConfig);

    connect(_ui->genPoster,         &QAbstractButton::clicked, this, &MainWindow::onGenPoster);
    connect(_ui->nzbPassCB,         &QAbstractButton::toggled, this, &MainWindow::onNzbPassToggled);
    connect(_ui->genPass,           &QAbstractButton::clicked, this, &MainWindow::onGenNzbPassword);
    connect(_ui->obfuscateMsgIdCB,  &QAbstractButton::toggled, this, &MainWindow::onObfucateToggled);

    _ui->fromEdit->setText(_ngPost->xml2txt(_ngPost->_from.c_str()));
    _ui->nzbPassCB->setChecked(false);
    onNzbPassToggled(false);

    _ui->groupsEdit->setText(QString::fromStdString(_ngPost->_groups));

    _ui->obfuscateMsgIdCB->setChecked(_ngPost->_obfuscateArticles);

    _ui->articleSizeEdit->setText(QString::number(_ngPost->articleSize()));
    _ui->articleSizeEdit->setValidator(new QIntValidator(100000, 10000000, _ui->articleSizeEdit));

    _ui->nbRetryEdit->setText(QString::number(NntpArticle::nbMaxTrySending()));
    _ui->nbRetryEdit->setValidator(new QIntValidator(0, 100, _ui->nbRetryEdit));

    _ui->threadEdit->setText(QString::number(_ngPost->_nbThreads));
    _ui->threadEdit->setValidator(new QIntValidator(1, 100, _ui->threadEdit));

}

void MainWindow::_updateServers()
{
    qDeleteAll(_ngPost->_nntpServers);
    _ngPost->_nntpServers.clear();

    int nbRows = _ui->serversTable->rowCount();
    for (int row = 0 ; row < nbRows; ++row)
    {
        int col = 0;
        QLineEdit *hostEdit = static_cast<QLineEdit*>(_ui->serversTable->cellWidget(row, col++));
        if (hostEdit->text().isEmpty())
            continue;

        QLineEdit *portEdit = static_cast<QLineEdit*>(_ui->serversTable->cellWidget(row, col++));
        CheckBoxCenterWidget *sslCb = static_cast<CheckBoxCenterWidget*>(_ui->serversTable->cellWidget(row, col++));
        QLineEdit *nbConsEdit = static_cast<QLineEdit*>(_ui->serversTable->cellWidget(row, col++));
        QLineEdit *userEdit = static_cast<QLineEdit*>(_ui->serversTable->cellWidget(row, col++));
        QLineEdit *passEdit = static_cast<QLineEdit*>(_ui->serversTable->cellWidget(row, col++));

        NntpServerParams *srvParams = new NntpServerParams(hostEdit->text(), portEdit->text().toUShort());
        srvParams->useSSL = sslCb->isChecked();
        srvParams->nbCons = nbConsEdit->text().toInt();
        QString user = userEdit->text();
        if (!user.isEmpty())
        {
            srvParams->auth = true;
            srvParams->user = user.toStdString();
            srvParams->pass = passEdit->text().toStdString();
        }

        _ngPost->_nntpServers << srvParams;
    }
}

void MainWindow::_updateParams()
{
    QString from = _ui->fromEdit->text();
    if (!from.isEmpty())
    {
        QRegularExpression email("\\w+@\\w+\\.\\w+");
        if (!email.match(from).hasMatch())
            from += QString("@%1.com").arg(_ngPost->_articleIdSignature.c_str());
        _ngPost->_from   = _ngPost->escapeXML(from).toStdString();
    }

    _ngPost->updateGroups(_ui->groupsEdit->toPlainText());
    _ngPost->_groups = _ui->groupsEdit->toPlainText().toStdString();

    _ngPost->_obfuscateArticles  = _ui->obfuscateMsgIdCB->isChecked();

    bool ok = false;
    uint articleSize = _ui->articleSizeEdit->text().toUInt(&ok);
    if (ok)
        NgPost::sArticleSize = articleSize;

    ushort nbMaxRetry = _ui->nbRetryEdit->text().toUShort(&ok);
    if (ok)
        NntpArticle::setNbMaxRetry(nbMaxRetry);

    _ngPost->_nbThreads = _ui->threadEdit->text().toInt();
    if (_ngPost->_nbThreads < 1)
        _ngPost->_nbThreads = 1;

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

int MainWindow::_createNntpFiles()
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

void MainWindow::_addServer(NntpServerParams *serverParam)
{
    int nbRows = _ui->serversTable->rowCount(), col = 0;
    _ui->serversTable->setRowCount(nbRows+1);

    QLineEdit *hostEdit = new QLineEdit(_ui->serversTable);
    if (serverParam)
        hostEdit->setText(serverParam->host);
    hostEdit->setFrame(false);
    _ui->serversTable->setCellWidget(nbRows, col++, hostEdit);

    QLineEdit *portEdit = new QLineEdit(_ui->serversTable);
    portEdit->setFrame(false);
    portEdit->setValidator(new QIntValidator(1, 99999, portEdit));
    portEdit->setText(QString::number(serverParam ? serverParam->port : sDefaultServerPort));
    portEdit->setAlignment(Qt::AlignCenter);
    _ui->serversTable->setCellWidget(nbRows, col++, portEdit);

    _ui->serversTable->setCellWidget(nbRows, col++,
                                     new CheckBoxCenterWidget(_ui->serversTable,
                                                              serverParam ? serverParam->useSSL : sDefaultServerSSL));

    QLineEdit *nbConsEdit = new QLineEdit(_ui->serversTable);
    nbConsEdit->setFrame(false);
    nbConsEdit->setValidator(new QIntValidator(1, 99999, nbConsEdit));
    nbConsEdit->setText(QString::number(serverParam ? serverParam->nbCons : sDefaultConnections));
    nbConsEdit->setAlignment(Qt::AlignCenter);
    _ui->serversTable->setCellWidget(nbRows, col++, nbConsEdit);


    QLineEdit *userEdit = new QLineEdit(_ui->serversTable);
    if (serverParam)
        userEdit->setText(serverParam->user.c_str());
    userEdit->setFrame(false);
    _ui->serversTable->setCellWidget(nbRows, col++, userEdit);

    QLineEdit *passEdit = new QLineEdit(_ui->serversTable);
    passEdit->setEchoMode(QLineEdit::EchoMode::PasswordEchoOnEdit);
    if (serverParam)
        passEdit->setText(serverParam->pass.c_str());
    passEdit->setFrame(false);
    _ui->serversTable->setCellWidget(nbRows, col++, passEdit);

    QPushButton *delButton = new QPushButton(_ui->serversTable);
    delButton->setProperty("server", QVariant::fromValue(static_cast<void*>(serverParam)));
    delButton->setIcon(QIcon(":/icons/clear.png"));
    delButton->setMaximumWidth(sDeleteColumnWidth);
    connect(delButton, &QAbstractButton::clicked, this, &MainWindow::onDelServer);
    _ui->serversTable->setCellWidget(nbRows, col++, delButton);
}

int MainWindow::_serverRow(QObject *delButton)
{
    int nbRows = _ui->serversTable->rowCount(), delCol =_ui->serversTable->columnCount()-1;
    for (int row = 0 ; row < nbRows; ++row)
    {
        if (_ui->serversTable->cellWidget(row, delCol) == delButton)
            return row;
    }
    return nbRows;
}

void MainWindow::_addPath(const QString &path, int currentNbFiles, int isDir)
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

bool MainWindow::_fileAlreadyInList(const QString &fileName, int currentNbFiles) const
{
    for (int i = 0 ; i < currentNbFiles ; ++i)
    {
        if (_ui->filesList->item(i)->text() == fileName)
            return true;
    }
    return false;
}

bool MainWindow::_thereAreFolders() const
{
    int currentNbFiles = _ui->filesList->count();
    for (int i = 0 ; i < currentNbFiles ; ++i)
    {
        if (QFileInfo(_ui->filesList->item(i)->text()).isDir())
            return true;
    }
    return false;
}

void MainWindow::onGenPoster()
{
    _ui->fromEdit->setText(_ngPost->randomFrom());
}

void MainWindow::onNzbPassToggled(bool checked)
{
    _ui->nzbPassEdit->setEnabled(checked);
    _ui->passLengthSB->setEnabled(checked);
    _ui->genPass->setEnabled(checked);
}

void MainWindow::onGenNzbPassword()
{
    _ui->nzbPassEdit->setText(_ngPost->randomPass(static_cast<uint>(_ui->passLengthSB->value())));
}

void MainWindow::onDebugToggled(bool checked)
{
#ifdef __DEBUG__
    qDebug() << "Debug mode: " << checked;
#endif
    _ngPost->setDebug(checked);
}


#include "AboutNgPost.h"
void MainWindow::onAboutClicked()
{
    AboutNgPost about(_ngPost);
    about.exec();
}

void MainWindow::onSaveConfig()
{
    _updateServers();
    _updateParams();
    _ngPost->saveConfig();
}

void MainWindow::toBeImplemented()
{
    QMessageBox::information(nullptr, "To be implemented...", "To be implemented...", QMessageBox::Ok);
}


#include <QKeyEvent>
#include <QClipboard>
#include <QDir>
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
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


void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *e)
{
    int currentNbFiles = _ui->filesList->count();
    for (const QUrl &url : e->mimeData()->urls())
    {
        QString fileName = url.toLocalFile();
        _addPath(fileName, currentNbFiles, QFileInfo(fileName).isDir());
    }
}

#include <QFileDialog>
//#include <QMessageBox>
void MainWindow::onSelectFilesClicked()
{
    //    QMessageBox::warning(nullptr,
    //                    QCoreApplication::translate("Cosi7Application", "Pdf viewer needed..."),
    //                    QCoreApplication::translate("Cosi7Application", "Please install a pdf viewer to be able to browse the manual."),
    //                    QMessageBox::Ok);

    QStringList files = QFileDialog::getOpenFileNames(
                this,
                tr("Select one or more files to Post"),
                _ngPost->_inputDir);

    int currentNbFiles = _ui->filesList->count();
    for (QString &file : files)
        _addPath(file, currentNbFiles);
}

void MainWindow::onSelectFolderClicked()
{
    QString folder = QFileDialog::getExistingDirectory(
                this,
                tr("Select a Folder"),
                _ngPost->_inputDir,
                QFileDialog::ShowDirsOnly);

    if (!folder.isEmpty())
        _addPath(folder, _ui->filesList->count(), true);
}

void MainWindow::onClearFilesClicked()
{
    _ui->filesList->clear2();
    _ui->nzbFileEdit->clear();
    _ui->compressNameEdit->clear();
}

void MainWindow::onCompressCB(bool checked)
{
    _ui->compressNameEdit->setEnabled(checked);
    _ui->nameLengthSB->setEnabled(checked);
    _ui->genCompressName->setEnabled(checked);
    _ui->par2CB->setEnabled(checked);
}

void MainWindow::onGenCompressName()
{
    _ui->compressNameEdit->setText(_ngPost->randomPass(static_cast<uint>(_ui->nameLengthSB->value())));
}

void MainWindow::onCompressPathClicked()
{
    QString path = QFileDialog::getExistingDirectory(
                this,
                tr("Select a Folder"),
                _ngPost->_tmpPath,
                QFileDialog::ShowDirsOnly);

    if (!path.isEmpty())
        _ui->compressPathEdit->setText(path);
}

void MainWindow::onNzbFileClicked()
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

void MainWindow::onRarPathClicked()
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

const QString MainWindow::sGroupBoxStyle =  "\
        QGroupBox {\
        font: bold; \
        border: 1px solid silver;\
        border-radius: 6px;\
        margin-top: 6px;\
        }\
        QGroupBox::title {\
        subcontrol-origin:  margin;\
        left: 7px;\
        padding: 0 5px 0 5px;\
        }";
