include(ngPost.pri)

QT += testlib
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
INCLUDEPATH += $$PWD

TRANSLATIONS = lang/ngPost_en.ts  lang/ngPost_fr.ts
TARGET = test_ngPost
TEMPLATE = app
CONFIG += c++20 console
#CONFIG -= use_hmi  # Remove the hmi

#CONFIG(release, debug|release): DEFINES += QT_NO_DEBUG_OUTPU

DEFINES += __test_ngPost__


CONFIG(release, debug|release): DEFINES -= __TEST_DEBUG__
CONFIG(debug, debug|release): DEFINES += __TEST_DEBUG__



#-------------------------------------------------
# Macro Tests to activate/desactivate
#-------------------------------------------------

DEFINES += __Launch_TestNgTools__
DEFINES += __Launch_TestConfig__
DEFINES -= __Launch_TestLocalConfig__
DEFINES += __Launch_TestNzbCheck__
DEFINES += __Launch_TestDatabase__
DEFINES += __Launch_TestResumeJobs__

#-------------------------------------------------
# MACROS for TRACES
#-------------------------------------------------
DEFINES -= __MB_TRACE_UndoStack__
DEFINES -= __MB_TRACE_CACHE__

SOURCES += \
    tests/utils/MacroTest.cpp \
    tests/LoadConfig/TestConfig.cpp \
    tests/utils/PostingJobHandler.cpp \
    tests/utils/TestUtils.cpp \
    tests/utils/ConnectionHandler.cpp \
    tests/main.cpp \
    tests/testNgTools.cpp \
    tests/testdatabase.cpp \
    tests/testnzbcheck.cpp \
    tests/testresumejobs.cpp

HEADERS += \
    tests/LoadConfig/TestConfig.h \
    tests/utils/MacroTest.h \
    tests/utils/Macros.h \
    tests/utils/PostingJobHandler.h \
    tests/utils/TestUtils.h \
    tests/utils/ConnectionHandler.h \
    tests/testNgTools.h \
    tests/testdatabase.h \
    tests/testnzbcheck.h \
    tests/testresumejobs.h

RESOURCES += \
    tests/resources/test_ngPost.qrc

