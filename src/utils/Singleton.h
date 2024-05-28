//========================================================================
//
// Copyright (C) 2024 Matthieu Bruel <Matthieu.Bruel@gmail.com>
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
#ifndef SINGLETON_H
#define SINGLETON_H

template <typename T>
class Singleton
{
protected:
    static T *sInstance; //!< Unique instance

protected:
    // Constructor / Destructor are protected so we can inherit
    Singleton()          = default;
    virtual ~Singleton() = default;

    virtual void connectSignalSlots() { }

public:
    Singleton(Singleton const &)             = delete;
    Singleton(Singleton const &&)            = delete;
    Singleton &operator=(Singleton const &)  = delete;
    Singleton &operator=(Singleton const &&) = delete;

    static T const &instance() { return *sInstance; }

    //! should be called at the beginning of main.cpp
    static void createInstance()
    {
        if (!sInstance)
            sInstance = new T;
        sInstance->connectSignalSlots();
    }

    static bool isCreated() { return sInstance != nullptr; }

    static void reset()
    {
        if (sInstance)
        {
            delete sInstance;
            sInstance = nullptr;
        }
    }
};

template <typename T>
T *Singleton<T>::sInstance = nullptr;

#endif // SINGLETON_H
