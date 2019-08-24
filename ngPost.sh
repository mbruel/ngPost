#!/bin/sh
QT_VERSION=5.11.3
LIBS=./libs

export QT_PLUGIN_PATH=$LIBS/plugins/

# Uncomment the line below in case there is an issue loading the plugins
#export QT_DEBUG_PLUGINS=1

export LD_LIBRARY_PATH=$LIBS:$LD_LIBRARY_PATH

SCRIPT_PATH=`dirname "$0"`
$SCRIPT_PATH/bin/ngPost "$@"
