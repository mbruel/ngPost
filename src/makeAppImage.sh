#!/bin/bash
git pull
make clean
rm Makefile
$MB_QMAKE -o Makefile ngPost.pro CONFIG+=release
make -j 2
cp ngPost ../release/ngPost/
cd ..
cp  ngPost.conf release_notes.txt README* release/ngPost/
cd ..
/home/mb/apps/bin/linuxdeployqt-6-x86_64.AppImage ngPost/ngPost.desktop -appimage -qmake=$MB_QMAKE
ls -alrt

