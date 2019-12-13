QT += network gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

DEFINES += "APP_VERSION=\"1.4\""


INCLUDEPATH += $$PWD
TARGET = ngPost
TEMPLATE = app
CONFIG += c++14
CONFIG -= app_bundle

win32: {
    RC_ICONS += ngPost.ico

# we need the console to be able to print stuff in command line mode...
# we hide the console if we start in GUI mode
    CONFIG += console
}

DEFINES += __USE_MUTEX__
DEFINES += __DISP_PROGRESS_BAR__

CONFIG(debug, debug|release) :{
    DEFINES += __DEBUG__

    DEFINES += LOG_CONNECTION_STEPS
    DEFINES += LOG_CONNECTION_ERRORS_BEFORE_EMIT_SIGNALS
    DEFINES += LOG_NEWS_AUTH
    DEFINES += LOG_NEWS_DATA
    DEFINES += LOG_CONSTRUCTORS

    DEFINES += __SAVE_ARTICLES__
}
else {
    # In release mode, remove all qDebugs !
    DEFINES += QT_NO_DEBUG_OUTPUT
}



# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        NgPost.cpp \
        NntpConnection.cpp \
        hmi/CheckBoxCenterWidget.cpp \
        main.cpp \
        nntp/Nntp.cpp \
        nntp/NntpArticle.cpp \
        nntp/NntpFile.cpp \
        utils/Yenc.cpp \
        hmi/MainWindow.cpp


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    NgPost.h \
    NntpConnection.h \
    hmi/CheckBoxCenterWidget.h \
    nntp/Nntp.h \
    nntp/NntpArticle.h \
    nntp/NntpFile.h \
    nntp/NntpServerParams.h \
    utils/PureStaticClass.h \
    utils/Yenc.h \
    hmi/MainWindow.h


FORMS += \
    hmi/MainWindow.ui

RESOURCES += \
    resources/resources.qrc
