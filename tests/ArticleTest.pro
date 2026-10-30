QT += core testlib
CONFIG += console c++14 testcase
CONFIG -= app_bundle

INCLUDEPATH += ../src .

SOURCES += \
    ArticleTest.cpp \
    ../src/crypto/CryptoEngine.cpp \
    ../src/crypto/FF1Cipher.cpp \
    ../src/utils/Yenc.cpp

HEADERS += \
    ../src/SegmentIndexAllocator.h \
    ../src/crypto/CryptoEngine.h \
    ../src/crypto/FF1Cipher.h \
    ../src/utils/PureStaticClass.h \
    ../src/utils/Yenc.h

unix: LIBS += -largon2 -lsodium -lcrypto
win32: LIBS += -largon2 -lsodium -llibcrypto
