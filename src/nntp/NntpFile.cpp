//========================================================================
//
// Copyright (C) 2019 Matthieu Bruel <Matthieu.Bruel@gmail.com>
//
// This file is a part of ngPost : https://github.com/mbruel/ngPost
//
// ngPost is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; version 3.0 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// You should have received a copy of the GNU Lesser General Public
// License along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301,
// USA.
//
//========================================================================

#include "NntpFile.h"
#include "NgPost.h"
#include "NntpArticle.h"
#include <cmath>
#include <QTextStream>
#include <QDebug>

NntpFile::NntpFile(const QFileInfo &file, int num, int nbFiles, const QVector<QString> &grpList):
    QObject(),
    _file(file), _num(num), _nbFiles(nbFiles), _grpList(grpList),
    _nbAticles((int)std::ceil((float)file.size()/NgPost::articleSize())),
    _articles(),
    _nbPosted(0)
{
#if defined(__DEBUG__) && defined(LOG_CONSTRUCTORS)
    qDebug() << "Creation NntpFile: " << file.absoluteFilePath()
             << " size: " << file.size()
             << " article size: " << NgPost::articleSize()
             << " => nbArticles: " << _nbAticles;
#endif
    _articles.reserve(_nbAticles);
}

NntpFile::~NntpFile()
{
#if defined(__DEBUG__) && defined(LOG_CONSTRUCTORS)
    qDebug() << "Destruction nntpFile: " << _file.absoluteFilePath();
#endif
    qDeleteAll(_articles);
}

void NntpFile::onArticlePosted(NntpArticle *article)
{
    ++_nbPosted;
    qDebug() << "[NntpFile::articlePosted] " << _nbPosted << " / " << _nbAticles
             << " article part " << article->_part
             << article->id();
    article->_body.clear(); // free resources

    if (_nbPosted == _nbAticles)
        emit allArticlesArePosted(this);
}

#include <string>
#include <QDateTime>
void NntpFile::writeToNZB(QTextStream &stream)
{
    //    <file poster="NewsUP &lt;NewsUP@somewhere.cbr&gt;" date="1565026184" subject="[1/846] - &quot;1w7NbOvYC2E8D5oYeXROyp4FZAaxEOmK&quot; ">
    //        <groups>
    //          <group>alt.binaries.superman</group>
    //          <group>alt.binaries.paxer</group>
    //          <group>alt.binaries.xylo</group>
    //          <group>alt.binaries.kleverig</group>
    //        </groups>
    //        <segments>
    //          <segment bytes="716800" number="1">part52of36.5M2EYjx9pGk-gkmn6rAa_tc@powerpost2000AA.local</segment>



//    file poster="ngPost@somewhere.com" date="1565794820357" subject="Odin_3.09.3.zip">
//      <groups>    </groups>    <segments>      <segment bytes="716800" number="1">{83f5c794-1b1a-4bc3-88cb-87dff4060f2e}</segment>
//        <segment bytes="287882" number="2">{6f2e54e9-6a84-4eed-a811-5d9ebd0069e4}</segment>
//      <segments>  </file>
//    file poster="ngPost@somewhere.com" date="1565794820357" subject="nginx-1.15.1.tar.gz">
//      <groups>    </groups>    <segments>      <segment bytes="716800" number="1">{1c502e51-ab9c-4ec6-bd0a-17377ed8a0a7}</segment>
//        <segment bytes="307286" number="2">{ebae6f34-37e7-4df2-91b8-850f82f3cc47}</segment>
//      <segments>  </file>
    if (_nbAticles)
    {
        QString tab = NgPost::space();

        stream << tab << "<file poster=\"" << _articles.first()->_from.c_str()<< "\""
               << " date=\"" << QDateTime::currentMSecsSinceEpoch() << "\""
               << " subject=\""  << "[" << _num << "/" << _nbFiles << "] - &quot;"
               << NgPost::escapeXML(_file.fileName())
               << "&quot; \">\n";

        stream << tab << tab << "<groups>\n";
        for (const QString &grp : _grpList)
            stream << tab << tab << tab << "<group>" << grp << "</group>\n";
        stream << tab << tab << "</groups>\n";

        stream << tab << tab << "<segments>\n";
        for (NntpArticle *article : _articles)
        {
            stream << tab << tab << tab << "<segment"
                   << " bytes=\""  << article->_fileBytes << "\""
                   << " number=\"" << article->_part << "\">"
                   << article->id() << "@ngPost"
                   << "</segment>\n";
        }
        stream << tab << tab << "</segments>\n";


        stream << tab << "</file>\n";
    }
}
