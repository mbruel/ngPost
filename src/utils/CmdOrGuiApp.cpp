//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
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

#include "CmdOrGuiApp.h"
#include "hmi/MainWindow.h"
#include <QApplication>

CmdOrGuiApp::CmdOrGuiApp(int &argc, char *argv[]):
    _app(nullptr),
    _mode(argc > 1 ? AppMode::CMD : AppMode::HMI),
    _hmi(nullptr)
{
    if (_mode == AppMode::CMD)
        _app =  new QCoreApplication(argc, argv);
    else
    {
        _app = new QApplication(argc, argv);
        _hmi = new MainWindow();
    }
}

CmdOrGuiApp::~CmdOrGuiApp()
{
    if (_hmi)
        delete  _hmi;
    delete _app;
}

void CmdOrGuiApp::checkForNewVersion()
{
    // No need here
    // check ngPost implementation if required
    // https://github.com/mbruel/ngPost
}

int CmdOrGuiApp::startHMI()
{
    _hmi->show();
    return _app->exec();
}

int CmdOrGuiApp::startEventLoop()
{
    return _app->exec();
}

void CmdOrGuiApp::hideOrShowGUI()
{
    if (_hmi)
    {
        if (_hmi->isVisible())
            _hmi->hide();
        else
            _hmi->show();
    }
}
