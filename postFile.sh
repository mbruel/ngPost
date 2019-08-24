#!/bin/bash
#
# Script to upload a file on Usenet using ngPost (https://github.com/mbruel/ngPost)
# it's goal it's to do the split and the par2 generation in a temp folder prior to upload it
#
QT_VERSION=5.11.3
SCRIPT=`readlink -f "$0"`
SCRIPT_PATH=`dirname "$SCRIPT"`
LIBS="$SCRIPT_PATH/libs"

export QT_PLUGIN_PATH=$LIBS/plugins/

# Uncomment the line below in case there is an issue loading the plugins
#export QT_DEBUG_PLUGINS=1

export LD_LIBRARY_PATH=$LIBS:$LD_LIBRARY_PATH

NG_POST="$SCRIPT_PATH/bin/ngPost"
TMP_FOLDER=/tmp


syntax()
{
	echo "syntax: $0 (-c <ngPost config file>)? -i <input file>"
}

#1.: parse the options
while getopts "hc:i:" opt
do
	case $opt in
	"i")
		FILE="$OPTARG"
	;;
	"c")
		CONFIG="$OPTARG"
	;;
	"h")
		syntax
		exit 0
	;;
	"?")
		echo "Invalid option: -$OPTARG"
		syntax
		exit 1
	;;
	esac
done

#2.: validate the options
#if [ -z $CONFIG ] || [ ! -f $CONFIG ] ; then
#	echo "you need to provide ngPost configuration file (-c option)"
#	exit 1
#fi
if [ -z $FILE ] || [ !  -f $FILE ] ; then
	echo "you need to provide an input file..."
	exit 1
fi


#3.: create the TMP_FOLDER
echo "File to upload: $FILE"
FILENAME=$(basename "${FILE%.*}")
TMP_FOLDER="$TMP_FOLDER/$FILENAME"
if [ -e $TMP_FOLDER ]; then
	echo "Folder $TMP_FOLDER already exist..."
	rm -rf $TMP_FOLDER
fi
mkdir $TMP_FOLDER


#4.: create the archives and par2
cd $TMP_FOLDER
PASS=$(pwgen -cBsn 13 1)
echo "RAR pass: $PASS"
rar a -v50m -ed -ep1 -m0 -hp"$PASS" "$FILENAME.rar" "$FILE"
par2 c -s768000 -r8 *.rar



#5.: post TMP_FOLDER
#echo "$NG_POST -m "password=$PASS" -i $TMP_FOLDER"
if [ -z $CONFIG ] ; then
	$NG_POST -m "password=$PASS" -i $TMP_FOLDER
else
	$NG_POST -m "password=$PASS" -c "$CONFIG" -i $TMP_FOLDER
fi


#6.: clean TMP_FOLDER
rm -rf $TMP_FOLDER

exit 0
