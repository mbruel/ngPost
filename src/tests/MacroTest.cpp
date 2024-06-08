#include "MacroTest.h"
#include "NgPost.h"

void MacroTest::cleanup()
{
    qDebug() << "Reset config ngPost...";
    _ngPost->resetConfig();
}

