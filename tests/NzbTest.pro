QT += core testlib
CONFIG += console c++14 testcase
CONFIG -= app_bundle

INCLUDEPATH += ../src

SOURCES += \
    NzbTest.cpp

# PostingJob.h intentionally omitted: Q_OBJECT header included for its inline writeNzbHead only; moc'ing it would require PostingJob.cpp.
HEADERS += \
    ../src/EncryptionSettings.h \
    ../src/utils/CmdOrGuiApp.h
