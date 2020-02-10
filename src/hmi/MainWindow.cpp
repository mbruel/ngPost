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
#include "PostingWidget.h"
#include "AutoPostWidget.h"
#include "NgPost.h"
#include "nntp/NntpServerParams.h"
#include "nntp/NntpArticle.h"

#include <QDebug>
#include <QProgressBar>
#include <QLabel>
#include <QDesktopWidget>
#include <QMessageBox>


const QColor  MainWindow::sPostingColor = QColor(255,162, 0); // gold (#FFA200)
const QString MainWindow::sPostingIcon  = ":/icons/uploading.png";
const QColor  MainWindow::sPendingColor = Qt::darkBlue;
const QString MainWindow::sPendingIcon  = ":/icons/pending.png";
const QColor  MainWindow::sDoneOKColor  = Qt::darkGreen;
const QString MainWindow::sDoneOKIcon   = ":/icons/ok.png";
const QColor  MainWindow::sDoneKOColor  = Qt::darkRed;
const QString MainWindow::sDoneKOIcon   = ":/icons/ko.png";
const QColor  MainWindow::sArticlesFailedColor  = Qt::darkYellow;


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
    _state(STATE::IDLE),
    _quickJobTab(new PostingWidget(ngPost, this, 1)),
    _autoPostTab(new AutoPostWidget(ngPost, this))
{
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


    _ui->progressBar->setRange(0, 100);
    updateProgressBar(0, 0, "");

    statusBar()->hide();

    QSize screenSize = QDesktopWidget().availableGeometry(this).size();
    resize(screenSize * 0.8);
    setWindowIcon(QIcon(":/icons/ngPost.png"));
    setGeometry((screenSize.width() - width())/2,  (screenSize.height() - height())/2, width(), height());

    connect(_ui->clearLogButton, &QAbstractButton::clicked, _ui->logBrowser, &QTextEdit::clear);
    connect(_ui->debugBox,       &QAbstractButton::toggled, this, &MainWindow::onDebugToggled);
    _ui->debugBox->setChecked(_ngPost->debugMode());


    QTabBar *tabBar = _ui->postTabWidget->tabBar();
    tabBar->setContextMenuPolicy(Qt::CustomContextMenu);

    _ui->postTabWidget->clear();
    _ui->postTabWidget->addTab(_quickJobTab, QIcon(":/icons/quick.png"), QString("%1").arg(_ngPost->sQuickJobName));
    _ui->postTabWidget->addTab(_autoPostTab, QIcon(":/icons/auto.png"), _ngPost->sFolderMonitoringName);
    _ui->postTabWidget->addTab(new QWidget(_ui->postTabWidget), QIcon(":/icons/plus.png"), "New");
    tabBar->setTabToolTip(2, QString("Create a new %1").arg(_ngPost->sQuickJobName));

//    connect(_ui->postTabWidget,           &QTabWidget::currentChanged, this, &MainWindow::onJobTabClicked);
    connect(tabBar, &QTabBar::tabBarClicked,              this, &MainWindow::onJobTabClicked);
    connect(tabBar, &QWidget::customContextMenuRequested, this, &MainWindow::onTabContextMenu);
    connect(tabBar, &QTabBar::tabCloseRequested,          this, &MainWindow::onCloseJob);
    _ui->postTabWidget->setTabsClosable(true);
    _ui->postTabWidget->installEventFilter(this);
//    _ui->postTabWidget->setCurrentIndex(1);


    setJobLabel(1);
}

MainWindow::~MainWindow()
{
    delete _ui;
}

void MainWindow::init()
{
    for (const QString &lang : _ngPost->languages())
        _ui->langCB->addItem(QIcon(QString(":/icons/flag_%1.png").arg(lang.toUpper())), lang.toUpper(), lang);
    _ui->langCB->setCurrentText(_ngPost->_lang.toUpper());
    connect(_ui->langCB, &QComboBox::currentTextChanged, this, &MainWindow::onLangChanged);

    _initServerBox();
    _initPostingBox();
    _quickJobTab->init();
    _autoPostTab->init();
}


void MainWindow::updateProgressBar(uint nbArticlesTotal, uint nbArticlesUploaded, const QString &avgSpeed)
{
    qDebug() << "[MainWindow::updateProgressBar] _nbArticlesUploaded: " << nbArticlesUploaded;
    _ui->progressBar->setValue(static_cast<int>(nbArticlesUploaded));
    _ui->uploadLbl->setText(QString("(%1 / %2) avg speed: %3").arg(
                                nbArticlesUploaded).arg(
                                nbArticlesTotal).arg(
                                avgSpeed));
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

#include <QKeyEvent>
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress && obj == _ui->postTabWidget)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        qDebug() << "[MainWindow] getting key event: " << keyEvent->key();
        int currentTabIdx = _ui->postTabWidget->currentIndex();
        if (currentTabIdx == 1)
            static_cast<AutoPostWidget*>(_ui->postTabWidget->currentWidget())->handleKeyEvent(keyEvent);
        else if (currentTabIdx < _ui->postTabWidget->count() - 1)
            static_cast<PostingWidget*>(_ui->postTabWidget->currentWidget())->handleKeyEvent(keyEvent);
    }
    return QObject::eventFilter(obj, event);
}

#include <QMimeData>
void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *e)
{
    int currentTabIdx = _ui->postTabWidget->currentIndex();
    if (currentTabIdx != 1 && currentTabIdx < _ui->postTabWidget->count() - 1)
        static_cast<PostingWidget*>(_ui->postTabWidget->currentWidget())->handleDropEvent(e);
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    if (_ngPost->hasPostingJobs())
    {
        int res = QMessageBox::question(this,
                                        tr("close while still posting?"),
                                        tr("ngPost is currently posting.\nAre you sure you want to quit?"),
                                        QMessageBox::Yes,
                                        QMessageBox::No);
        if (res == QMessageBox::Yes)
        {
            _ngPost->closeAllPostingJobs();
            event->accept();
        }
        else
            event->ignore();

    }
    else
        event->accept();
}

void MainWindow::changeEvent(QEvent *event)
{
    if(event)
    {
        switch(event->type()) {
        // this event is send if a translator is loaded
        case QEvent::LanguageChange:
            qDebug() << "MainWindow::changeEvent";
            _ui->retranslateUi(this);
            setJobLabel(_ui->postTabWidget->currentIndex());
            _quickJobTab->retranslate();
            _autoPostTab->retranslate();
            for (int i = 2 ; i < _ui->postTabWidget->count() - 1; ++i)
                _getPostWidget(i)->retranslate();
            break;

            // this event is send, if the system, language changes
        default:
            break;
        }
    }
    QMainWindow::changeEvent(event);
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

void MainWindow::onTabContextMenu(const QPoint &point)
{
//    qDebug() << "MainWindow::onTabContextMenu: " << point;
    if (point.isNull())
        return;

//    QTabBar *tabBar = _ui->postTabWidget->tabBar();
//    int tabIndex = tabBar->tabAt(point);
//    PostingWidget *currentPostWidget = _getPostWidget(tabIndex);
    QMenu menu(tr("Quick Tabs Menu"), this);
    QAction *action = menu.addAction(QIcon(":/icons/clear.png"), tr("Close All finished Tabs"), this, &MainWindow::onCloseAllFinishedQuickTabs);
    action->setEnabled(hasFinishedPosts());
    menu.exec(QCursor::pos());
}

bool MainWindow::hasFinishedPosts() const
{
    for (int idx = 2 ; idx < _ui->postTabWidget->count() - 2 ; ++idx)
    {
        PostingWidget *postWidget = _getPostWidget(idx);
        if (postWidget && postWidget->isPostingFinished())
            return true;
    }
    return false;
}

void MainWindow::onCloseAllFinishedQuickTabs()
{
    // go backwards as we may delete the current tab ;)
    for (int idx = _ui->postTabWidget->count() - 2 ; idx > 1  ; --idx)
    {
        PostingWidget *postWidget = _getPostWidget(idx);
        if (postWidget && postWidget->isPostingFinished())
            onCloseJob(idx);
    }
}

void MainWindow::onSetProgressBarRange(int nbArticles)
{
    qDebug() << "MainWindow::onSetProgressBarRange: " << nbArticles;
    _ui->progressBar->setRange(0, nbArticles);
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


void MainWindow::_initPostingBox()
{
    connect(_ui->saveButton,        &QAbstractButton::clicked, this, &MainWindow::onSaveConfig);

    connect(_ui->genPoster,         &QAbstractButton::clicked, this, &MainWindow::onGenPoster);
    connect(_ui->obfuscateMsgIdCB,  &QAbstractButton::toggled, this, &MainWindow::onObfucateToggled);

    _ui->fromEdit->setText(_ngPost->xml2txt(_ngPost->_from.c_str()));
    _ui->groupsEdit->setText(QString::fromStdString(_ngPost->_groups));

    _ui->obfuscateMsgIdCB->setChecked(_ngPost->_obfuscateArticles);

    _ui->articleSizeEdit->setText(QString::number(_ngPost->articleSize()));
    _ui->articleSizeEdit->setValidator(new QIntValidator(100000, 10000000, _ui->articleSizeEdit));

    _ui->nbRetrySB->setRange(0, 15);
    _ui->nbRetrySB->setValue(NntpArticle::nbMaxTrySending());

    _ui->threadSB->setRange(0, 50);
    _ui->threadSB->setValue(_ngPost->_nbThreads);

    _ui->nzbPathEdit->setText(_ngPost->_nzbPath);
    connect(_ui->nzbPathButton, &QAbstractButton::clicked, this, &MainWindow::onNzbPathClicked);
}

void MainWindow::updateServers()
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

void MainWindow::updateParams()
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

    NntpArticle::setNbMaxRetry(static_cast<ushort>(_ui->nbRetrySB->value()));

    _ngPost->_nbThreads = _ui->threadSB->value();
    if (_ngPost->_nbThreads < 1)
        _ngPost->_nbThreads = 1;

    QFileInfo nzbPath(_ui->nzbPathEdit->text());
    if (nzbPath.exists() && nzbPath.isDir() && nzbPath.isWritable())
        _ngPost->_nzbPath = nzbPath.absoluteFilePath();
}

void MainWindow::updateAutoPostingParams()
{
    updateServers();
    updateParams();
    _autoPostTab->udatePostingParams();
}

PostingWidget *MainWindow::addNewQuickTab(int lastTabIdx, const QFileInfoList &files)
{
    if (!lastTabIdx)
        lastTabIdx = _ui->postTabWidget->count() -1;
    PostingWidget *newPostingWidget = new PostingWidget(_ngPost, this, static_cast<uint>(lastTabIdx));
    newPostingWidget->init();
    _ui->postTabWidget->insertTab(lastTabIdx,
                                  newPostingWidget ,
                                  QIcon(":/icons/quick.png"),
                                  QString("%1 #%2").arg(_ngPost->sQuickJobName).arg(lastTabIdx));

    for (const QFileInfo &file : files)
        newPostingWidget->addPath(file.absoluteFilePath(), 0, file.isDir());

    return newPostingWidget;
}

void MainWindow::setTab(QWidget *postWidget)
{
    int nbJob = _ui->postTabWidget->count() -1;
    for (int i = 0 ; i < nbJob ; ++i)
    {
        if (_ui->postTabWidget->widget(i) == postWidget)
        {
            _ui->postTabWidget->setCurrentIndex(i);
            break;
        }
    }
}

void MainWindow::clearJobTab(QWidget *postWidget)
{
    int nbJob = _ui->postTabWidget->count() -1;
    for (int i = 0 ; i < nbJob ; ++i)
    {
        if (_ui->postTabWidget->widget(i) == postWidget)
        {
            QTabBar *bar = _ui->postTabWidget->tabBar();
            bar->setTabToolTip(i, "");
            bar->setTabTextColor(i, Qt::black);
            bar->setTabIcon(i, QIcon(":/icons/quick.png"));
        }
    }
}

void MainWindow::updateJobTab(QWidget *postWidget, const QColor &color, const QIcon &icon, const QString &tooltip)
{
    int nbJob = _ui->postTabWidget->count() -1;
    for (int i = 0 ; i < nbJob ; ++i)
    {
        if (_ui->postTabWidget->widget(i) == postWidget)
        {
            QTabBar *bar = _ui->postTabWidget->tabBar();
            if (!tooltip.isEmpty())
                bar->setTabToolTip(i, tooltip);
            bar->setTabTextColor(i, color);
            bar->setTabIcon(i, icon);
            break;
        }
    }
}

void MainWindow::setJobLabel(int jobNumber)
{
    _ui->jobLabel->setText(QString("<b><u>Post #%1</u></b>").arg(jobNumber != 1 ? QString::number(jobNumber) : "Auto"));
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

PostingWidget *MainWindow::_getPostWidget(int tabIndex) const
{
    if(tabIndex > 1 && tabIndex < _ui->postTabWidget->count() - 1)
        return static_cast<PostingWidget*>(_ui->postTabWidget->widget(tabIndex));
    else
        return nullptr;
}



void MainWindow::onGenPoster()
{
    _ui->fromEdit->setText(_ngPost->randomFrom());
}

void MainWindow::onDebugToggled(bool checked)
{
#ifdef __DEBUG__
    qDebug() << "Debug mode: " << checked;
#endif
    _ngPost->setDebug(checked);
}


void MainWindow::onSaveConfig()
{
    updateServers();
    updateParams();
    int currentTabIdx = _ui->postTabWidget->currentIndex();
    if (currentTabIdx == 0)
        _quickJobTab->udatePostingParams();
    else if (currentTabIdx == 1)
        _autoPostTab->udatePostingParams();
    else
    {
        PostingWidget *postWidget = _getPostWidget(currentTabIdx);
        if (postWidget)
            postWidget->udatePostingParams();
    }
    _ngPost->saveConfig();
}

void MainWindow::onJobTabClicked(int index)
{
//    Q_UNUSED(index)
//    if (index == 1)
//    {
//        QMessageBox box(this);
//        box.setWindowTitle(tr("to be implemented..."));
//        box.setInformativeText( tr("The idea would be to allow to prepare several posting Jobs with compression/par2... while the first job is posting.\n")
//                               +tr("They will then start automatically, one after each other!\n")
//                               +tr("\nAs this feature will require few days of work and that I don't particularly need it, ")
//                               +tr("I'm waiting for some donations and if I get several, I'll definitely code it for you ;)\n"));
//        box.setIcon(QMessageBox::Icon::Information);
//        QAbstractButton* donateButton = box.addButton(tr("Donate"), QMessageBox::ActionRole);
//        donateButton->setIcon(QIcon(":/icons/donate.png"));
//        connect(donateButton, &QAbstractButton::clicked, _ngPost, &NgPost::onDonation);
//        box.addButton(tr("Ok"), QMessageBox::AcceptRole);
//        box.exec();
//        _ui->postTabWidget->setCurrentIndex(0);
//    }

    int nbJob = _ui->postTabWidget->count() -1;
    qDebug() << "Click on tab: " << index << ", count: " << nbJob;
    if (index == nbJob) // click on the last tab
        addNewQuickTab(nbJob);
}

void MainWindow::onCloseJob(int index)
{
    int nbJob = _ui->postTabWidget->count() -1;
    qDebug() << "onCloseJob on tab: " << index << ", count: " << nbJob;
    if (index > 1 && index < nbJob )
    {
        PostingWidget *postWidget = _getPostWidget(index);
        if (postWidget->isPosting())
        {
            QMessageBox::warning(this,
                                 tr("Quick Post is working.."),
                                 tr("The Quick post is currentling uploading.\n Please Stop it before closing it.."));
        }
        else
        {
            _ui->postTabWidget->removeTab(index);
            delete postWidget;

            if (index == nbJob - 1)
                _ui->postTabWidget->setCurrentIndex(_ui->postTabWidget->count() - 2);
        }
    }
}

void MainWindow::toBeImplemented()
{
    QMessageBox::information(nullptr, "To be implemented...", "To be implemented...", QMessageBox::Ok);
}

#include <QFileDialog>
void MainWindow::onNzbPathClicked()
{
    QString path = QFileDialog::getExistingDirectory(
                this,
                tr("Select a Folder"),
                _ui->nzbPathEdit->text(),
                QFileDialog::ShowDirsOnly);

    if (!path.isEmpty())
        _ui->nzbPathEdit->setText(path);
}

void MainWindow::onLangChanged(const QString &lang)
{
    qDebug() << "Changing lang to " << lang;
    _ngPost->changeLanguage(lang.toLower());
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
