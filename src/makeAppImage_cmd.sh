#!/bin/bash
git pull
make clean
rm Makefile
$MB_QMAKE -o Makefile ngPost_cmd.pro CONFIG+=release
make -j 2
cp ngPost ../release/ngPost/
cd ..
rm -rf release/ngPost/lib release/ngPost/plugins
cp  ngPost.conf release_notes.txt README* release/ngPost/
cd ..
/home/mb/apps/bin/linuxdeployqt-6-x86_64.AppImage ngPost/ngPost.desktop -appimage -qmake=$MB_QMAKE
ls -alrt

