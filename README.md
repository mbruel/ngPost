# ngPost

command line usenet poster for binaries developped in C++/QT</br>
it is designed to be as fast as possible and offer all the main features to post data easily and safely.</br>
since v1.1, a minimalist GUI has been added on request

Here are the main features and advantages of ngPost:

-   it can use **several servers** (using config file or the HMI) with each **several connections** (supporting ssl)
-   it is spreading those connection on **several threads**. By default the number of cores of the station but you can set the number if you fancy.
-   it is using **asynchronous sockets** in the upload threads
-   it is **preparing the Articles on the main Thread** (yEnc encoding) so the upload threads are **always ready to write** when they can. (each connections has 2 Articles ready in advance)
-   it is **limiting the disk access** to the minimum by opening the files to post only once in the main Thread and processing them sequentially one by one (no need to open several files at the same time)
-   it can do **full obfuscation of the Article Header** (subject and msg-id) careful, using this, you won't be able to find your post if you don't have the NZB file
-   it is generating a **random uploader for each Article Header** (from) but can use a fixed one if desired
-   it is of course **generating the nzb file**
-   it can support **multiple files** and **multiple folders**
-   there is an **handler on interruption** which means that if you stop it, you'll close properly and **generate the nzb for what has been posted**
-   in case of interruption, it will **list the files that havn't been uploaded** so you can repost only those ones and then manually concatenate the nzb files
-   you can **add meta in the header of the nzb** (typically for a password)
-   it retries to post an Article with a different UUID in case of error (cf retry parameter)
-   it retries to reconnect if there is an error on a socket (same retry parameter than for the articles)
-   ...

What it does not:
- compress or generate the par2 for a single files (use a script to do it ;))

**BUT I provide the postFile.sh for Linux** that is a handy script to post Files with rar compression with obfuscation and random password, the par2 generation and finally post all that on UseNet.
<pre>
Syntax: ./postFile.sh (-c <ngPost config file>)? (-i <input file>)+ (-o <nzb file>)?
There are 3 options:
   -i file or folders (you can add several, with relative or full path)
   -c for the config file
   -o for the nzb path
(   -h for the syntax)
</pre>
   

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
releases have only been made for Linux x64 and Windows x64 (for 7 and above)<br/>
in order to build on other OS, the easiest way would be to [install QT](https://www.qt.io/download) and load the project in QtCreator<br/>

### How to use it
<pre>
Syntax: ngPost (options)? (-i "file or directory to upload")+
	-help               : Help: display syntax
	-v or --version     : app version
	-c or --conf        : use configuration file (if not provided, we try to load $HOME/.ngPost)
	--disp_progress     : display cmd progress: NONE (default), BAR or FILES
	-i or --input       : input file to upload (single file or directory), you can use it multiple times
	-o or --output      : output file path (nzb)
	-t or --thread      : number of Threads (the connections will be distributed amongs them)
	-x or --obfuscate   : obfuscate the subjects of the articles (CAREFUL you won't find your post if you lose the nzb file)
	-g or --groups      : newsgroups where to post the files (coma separated without space)
	-m or --meta        : extra meta data in header (typically "password=qwerty42")
	-f or --from        : poster email (random one if not provided)
	-a or --article_size: article size (default one: 716800)
	-z or --msg_id      : msg id signature, after the @ (default one: ngPost)
	-r or --retry       : number of time we retry to an Article that failed (default: 5)

// without config file, you can provide all the parameters to connect to ONE SINGLE server
	-h or --host        : NNTP server hostname (or IP)
	-P or --port        : NNTP server port
	-s or --ssl         : use SSL
	-u or --user        : NNTP server username
	-p or --pass        : NNTP server password
	-n or --connection  : number of NNTP connections

Examples:
  - with config file: ngPost -c ~/.ngPost -m "password=qwerty42" -f ngPost@nowhere.com -i /tmp/file1 -i /tmp/file2 -i /tmp/folderToPost1 -i /tmp/folderToPost2
  - with all params:  ngPost -t 1 -m "password=qwerty42" -m "metaKey=someValue" -h news.newshosting.com -P 443 -s -u user -p pass -n 30 -f ngPost@nowhere.com             -g "alt.binaries.test,alt.binaries.test2" -a 64000 -i /tmp/folderToPost -o /tmp/folderToPost.nzb

If you don't provide the output file (nzb file), we will create it in the nzbPath with the name of the last file or folder given in the command line.
so in the first example above, the nzb would be: /tmp/folderToPost2.nzb
</pre>

### Portable release (Linux)
if you don't want to build it and install the dependencies, you can also the portable release that includes everything.<br/>
- download [ngPost_v1.6-x86_64.AppImage](https://github.com/mbruel/ngPost/raw/master/release/ngPost_v1.6-x86_64.AppImage)
- chmod 755 ngPost_v1.6-x86_64.AppImage
- launch it using the same syntax than describe in the section above
- if you wish to keep the configuration file, edit the file **~/.ngPost** using [this model](https://raw.githubusercontent.com/mbruel/ngPost/master/ngPost.conf) (don't put the .conf extension)

<u>Notes for older releases:</u>
- download the latest release [ngPost_v1.1_linux_x86_64.tar.bz2](https://github.com/mbruel/ngPost/raw/master/release/old/ngPost_v1.1_linux_x86_64.tar.bz2)
- decompress it (tar xjvf ngPost_v1.1_linux_x86_64.tar.bz2)
- use **ngPost.sh** or **postFile.sh** as they will set the required environment variables (LD_LIBRARY_PATH and QT_PLUGIN_PATH)

**ngPost.sh** uses the exact same arguments than ngPost (cf the above section)</br>
**postFile.sh** is a handy script that can only takes 2 arguments: -c for the config file and -i for a single input file that will be rar and par2 before posting


### Windows installer
- just use the packager [ngPost_1.6_x64_setup.exe](https://github.com/mbruel/ngPost/raw/master/release/ngPost_1.6_x64_setup.exe) or [ngPost_1.6_x86_setup.exe](https://github.com/mbruel/ngPost/raw/master/release/ngPost_1.6_x86_setup.exe) for the 32bit version
- edit **ngPost.conf** (in the installation folder) to add your server settings (you can put several). 
- launch **ngPost.exe** (GUI version)
- or you can use it with the command line: **ngPost.exe** -i "your file or directory"

if you prefer, you can give all the server parameters in the command line (cf the above section)<br/>
By default:
- ngPost will load the configuration file 'ngPost.conf' that is in the directory
- it will write the nzb file inside this directory too. (it's name will be the one of the latest input file in the command line)


### Minimalist GUI
![ngPost_v1.1_HMI](https://raw.githubusercontent.com/mbruel/ngPost/master/ngPost_v1.1_HMI.png)


### Alternatives

A list of Usenet posters from Nyuu github [can be found here](https://github.com/animetosho/Nyuu/wiki/Usenet-Uploaders).



### Licence
<pre>
//========================================================================
//
// Copyright (C) 2019 Matthieu Bruel <Matthieu.Bruel@gmail.com>
//
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
</pre>


### Questions / Issues / Requests
- if you've any troubles to build or run ngPost, feel free to drop me an email
- if you've some comments on the code, any questions on the implementation or any proposal for improvements, I'll be happy to discuss it with you so idem, feel free to drop me an email
- if you'd like some other features, same same (but different), drop me an email ;)

Here is my email: Matthieu.Bruel@gmail.com

