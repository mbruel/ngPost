#!/bin/bash
#
# Script to upload a file on Usenet using ngPost (https://github.com/mbruel/ngPost)
# it's goal it's to do the split and the par2 generation in a temp folder prior to upload it
#
UUID_NAME=1
GEN_UUID=dbus-uuidgen

QT_VERSION=5.12.6
SCRIPT=`readlink -f "$0"`
SCRIPT_PATH=`dirname "$SCRIPT"`
LIBS="$SCRIPT_PATH/libs"

export QT_PLUGIN_PATH=$LIBS/plugins/

NG_POST="$SCRIPT_PATH/src/ngPost"
TMP_FOLDER=/tmp

# Uncomment the line below in case there is an issue loading the plugins
#export QT_DEBUG_PLUGINS=1

export LD_LIBRARY_PATH=$LIBS:$LD_LIBRARY_PATH

# 0.: if using RAR obfuscation, generate RAR_NAME
if [ "$UUID_NAME" -eq 1 ]; then
	RAR_NAME=`$GEN_UUID`
	if [ "$?" -ne 0 ]; then
		echo "can't find the '$GEN_UUID' command to generate the RAR name..."
		exit -1
		# could try using pwgen instead ;)
	else
		echo "using RAR obfuscation: $RAR_NAME.rar"
	fi
fi

syntax()
{
	echo "syntax: $0 (-c <ngPost config file>)? (-i <input file>)+ (-o <nzb file>)?"
}

#1.: parse the options
while getopts "hc:i:o:" opt
do
	case $opt in
	"i")
#		FILE="$OPTARG"
		FILES+=("$OPTARG")
	;;
	"o")
		NZBFILE="$NZB_PATH/$OPTARG"
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

MAIN_FILE=$FILES
echo "Nb files: ${#FILES[@]}, main one: $MAIN_FILE, all files: ${FILES[@]}"

#2.: validate the options
#if [ -z $CONFIG ] || [ ! -f $CONFIG ] ; then
#	echo "you need to provide ngPost configuration file (-c option)"
#	exit 1
#fi
if [ -z $MAIN_FILE ] || [ !  -f $MAIN_FILE ] ; then
	echo "you need to provide at least one input file..."
	exit 1
fi


#3.: create the TMP_FOLDER
echo "File to upload: $MAIN_FILE"
FILENAME=$(basename "${MAIN_FILE%.*}")
TMP_FOLDER="$TMP_FOLDER/$FILENAME"
if [ -e $TMP_FOLDER ]; then
	echo "Folder $TMP_FOLDER already exist..."
	rm -rf $TMP_FOLDER
fi
mkdir $TMP_FOLDER


#4.: if no RAR_NAME (no obfuscation) use real FILENAME
if [ -z "$RAR_NAME" ]; then
	RAR_NAME=$FILENAME
fi


#4.: create the archives and par2
PASS=$(pwgen -cBsn 13 1)
echo "RAR pass: $PASS"
rar a -v50m -ed -ep1 -m0 -hp"$PASS" "$TMP_FOLDER/$RAR_NAME.rar" "${FILES[@]}"
cd $TMP_FOLDER
par2 c -s768000 -r8 *.rar



#5.: create the NZBFILE if not provided (using main FILENAME)
if [ -z "$NZBFILE" ]; then
	NZBFILE="$NZB_PATH/$FILENAME"
fi


#6.: generate random from
FROM="$(pwgen -cBsn 11 1)@ngPost.com"
echo "RAR_NAME: $RAR_NAME, PASS: $PASS, NZBFILE: $NZBFILE, FROM: $FROM"


#7.: post TMP_FOLDER
#echo "$NG_POST -m "password=$PASS" -i $TMP_FOLDER"
if [ -z $CONFIG ] ; then
	$NG_POST -m "password=$PASS" -i $TMP_FOLDER -o $NZBFILE -f $FROM
else
	$NG_POST -m "password=$PASS" -c "$CONFIG" -i $TMP_FOLDER -o $NZBFILE -f $FROM
fi


#8.: clean TMP_FOLDER
rm -rf $TMP_FOLDER

exit 0
