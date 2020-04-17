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

#ifndef CMDORGUIAPP_H
#define CMDORGUIAPP_H

class MainWindow;
class QCoreApplication;
class QString;

class CmdOrGuiApp
{
protected:
    enum class AppMode : bool {CMD = 0, HMI = 1}; //!< supposed to be CMD but a simple HMI has been added

    QCoreApplication  *_app;  //!< Application instance (either a QCoreApplication or a QApplication in HMI mode)
    const AppMode      _mode; //!< CMD or HMI (for Windowser...)
    MainWindow        *_hmi;  //!< potential HMI


public:
    explicit CmdOrGuiApp(int &argc, char *argv[]);
    virtual ~CmdOrGuiApp();

    virtual const char *appName() = 0;

    virtual bool parseCommandLine(int argc, char *argv[]) = 0;
    virtual void checkForNewVersion();

    virtual int startHMI();

    inline bool useHMI() const;
    int startEventLoop(); //!< to start in CMD
};

bool CmdOrGuiApp::useHMI() const { return _mode == AppMode::HMI; }

#endif // CMDORGUIAPP_H
