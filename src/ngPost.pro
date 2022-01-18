CONFIG  += use_hmi

use_hmi {
    QT += gui
    greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

    DEFINES += __USE_HMI__
}
else {
    QT -= gui
    CONFIG += console
}

include(ngPost.pri)

use_hmi {
SOURCES += \
    hmi/AboutNgPost.cpp \
    hmi/AutoPostWidget.cpp \
    hmi/CheckBoxCenterWidget.cpp \
    hmi/PostingWidget.cpp \
    hmi/SignedListWidget.cpp \
    hmi/MainWindow.cpp

HEADERS += \
    hmi/AboutNgPost.h \
    hmi/AutoPostWidget.h \
    hmi/CheckBoxCenterWidget.h \
    hmi/PostingWidget.h \
    hmi/SignedListWidget.h \
    hmi/MainWindow.h

FORMS += \
    hmi/AboutNgPost.ui \
    hmi/AutoPostWidget.ui \
    hmi/MainWindow.ui \
    hmi/PostingWidget.ui
}
