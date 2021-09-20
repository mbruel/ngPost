#!/bin/bash
make clean
git pull
$MB_QMAKE -o Makefile ngPost.pro CONFIG+=release
make -j 2
cp ngPost ../release/ngPost/
cd ../..
linuxdeployqt ngPost/ngPost.desktop -appimage -qmake=$MB_QMAKE
lt
