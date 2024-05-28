// #include "testnzbcheck.h"

// #include "TestNzbGet.h"

// #include <QDebug>
// #include <QtTest/QtTest>

// #include "NgPost.h"

// void TestNzbGet::initTestCase() { NgLogger::createInstance(); }
// void TestNzbGet::cleanupTestCase() { NgLogger::destroy(); }

// TestNzbGet::TestNzbGet(QString const &testName, int argc, char *argv[]) : MacroTest(testName)
//{
//     qDebug() << "Construction TestNzbGet => _ngPost...";
//     _ngPost = new NgPost(argc, argv);
// }

// void TestNzbGet::cleanup()
//{
//     qDebug() << "Deleting TestNzbGet =>ngPost...";
//     delete _ngPost;
//     // Code to run after each test
// }
// void TestNzbGet::onTestNzbCheckOK() { qDebug() << "onTestNzbCheckOK..."; }
// void TestNzbGet::onTestNzbCheckKO() { qDebug() << "onTestNzbCheckKO..."; }
