#include "AboutNgPost.h"
#include "ui_AboutNgPost.h"
#include "NgPost.h"

AboutNgPost::AboutNgPost(NgPost *ngPost, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutNgPost)
{
    ui->setupUi(this);
    setWindowFlag(Qt::FramelessWindowHint);
    setStyleSheet("QDialog {border:2px solid black}");

    ui->titleLbl->setText(QString("<pre>%1</pre>").arg(ngPost->escapeXML(ngPost->asciiArtWithVersion())));

    ui->copyrightLbl->setText("Copyright © 2020 - Matthieu Bruel");
    ui->copyrightLbl->setStyleSheet("QLabel { color : darkgray; }");
    ui->copyrightLbl->setFont(QFont( "Arial", 12, QFont::Bold));

    ui->descLbl->setText(ngPost->desc());
    ui->descLbl->setStyleSheet("QLabel { color : #359bdb; }");
    ui->descLbl->setFont(QFont( "Caladea", 14, QFont::Medium));
//    ui->cosi7->setFont(QFont( "DejaVu Serif", 28, QFont::Bold));


    connect(ui->donateButton, &QAbstractButton::clicked, ngPost, &NgPost::onDonation);
}

AboutNgPost::~AboutNgPost()
{
    delete ui;
}

void AboutNgPost::keyPressEvent(QKeyEvent *e)
{
    Q_UNUSED(e);
    close();
}

void AboutNgPost::mousePressEvent(QMouseEvent *e)
{
    Q_UNUSED(e);
    close();
}
