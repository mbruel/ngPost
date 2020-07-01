#ifndef MACROS_H
#define MACROS_H
#include <QtGlobal>

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    #define MB_FLUSH flush
#else
    #define MB_FLUSH Qt::flush
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    #define MB_LoadAtomic(atom) atom.load()
#else
    #define MB_LoadAtomic(atom) atom.loadRelaxed()
#endif


#endif // MACROS_H
