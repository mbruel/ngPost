QT += testlib core gui xml concurrent network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
INCLUDEPATH += $$PWD

TRANSLATIONS = lang/ngPost_en.ts  lang/ngPost_fr.ts
TARGET = test_ngPost
TEMPLATE = app
CONFIG += c++20 console
#CONFIG -= use_hmi  # Remove the hmi

#CONFIG(release, debug|release): DEFINES += QT_NO_DEBUG_OUTPU

DEFINES += __test_ngPost__

include(ngPost.pri)

CONFIG(release, debug|release): DEFINES -= __TEST_DEBUG__
CONFIG(debug, debug|release): DEFINES += __TEST_DEBUG__



#-------------------------------------------------
# Macro Tests to activate/desactivate
#-------------------------------------------------

DEFINES += __Launch_TestNgTools__
DEFINES += __Launch_TestConfig__
DEFINES += __Launch_TestNzbCheck__
DEFINES += __Launch_NoPostingConnection__

#-------------------------------------------------
# MACROS for TRACES
#-------------------------------------------------
DEFINES -= __MB_TRACE_UndoStack__
DEFINES -= __MB_TRACE_CACHE__

SOURCES += \
    tests/LoadConfig/TestConfig.cpp \
    tests/TestUtils.cpp \
    tests/main.cpp \
    tests/testNgTools.cpp \
    tests/testnzbcheck.cpp

HEADERS += \
    tests/LoadConfig/TestConfig.h \
    tests/MacroTest.h \
    tests/Macros.h \
    tests/TestUtils.h \
    tests/testNgTools.h \
    tests/testnzbcheck.h

RESOURCES += \
    tests/resources/test_ngPost.qrc

