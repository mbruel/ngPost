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

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "PostingWidget.h"
#include "AutoPostWidget.h"
#include "NgPost.h"
#include "CircularImageButton.h"
#include "nntp/NntpServerParams.h"
#include "nntp/NntpArticle.h"

#include <QDebug>
#include <QProgressBar>
#include <QLabel>
#include <QScreen>
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


const QList<const char *> MainWindow::sServerListHeaders = {
    QT_TRANSLATE_NOOP("MainWindow", "on"),
    QT_TRANSLATE_NOOP("MainWindow", "Host (name or IP)"),
    QT_TRANSLATE_NOOP("MainWindow", "Port"),
    QT_TRANSLATE_NOOP("MainWindow", "SSL"),
    QT_TRANSLATE_NOOP("MainWindow", "Connections"),
    QT_TRANSLATE_NOOP("MainWindow", "Username"),
    QT_TRANSLATE_NOOP("MainWindow", "Password"),
    "" // for the delete button
};
const QVector<int> MainWindow::sServerListSizes   = {30, 200, 50, 30, 100, 150, 150, sDeleteColumnWidth};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    _ui(new Ui::MainWindow),
    _ngPost(nullptr),
    _state(STATE::IDLE),
    _quickJobTab(nullptr),
    _autoPostTab(nullptr)
{
    setAcceptDrops(true);

    _ui->setupUi(this);

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

    QSize screenSize = qApp->screens()[0]->size();
    resize(screenSize * 0.8);
    setWindowIcon(QIcon(":/icons/ngPost.png"));
    setGeometry((screenSize.width() - width())/2,  (screenSize.height() - height())/2, width(), height());

    connect(_ui->clearLogButton, &QAbstractButton::clicked, _ui->logBrowser, &QTextEdit::clear);
    connect(_ui->debugBox,       &QAbstractButton::toggled, this,            &MainWindow::onDebugToggled);
    connect(_ui->pauseButton,    &QAbstractButton::clicked, this,            &MainWindow::onPauseClicked);

#if QT_VERSION < QT_VERSION_CHECK(5, 12, 0)
    connect(_ui->debugSB, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &MainWindow::onDebugValue);
#else
    connect(_ui->debugSB,   qOverload<int>(&QSpinBox::valueChanged),   this,    &MainWindow::onDebugValue);
#endif
}

MainWindow::~MainWindow()
{
    delete _ui;
}

void MainWindow::init(NgPost *ngPost)
{
    _ngPost = ngPost;

    _quickJobTab = new PostingWidget(ngPost, this, 1);
    _autoPostTab = new AutoPostWidget(ngPost, this);

    _ui->debugBox->setChecked(_ngPost->debugMode());
    _ui->debugSB->setEnabled(_ngPost->debugMode());

    QTabBar *tabBar = _ui->postTabWidget->tabBar();
    tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    tabBar->setElideMode(Qt::TextElideMode::ElideNone);
    tabBar->setIconSize({18, 18});

    _ui->postTabWidget->clear();
    _ui->postTabWidget->setUsesScrollButtons(true);
    _ui->postTabWidget->setStyleSheet(sTabWidgetStyle);
    _ui->postTabWidget->addTab(_quickJobTab, QIcon(":/icons/quick.png"), _ngPost->quickJobName());
    tabBar->setTabToolTip(0, tr("Default %1").arg(_ngPost->quickJobName()));
    _ui->postTabWidget->addTab(_autoPostTab, QIcon(":/icons/auto.png"), _ngPost->folderMonitoringName());
    tabBar->setTabToolTip(1, _ngPost->folderMonitoringName());
    _ui->postTabWidget->addTab(new QWidget(_ui->postTabWidget), QIcon(":/icons/plus.png"), tr("New"));
    tabBar->setTabToolTip(2, QString("Create a new %1").arg(_ngPost->quickJobName()));

//    connect(_ui->postTabWidget,           &QTabWidget::currentChanged, this, &MainWindow::onJobTabClicked);
    connect(tabBar, &QTabBar::tabBarClicked,              this, &MainWindow::onJobTabClicked);
    connect(tabBar, &QWidget::customContextMenuRequested, this, &MainWindow::onTabContextMenu);
    connect(tabBar, &QTabBar::tabCloseRequested,          this, &MainWindow::onCloseJob);
    _ui->postTabWidget->setTabsClosable(true);
    _ui->postTabWidget->installEventFilter(this);
//    _ui->postTabWidget->setCurrentIndex(1);

    setJobLabel(1);


    for (const QString &lang : _ngPost->languages())
        _ui->langCB->addItem(QIcon(QString(":/icons/flag_%1.png").arg(lang.toUpper())), lang.toUpper(), lang);
    _ui->langCB->setCurrentText(_ngPost->_lang.toUpper());
    connect(_ui->langCB, &QComboBox::currentTextChanged, this, &MainWindow::onLangChanged);

    _initServerBox();
    _initPostingBox();
    _quickJobTab->init();
    _autoPostTab->init();

    _ui->goCmdButton->hide();
//    connect(_ui->goCmdButton, &QAbstractButton::clicked, _ngPost, &NgPost::onGoCMD, Qt::QueuedConnection);

    connect(_ui->nightModeButton, &QAbstractButton::clicked, _ngPost, &NgPost::onSwitchNightMode);

    updateProgressBar(0, 0);
}


void MainWindow::updateProgressBar(uint nbArticlesTotal, uint nbArticlesUploaded, const QString &avgSpeed
                                   #ifdef __COMPUTE_IMMEDIATE_SPEED__
                                       , const QString &immediateSpeed
                                   #endif
                                   )
{
//    qDebug() << "[MainWindow::updateProgressBar] _nbArticlesUploaded: " << nbArticlesUploaded;
    _ui->progressBar->setValue(static_cast<int>(nbArticlesUploaded));

#ifdef __COMPUTE_IMMEDIATE_SPEED__
    _ui->uploadLbl->setText(QString("%5 (%1 / %2) %3: %4").arg(
                                nbArticlesUploaded).arg(
                                nbArticlesTotal).arg(
                                tr("avg speed")).arg(
                                avgSpeed).arg(
                                immediateSpeed));
#else
    _ui->uploadLbl->setText(QString("(%1 / %2) %3: %4").arg(
                                nbArticlesUploaded).arg(
                                nbArticlesTotal).arg(
                                tr("avg speed")).arg(
                                avgSpeed));
#endif
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

bool MainWindow::useFixedPassword() const
{
    return _ui->rarPassCB->isChecked();
}

bool MainWindow::hasAutoCompress() const
{
    return _ui->autoCompressCB->isChecked();
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
    if (currentTabIdx == 1)
        _autoPostTab->handleDropEvent(e);
    else if (currentTabIdx < _ui->postTabWidget->count() - 1)
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
        QStringList serverTableHeader;
        QTabBar *tabBar = _ui->postTabWidget->tabBar();
        int lastTabIdx = tabBar->count() - 1;
        switch(event->type()) {
        // this event is send if a translator is loaded
        case QEvent::LanguageChange:
            qDebug() << "MainWindow::changeEvent";
            _ui->retranslateUi(this);
#ifdef __COMPUTE_IMMEDIATE_SPEED__
            _ui->uploadLbl->setToolTip(tr("Immediate speed (avg on %1 sec) - (nb Articles uploaded / total number of Articles) - avg speed").arg(NgPost::immediateSpeedDuration()));
#endif
            _ui->shutdownCB->setToolTip(tr("Shutdown computer when all the current Posts are done (with command: %1)").arg(
                                            _ngPost->_shutdownCmd));

            _ui->serverBox->setTitle(tr("Servers"));
            _ui->fileBox->setTitle(tr("Files"));
            _ui->postingBox->setTitle(tr("Parameters"));
            _ui->logBox->setTitle(tr("Posting Log"));

            tabBar->setTabText(0, _ngPost->quickJobName());
            tabBar->setTabToolTip(0, tr("Default %1").arg(_ngPost->quickJobName()));
            tabBar->setTabText(1, _ngPost->folderMonitoringName());
            tabBar->setTabToolTip(1, _ngPost->folderMonitoringName());
            for (int i = 2 ; i < lastTabIdx; ++i)
                tabBar->setTabText(i, _ngPost->quickJobName());
            tabBar->setTabText(lastTabIdx, tr("New"));
            tabBar->setTabToolTip(2, QString("Create a new %1").arg(_ngPost->quickJobName()));

            setJobLabel(_ui->postTabWidget->currentIndex());

            for (const char *header : sServerListHeaders)
                serverTableHeader << tr(header);
            _ui->serversTable->setHorizontalHeaderLabels(serverTableHeader);


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
    bool enabled = !checked;
    _ui->fromEdit->setEnabled(enabled);
    _ui->genPoster->setEnabled(enabled);
    _ui->saveFromCB->setEnabled(enabled);
    _ui->uniqueFromCB->setEnabled(enabled);
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
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QAction *action = menu.addAction(QIcon(":/icons/clear.png"), tr("Close All finished Tabs"), this, &MainWindow::onCloseAllFinishedQuickTabs);
#else
    QAction *action = menu.addAction(QIcon(":/icons/clear.png"), tr("Close All finished Tabs"), this, SLOT(onCloseAllFinishedQuickTabs));
#endif
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
    connect(_ui->shutdownCB,        &QAbstractButton::toggled, this, &MainWindow::onShutdownToggled);
    connect(_ui->saveButton,        &QAbstractButton::clicked, this, &MainWindow::onSaveConfig);

    connect(_ui->genPoster,         &QAbstractButton::clicked, this, &MainWindow::onGenPoster);
    connect(_ui->obfuscateMsgIdCB,  &QAbstractButton::toggled, this, &MainWindow::onObfucateToggled);
    connect(_ui->uniqueFromCB,      &QAbstractButton::toggled, this, &MainWindow::onUniqueFromToggled);
    connect(_ui->rarPassCB,         &QAbstractButton::toggled, this, &MainWindow::onRarPassToggled);
    connect(_ui->genPass,           &QAbstractButton::clicked, this, &MainWindow::onArchivePass);
    connect(_ui->autoCompressCB,    &QAbstractButton::toggled, this, &MainWindow::onAutoCompressToggled);
    connect(_ui->rarPassEdit,       &QLineEdit::textChanged,   this, &MainWindow::onRarPassUpdated);

    _ui->fromEdit->setText(_ngPost->xml2txt(_ngPost->_from.c_str()));
    _ui->groupsEdit->setText(_ngPost->groups());
    _ui->uniqueFromCB->setChecked(_ngPost->_genFrom);
    _ui->saveFromCB->setChecked(_ngPost->_saveFrom);

    _ui->rarLengthSB->setValue(static_cast<int>(_ngPost->_lengthPass));
    if (_ngPost->_rarPassFixed.isEmpty())
    {
        _ui->rarPassCB->setChecked(false);
        onRarPassToggled(false);
    }
    else
    {
	// Issue #48 we should set the text first!
        _ui->rarPassEdit->setText(_ngPost->_rarPassFixed);
        _ui->rarPassCB->setChecked(true);
    }

    _ui->autoCompressCB->setChecked(_ngPost->_packAuto);
    _ui->autoCloseCB->setChecked(_ngPost->_autoCloseTabs);

    _ui->obfuscateMsgIdCB->setChecked(_ngPost->_obfuscateArticles);
    _ui->obfuscateFileNameCB->setChecked(_ngPost->_obfuscateFileName);

    _ui->articleSizeEdit->setText(QString::number(NgConf::kArticleSize));
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
        bool isEnabled =  static_cast<CheckBoxCenterWidget*>(_ui->serversTable->cellWidget(row, col++))->isChecked();

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
        srvParams->enabled = isEnabled;
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
            from += QString("@%1.com").arg(NgConf::kArticleIdSignature.c_str());
        _ngPost->_from   = _ngPost->escapeXML(from).toStdString();
    }
    _ngPost->_genFrom  = _ui->uniqueFromCB->isChecked();
    _ngPost->_saveFrom = _ui->saveFromCB->isChecked();

    if (_ui->rarPassCB->isChecked())
    {
        _ngPost->_rarPassFixed = _ui->rarPassEdit->text();
        _ngPost->_rarPass      = _ngPost->_rarPassFixed;
    }

    _ngPost->enableAutoPacking(_ui->autoCompressCB->isChecked());
    _ngPost->_autoCloseTabs = _ui->autoCloseCB->isChecked();

    _ngPost->updateGroups(_ui->groupsEdit->toPlainText());

    _ngPost->_obfuscateArticles = _ui->obfuscateMsgIdCB->isChecked();
    _ngPost->_obfuscateFileName = _ui->obfuscateFileNameCB->isChecked();

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

QString MainWindow::fixedArchivePassword() const
{
    if (_ui->rarPassCB->isChecked())
    {
        QString pass = _ui->rarPassEdit->text();
        if (!pass.isEmpty())
            return pass;
    }
    return QString();
}

PostingWidget *MainWindow::addNewQuickTab(int lastTabIdx, const QFileInfoList &files)
{
    if (!lastTabIdx)
        lastTabIdx = _ui->postTabWidget->count() -1;
    PostingWidget *newPostingWidget = new PostingWidget(_ngPost, this, static_cast<uint>(lastTabIdx));
    newPostingWidget->init();
    QString tabName = QString("%1 #%2").arg(_ngPost->quickJobName()).arg(lastTabIdx);
    _ui->postTabWidget->insertTab(lastTabIdx,
                                  newPostingWidget ,
                                  QIcon(":/icons/quick.png"),
                                  tabName);
    _ui->postTabWidget->setTabToolTip(lastTabIdx, tabName);

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

    _ui->serversTable->setCellWidget(nbRows, col++,
                                     new CheckBoxCenterWidget(_ui->serversTable,
                                                              serverParam ? serverParam->enabled : true));

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

int MainWindow::_getPostWidgetIndex(PostingWidget *postWidget) const
{
    int nbJob = _ui->postTabWidget->count() -1;
    for (int i = 2; i < nbJob ; ++i)
    {
        if (_ui->postTabWidget->widget(i) == postWidget)
            return i;
    }
    return 0;
}



void MainWindow::onGenPoster()
{
    _ui->fromEdit->setText(_ngPost->randomFrom());
}

void MainWindow::onUniqueFromToggled(bool checked)
{
    bool enabled = !checked;
    _ui->genPoster->setEnabled(enabled);
    _ui->fromEdit->setEnabled(enabled);
    _ui->saveFromCB->setEnabled(enabled);
}

void MainWindow::onRarPassToggled(bool checked)
{
    _ui->rarPassEdit->setEnabled(checked);
    _ui->rarLengthSB->setEnabled(checked);
    _ui->genPass->setEnabled(checked);
    if (checked)
        onRarPassUpdated(_ui->rarPassEdit->text());
}

void MainWindow::onRarPassUpdated(const QString &fixedPass)
{
    _ngPost->_rarPassFixed = fixedPass;
//    if (!_quickJobTab->isPosting())
        _quickJobTab->setNzbPassword(fixedPass);
    PostingWidget *currentQuickPost = _getPostWidget(_ui->postTabWidget->currentIndex());
    if (currentQuickPost) //&& !currentQuickPost->isPosting())
        currentQuickPost->setNzbPassword(fixedPass);
}

void MainWindow::onArchivePass()
{
    _ui->rarPassEdit->setText(_ngPost->randomPass(static_cast<uint>(_ui->rarLengthSB->value())));
}

void MainWindow::onAutoCompressToggled(bool checked)
{
    _ngPost->enableAutoPacking(checked);
    _autoPostTab->setPackingAuto(checked, _ngPost->_packAutoKeywords);
    if (!_quickJobTab->isPosting())
        _quickJobTab->setPackingAuto(checked, _ngPost->_packAutoKeywords);
    PostingWidget *currentQuickPost = _getPostWidget(_ui->postTabWidget->currentIndex());
    if (currentQuickPost && !currentQuickPost->isPosting())
        currentQuickPost->setPackingAuto(checked, _ngPost->_packAutoKeywords);
}

void MainWindow::onDebugToggled(bool checked)
{
#ifdef __DEBUG__
    qDebug() << "Debug mode: " << checked;
#endif
    if (checked)
    {
        if (!_ui->debugSB->value())
            _ui->debugSB->setValue(1);
        _ngPost->setDebug(static_cast<ushort>(_ui->debugSB->value()));
    }
    else
        _ngPost->setDebug(0);
    _ui->debugSB->setEnabled(checked);
}

void MainWindow::onDebugValue(int value)
{
    _ngPost->setDebug(static_cast<ushort>(value));
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

void MainWindow::closeTab(PostingWidget *postWidget)
{
    int index = _getPostWidgetIndex(postWidget);
    if (index)
    {
        int nbJob = _ui->postTabWidget->count() -1;
        _ui->postTabWidget->removeTab(index);
        delete postWidget;

        if (index == nbJob - 1)
            _ui->postTabWidget->setCurrentIndex(_ui->postTabWidget->count() - 2);
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

void MainWindow::onShutdownToggled(bool checked)
{
    if (checked)
    {
        int res = QMessageBox::question(this,
                                        tr("Automatic Shutdown?"),
                                        QString("%1\n%2").arg(
                                            tr("You're about to schedule the shutdown of the computer once all the current Postings will be finished")).arg(
                                            tr("Are you sure you want to switch off the computer?")),
                                        QMessageBox::Yes,
                                        QMessageBox::No);
        if (res == QMessageBox::Yes)
            _ngPost->_doShutdownWhenDone = checked;
        else
            _ui->shutdownCB->setChecked(false);
    }
    else
        _ngPost->_doShutdownWhenDone = false;
}

void MainWindow::setPauseIcon(bool pause)
{
    if (pause)
        _ui->pauseButton->setIcon(QIcon(":/icons/pause.png"));
    else
        _ui->pauseButton->setIcon(QIcon(":/icons/play.png"));
}

void MainWindow::setNightMode(bool goNight)
{
    static const QString kGoDay = ":/icons/DayMode.png";
    static const QString kGoNight = ":/icons/NightMode.png";
    _ui->nightModeButton->setImage(goNight ? kGoDay : kGoNight);
}

void MainWindow::onPauseClicked()
{
    if (_ngPost->isPosting())
    {
        if (_ngPost->isPaused())
            _ngPost->resume();
        else
            _ngPost->pause();
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

// from https://doc.qt.io/qt-5/stylesheet-examples.html#customizing-qtabwidget-and-qtabbar
const QString MainWindow::sTabWidgetStyle = "\
        QTabWidget::pane { /* The tab widget frame */\
            border-top: 2px solid #C2C7CB;\
        }\
        \
        QTabWidget::tab-bar {\
            left: 5px; /* move to the right by 5px */\
        }\
        \
        /* Style the tab using the tab sub-control. Note that\
            it reads QTabBar _not_ QTabWidget */ \
        QTabBar::tab { \
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,\
                                        stop: 0 #E1E1E1, stop: 0.4 #DDDDDD,\
                                        stop: 0.5 #D8D8D8, stop: 1.0 #D3D3D3);\
            border: 2px solid #C4C4C3;\
            border-bottom-color: #C2C7CB; /* same as the pane color */\
            border-top-left-radius: 4px;\
            border-top-right-radius: 4px;\
            min-width: 8ex;\
            padding: 2px;\
        }\
        \
        QTabBar::tab:selected, QTabBar::tab:hover {\
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,\
                                        stop: 0 #fafafa, stop: 0.4 #f4f4f4,\
                                        stop: 0.5 #e7e7e7, stop: 1.0 #fafafa);\
        }\
        \
        QTabBar::tab:selected {\
            border-color: #9B9B9B;\
            border-bottom-color: #C2C7CB; /* same as pane color */\
        }\
        \
        QTabBar::tab:!selected {\
            margin-top: 2px; /* make non-selected tabs look smaller */\
        }\
        \
        /* make use of negative margins for overlapping tabs */\
        QTabBar::tab:selected {\
            /* expand/overlap to the left and right by 4px */\
            margin-left: -4px;\
            margin-right: -4px;\
        }\
        \
        QTabBar::tab:first:selected {\
            margin-left: 0; /* the first selected tab has nothing to overlap with on the left */\
        }\
        \
        QTabBar::tab:last:selected {\
            margin-right: 0; /* the last selected tab has nothing to overlap with on the right */\
        }\
        \
        QTabBar::tab:only-one {\
            margin: 0; /* if there is only one tab, we don't want overlapping margins */\
        }\
        \
        QTabBar::scroller { /* the width of the scroll buttons */\
            width: 40px;\
        }\
        QTabBar QToolButton::right-arrow { /* the arrow mark in the tool buttons */\
            image: url(:/icons/right.png);\
        }\
        \
        QTabBar QToolButton::left-arrow {\
            image: url(:/icons/left.png);\
        }\
";
