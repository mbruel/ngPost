#ifndef TESTTOOLS_H
#define TESTTOOLS_H
#include "utils/PureStaticClass.h"
#include <QString>
class NgPost;

class TestTools : public PureStaticClass
{
    inline static const QString     xsnewsConfig = "://config/ngPost.xsnews.partner";
    inline static const QString     xsnewsAddr   = "reader.xsnews.nl";
    static constexpr ushort         xsnewsPort   = 443;
    inline static const std::string xsnewsUser   = "xsn_66598d1cc90f9";

public:
    static QString const &partnerConfig() { return xsnewsConfig; }

    static void loadXSNewsPartnerConf(NgPost *ngPost);
    static bool loadXSNewsPartnerConfAndCheckConnection(NgPost *ngPost);
};

#endif // TESTTOOLS_H
