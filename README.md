<img align="left" width="80" height="80" src="https://raw.githubusercontent.com/mbruel/ngPost/master/src/resources/icons/ngPost.png" alt="ngPost">

# ngPost v4.16

[Pour la version Française cliquez ici ;)](https://github.com/mbruel/ngPost/blob/master/README_FR.md)<br/>
<br/>
[Here is an invite for the brand new discord server](https://discord.gg/TwYRDy9), feel free to join if you've questions or if interested in beta testing the new features before they're released ;)<br/>
<br/>

**Command Line and sexy GUI Usenet poster** for binaries developped in **C++11/Qt5**</br>
it is designed to be **as fast as possible** and offer ALL the main features **to post data easily and safely**.</br>
it can **compress** (using your external rar binary) and **generate the par2** before posting!<br/>
it has a **posting queue** to allow you to prepare several posts (especially using the GUI using Tabs).<br/>
it **packs the next Post while uploading the current one** (compression + par2 generation) <br/>
it can **automate posts** by **scanning folder(s)** and posting each file/folder individually after having them compressed (with a potential random archive name and password) and generated the par2!<br/>
it can **monitor folder(s) to post each new file/folder** individually after having them compressed.<br/>
it can **auto delete files/folders once posted** (only for monitoring with the HMI and with both --auto and --monitor in cmd)<br/>
it can **execute a COMMAND or script after each Post** (cf NZB_POST_CMD in the configuration file)<br/>
it can **switch off the computer** when all the posts are finished<br/>
it is **translated in Chinese, Dutch, English, French, German, Portuguese and Spanish**.


![ngPost_v4.3](https://raw.githubusercontent.com/mbruel/ngPost/master/pics/ngPost_v4.3.png)

[Releases are availables](https://github.com/mbruel/ngPost/releases/tag/v4.16) for: Linux 64bit, Windows (both 32bit and 64bit), MacOS and Raspbian (RPI 4). Soon for Android then iOS...

Here are the main features and advantages of ngPost:
-   **full obfuscation of the Article Header** : the Subject will be a UUID (as the msg-id) and a random Poster will be used. **Be Careful**, using this, you won't be able to find your post on Usenet (or any Indexers) if you lose the NZB file. But using this method is **completely safe**, **no need to obfuscate your files or even tp use a password**.
-   **Posting Queue**: you can prepare several posts while you're posting something. Of course you can cancel pending posts ;)
-   **Post automation**: scan a folder and post each file/folder after compression. (cf **--auto option in CMD** and the "Auto Posting" tab on the GUI)
-   **compress using RAR or 7zip** (external command) with random **name obfuscation** and password and **generate par2** before posting
-   **write history of posts in a csv file**: so you can get the date, **file name**, size, upload speed but most important the **archive name and password** in simple excel spreadsheet ;)
-   **par2cmdline is included in the package** but you can use another tool (like Multipar) if you wish using the PAR2_CMD and PAR2_ARGS keywords in the config file
-   support **multiple files** and **multiple folders**
-   support **several servers** (using config file or the HMI) with each **several connections** (supporting ssl)
-   spread those connection on **several threads**. By default the number of cores of the station but you can set the number if you fancy.
-   **prepare the Articles on the main Thread** (yEnc encoding) so the upload threads are **always ready to write** when they can. (each connections has 2 Articles ready in advance)
-   use **asynchronous sockets** in the upload threads
-   **limit the disk access** to the minimum by opening the files to post only once in the main Thread and processing them sequentially one by one (no need to open several files at the same time, the Articles will be spread to all the connections)
-   generate a **random poster** (from) but can use a fixed one if desired
-   **generate the nzb file**
-   **handle interruption** : if a post is stopped,  **the nzb is generated with the files that have been posted** and it will **list the files that havn't been posted** so you can repost only those ones and then manually concatenate the nzb files. (in CMD, the application would close properly)
-   **add meta in the header of the nzb** (typically for a password)
-   Retry to post Articles **with a different UUID** in case of error
-   Try to reconnect if there is an error on a socket (same Retry parameter than for the articles)
-   ...


### How to build
#### Dependencies:
- build-essential (C++ compiler, libstdc++, make,...)
- qt5-default (Qt5 libraries and headers)
- qt5-qmake (to generate the moc files and create the Makefile)
- libssl (v1.0.2 or v1.1) but it should be already installed on your system

#### Build:
- go to the src folder
- qmake
- make

Easy! it should have generate the executable **ngPost**</br>
you can copy it somewhere in your PATH so it will be accessible from anywhere

 
As it is made in C++/QT, you can build it and run it on any OS (Linux / Windows / MacOS / Android) <br/>
releases have only been made for Linux x64 and Windows x64 (for 7 and above) and MacOS (<br/>
in order to build on other OS, the easiest way would be to [install QT](https://www.qt.io/download) and load the project in QtCreator<br/>

### How to use it in command line
<pre>
Syntax: ngPost (options)* (-i <file or folder> | --auto <folder> | --monitor <folder>)+
	--help             : Help: display syntax
	-v or --version    : app version
	-c or --conf       : use configuration file (if not provided, we try to load $HOME/.ngPost)
	--disp_progress    : display cmd progressbar: NONE (default), BAR or FILES
	-d or --debug      : display extra information
	--fulldebug        : display full debug information
	-l or --lang       : application language
	--check            : check nzb file (if articles are available on Usenet) cf https://github.com/mbruel/nzbCheck
	-q or --quiet      : quiet mode (no output on stdout)

// automated posting (scanning and/or monitoring)
	--auto             : parse directory and post every file/folder separately. You must use --compress, should add --gen_par2, --gen_name and --gen_pass
	--monitor          : monitor directory and post every new file/folder. You must use --compress, should add --gen_par2, --gen_name and --gen_pass
	--rm_posted        : delete file/folder once posted. You must use --auto or --monitor with this option.

// quick posting (several files/folders)
	-i or --input      : input file to upload (single file or directory), you can use it multiple times
	-o or --output     : output file path (nzb)
	-x or --obfuscate  : obfuscate the subjects of the articles (CAREFUL you won't find your post if you lose the nzb file)
	-g or --groups     : newsgroups where to post the files (coma separated without space)
	-m or --meta       : extra meta data in header (typically "password=qwerty42")
	-f or --from       : poster email (random one if not provided)
	-a or --article_size: article size (default one: 716800)
	-z or --msg_id     : msg id signature, after the @ (default one: ngPost)
	-r or --retry      : number of time we retry to an Article that failed (default: 5)
	-t or --thread     : number of Threads (the connections will be distributed amongs them)
	--gen_from         : generate a new random email for each Post (--auto or --monitor)

// for compression and par2 support
	--tmp_dir          : temporary folder where the compressed files and par2 will be stored
	--rar_path         : RAR absolute file path (external application)
	--rar_size         : size in MB of the RAR volumes (0 by default meaning NO split)
	--rar_max          : maximum number of archive volumes
	--par2_pct         : par2 redundancy percentage (0 by default meaning NO par2 generation)
	--par2_path        : par2 absolute file path (in case of self compilation of ngPost)
	--auto_compress    : compress inputs with random name and password and generate par2 (equivalent of --compress --gen_name --gen_pass --gen_par2)
	--compress         : compress inputs using RAR or 7z
	--gen_par2         : generate par2 (to be used with --compress)
	--rar_name         : provide the RAR file name (to be used with --compress)
	--rar_pass         : provide the RAR password (to be used with --compress)
	--gen_name         : generate random RAR name (to be used with --compress)
	--gen_pass         : generate random RAR password (to be used with --compress)
	--length_name      : length of the random RAR name (to be used with --gen_name), default: 17
	--length_pass      : length of the random RAR password (to be used with --gen_pass), default: 13
	--rar_no_root_folder: Remove root (parent) folder when compressing Folders using RAR

// you can provide servers in one string using -S and/or split the parameters for ONE SINGLE server (this will overwrite the configuration file)
	-S or --server     : NNTP server following the format (&lt;user&gt;:&lt;pass&gt;@@@)?&lt;host&gt;:&lt;port&gt;:&lt;nbCons&gt;:(no)?ssl
	-h or --host       : NNTP server hostname (or IP)
	-P or --port       : NNTP server port
	-s or --ssl        : use SSL
	-u or --user       : NNTP server username
	-p or --pass       : NNTP server password
	-n or --connection : number of NNTP connections

Examples:
  - with monitoring: ngPost_v4.16_cmd-x86_64.AppImage --monitor /data/folder1 --monitor /data/folder2 --auto_compress --rm_posted --disp_progress files
  - with auto post: ngPost_v4.16_cmd-x86_64.AppImage --auto /data/folder1 --auto /data/folder2 --compress --gen_par2 --gen_name --gen_pass --rar_size 42 --disp_progress files
  - with compression, filename obfuscation, random password and par2: ngPost_v4.16_cmd-x86_64.AppImage -i /tmp/file1 -i /tmp/folder1 -o /nzb/myPost.nzb --compress --gen_name --gen_pass --gen_par2
  - with config file: ngPost_v4.16_cmd-x86_64.AppImage -c ~/.ngPost -m "password=qwerty42" -f ngPost@nowhere.com -i /tmp/file1 -i /tmp/file2 -i /tmp/folderToPost1 -i /tmp/folderToPost2
  - with all params:  ngPost_v4.16_cmd-x86_64.AppImage -t 1 -m "password=qwerty42" -m "metaKey=someValue" -h news.newshosting.com -P 443 -s -u user -p pass -n 30 -f ngPost@nowhere.com  -g "alt.binaries.test,alt.binaries.test2" -a 64000 -i /tmp/folderToPost -o /tmp/folderToPost.nzb

If you don't provide the output file (nzb file), we will create it in the nzbPath with the name of the first file or folder given in the command line.
so in the second example above, the nzb would be: /tmp/file1.nzb
</pre>

### Configuration file and keywords that are only in config
The default configuration file for Linux and Mac environment is: **~/.ngPost** (no conf extension)<br/>
If you wish, you can use another one in command line with the -c option.<br/>
[Here the example to follow](https://github.com/mbruel/ngPost/blob/master/ngPost.conf).<br/>
<br/>
Most configuration keywords can be used in command line but few of them, are only in the config file:
- **MONITOR_NZB_FOLDERS** : each monitoring post would go in its own folder created in nzbPath (Req/Issue #15)
- **inputDir** : Default folder to open to select files from the HMI (it can be updated if you change the Auto Dir in the HMI and the SAVE)
- **POST_HISTORY**: csv file where all successful post will append the date, the file name, its size, the upload speed, the archive name and its password
- **GROUP_POLICY**: to define the policy for posting when several Groups are provided (on all, one per post or one per file)
- **NZB_RM_ACCENTS**: remove accents and special characters from the nzb file names
- **AUTO_CLOSE_TABS**: close Quick Post Tabs when posted successfully (for the GUI)
- **RESUME_WAIT**: Time to wait (seconds) before trying to resume a Post automatically in case of loss of Network (min: 30)
- **NO_RESUME_AUTO**: stop a post when you loose the network
- ** PREPARE_PACKING**: when several posts are queued, prepare the packing of the next post while uploading the current one

The following ones are for experimented posters:
- **RAR_EXTRA** : to customize the rar command (no need to put the 'a', '-idp' or '-r'). No need to use it for 7-zip except if you wish to change the compession level.
- **PAR2_CMD**  : to change the par2 generator and be able to use [Parpar](https://github.com/animetosho/ParPar) or [Multipar](http://hp.vector.co.jp/authors/VA021385/) if you wish. (par2cmdline is the default embedded generator)
- **PAR2_ARGS** : to customize the par2 command, especially if you choose to use another one than the default par2cmdline
- **TMP_RAM**: use a temporary folder with size constraint, typically a tmpfs partition, for posting files that would fit (otherwise TMP_DIR is used)
- **TMP_RAM_RATIO**: Ratio used on the source files size to compensate the par2 generation

### Linux 64bit portable release (compiled with Qt v5.12.6)
if you don't want to build it and install the dependencies, you can also the portable release that includes everything.<br/>
- download [ngPost_v4.16-x86_64.AppImage](https://github.com/mbruel/ngPost/releases/download/v4.16/ngPost_v4.16-x86_64.AppImage) or [ngPost_v4.16_cmd-x86_64.AppImage](https://github.com/mbruel/ngPost/releases/download/v4.16/ngPost_v4.16_cmd-x86_64.AppImage) for pure CMD (the first version doing both CMD and GUI)
- chmod 755 ngPost_v4.16-x86_64.AppImage
- launch it using the same syntax than describe in the section above
- if you wish to keep the configuration file, edit the file **~/.ngPost** using [this model](https://raw.githubusercontent.com/mbruel/ngPost/master/ngPost.conf) (don't put the .conf extension)

PS: for older system with GLIBC < 2.24, here is a version compiled on Debian8 with GLIBC 2.19 and Qt v5.8.0: [ngPost_v4.16-x86_64_debian8.AppImage](https://github.com/mbruel/ngPost/releases/download/v4.16/ngPost_v4.16-x86_64_debian8.AppImage)


### Raspbian release (armhf for Raspberry PI)
- download [ngPost_v4.16-armhf.AppImage](https://github.com/mbruel/ngPost/releases/download/v4.16/ngPost_v4.16-armhf.AppImage)
- chmod 755 ngPost_v4.16-armhf.AppImage
- launch it using the same syntax than describe in the section above
- if you wish to keep the configuration file, edit the file **~/.ngPost** using [this model](https://raw.githubusercontent.com/mbruel/ngPost/master/ngPost.conf) (don't put the .conf extension)

**As rar is not available on RPI, 7zip support has been added.** Use RAR_PATH to point on the 7z and RAR_EXTRA to have 7zip options. You should have something like this:
<pre>
## RAR or 7zip absolute file path (external application)
## /!\ The file MUST EXIST and BE EXECUTABLE /!\
## this is set for Linux environment, Windows users MUST change it
#RAR_PATH = /usr/bin/rar
RAR_PATH = /usr/bin/7z

## RAR EXTRA options (the first 'a' and '-idp' will be added automatically)
## -hp will be added if you use a password with --gen_pass, --rar_pass or using the HMI
## -v42m will be added with --rar_size or using the HMI
## you could change the compression level, lock the archive, add redundancy...
#RAR_EXTRA = -ep1 -m0 -k -rr5p
RAR_EXTRA = -mx0 -mhe=on
</pre>


### Windows installer
- just use the packager [ngPost_v4.16_x64_setup.exe](https://github.com/mbruel/ngPost/releases/download/v4.16/ngPost_v4.16_x64_setup.exe) or [ngPost_v4.16_x86_setup.exe](https://github.com/mbruel/ngPost/releases/download/v4.16/ngPost_v4.16_x86_setup.exe) for the 32bit version
- edit **ngPost.conf** (in the installation folder) to add your server settings (you can put several).
- launch **ngPost.exe** (GUI version)
- or you can use it with the command line: **ngPost.exe** -i "your file or directory"

if you prefer, you can give all the server parameters in the command line (cf the above section)<br/>
By default:
- ngPost will load the configuration file 'ngPost.conf' that is in the directory
- it will write the nzb file inside this directory too. (it's name will be the one of the latest input file in the command line)

**Know Issue with non SSL support:** you may need to install [msvc2015 redistribuables](https://www.microsoft.com/en-us/download/details.aspx?id=48145) as libssl depends on its APIs<br/>


### MacOS release built on High Sierra (v10.13)
- download [ngPost_v4.16.1.dmg](https://github.com/mbruel/ngPost/releases/download/v4.16/ngPost_v4.16.1.dmg)
- launch it using the same syntax than describe in the section above
- if you wish to keep the configuration file, edit the file **~/.ngPost** using [this model](https://raw.githubusercontent.com/mbruel/ngPost/master/ngPost.conf) (don't put the .conf extension)


### Alternatives

A list of Usenet posters from Nyuu github [can be found here](https://github.com/animetosho/Nyuu/wiki/Usenet-Uploaders).



### Licence
**ngPost** is published under licence **GPL v3**
<pre>
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
</pre>


### Questions / Issues / Requests
- if you've any troubles to build or run ngPost, feel free to drop me an email
- if you've some comments on the code, any questions on the implementation or any proposal for improvements, I'll be happy to discuss it with you so idem, feel free to drop me an email
- if you'd like some other features, same same (but different), drop me an email ;)

Here is my email: Matthieu.Bruel@gmail.com


### Supported Languages
For now, ngPost is translated in Chinese, English, French, German, Portuguese and Spanish.<br/>
If you'd like to translate ngPost in your language, it's easy to do (there is a nice GUI for that: QtLinguist), please get in touch with me (Matthieu.Bruel@gmail.com)


### Thanks
- Uukrull for his intensive testing and feedbacks and for building all the MacOS packages.
- awsms for his testing on proper server with a 10Gb/s connection that helped to improve ngPost's upload speed and the multi-threading support
- animetosho for having developped ParPar, the fasted par2 generator ever!
- demanuel for the dev of NewsUP that was my first poster
- noobcoder1983, tensai then yuppie for the German translation
- tiriclote for the Spanish translation
- hunesco for the Portuguese translation
- Peng for the Chinese translation
- all ngPost users ;)


### Donations
I'm Freelance nowadays, working on several personal projects, so if you use the app and would like to contribute to the effort, feel free to donate what you can.<br/>
<br/>
<a href="https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=W2C236U6JNTUA&item_name=ngPost&currency_code=EUR"><img align="left" src="https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif" alt="ex0days"></a>
 or in Bitcoin at this address: **3BGbnvnnBCCqrGuq1ytRqUMciAyMXjXAv6**
<img align="right" align="bottom" width="120" height="120" src="https://raw.githubusercontent.com/mbruel/ngPost/master/pics/btc_qr.gif" alt="ngPost_QR">
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
<br/>
