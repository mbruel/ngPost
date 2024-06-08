#ifndef TESTUTILS_H
#define TESTUTILS_H
#include "utils/PureStaticClass.h"
#include <QString>

class NgPost;
class NntpConnection;
class ConnectionHandler;

class TestUtils : public PureStaticClass
{
    inline static const QString     kXSNewsConfig = "://config/ngPost.xsnews.partner";
    inline static const QString     kXSNewsAddr   = "reader.xsnews.nl";
    static constexpr ushort         kXSNewsPort   = 443;
    inline static const std::string kXSNewsUser   = "xsn_66598d1cc90f9";

    static ConnectionHandler *sConnectionHandler;

public:
    static void           clearConnectionHandler();
    static QString const &partnerConfig() { return kXSNewsConfig; }

    static void               loadXSNewsPartnerConf(NgPost &ngPost, bool dispFirstSrv = true);
    static ConnectionHandler *loadXSNewsPartnerConfAndCheckConnection(NgPost &ngPost, bool testPosting = false);
};

#endif // TESTUTILS_H
