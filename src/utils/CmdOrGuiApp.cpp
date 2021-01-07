//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
// This file is a part of ngPost : https://github.com/mbruel/ngPost
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3..
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>
//
//========================================================================

#include "CmdOrGuiApp.h"
#ifdef __USE_HMI__
  #include "hmi/MainWindow.h"
  #include <QApplication>
#else
  #include <QCoreApplication>
#endif

CmdOrGuiApp::CmdOrGuiApp(int &argc, char *argv[]):
#ifdef __USE_HMI__
    _app(nullptr),
    _mode(argc > 1 ? AppMode::CMD : AppMode::HMI),
    _hmi(nullptr)
#else
    _app(new QCoreApplication(argc, argv))
#endif
{
#ifdef __USE_HMI__
    if (_mode == AppMode::CMD)
        _app = new QCoreApplication(argc, argv);
    else
    {
        _app = new QApplication(argc, argv);
        _hmi = new MainWindow();
    }
#endif
}

CmdOrGuiApp::~CmdOrGuiApp()
{
#ifdef __USE_HMI__
    if (_hmi)
        delete  _hmi;
#endif
    delete _app;
}

void CmdOrGuiApp::checkForNewVersion()
{
    // No need here
    // check ngPost implementation if required
    // https://github.com/mbruel/ngPost
}

int CmdOrGuiApp::startEventLoop() { return _app->exec(); }

#ifdef __USE_HMI__
int CmdOrGuiApp::startHMI()
{
    _hmi->show();
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
#endif
