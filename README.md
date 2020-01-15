<img align="left" width="80" height="80" src="https://raw.githubusercontent.com/mbruel/ngPost/master/src/resources/icons/ngPost.png" alt="ngPost">

# ngPost

command line / GUI Usenet poster for binaries developped in C++/QT</br>
it is designed to be as fast as possible and offer all the main features to post data easily and safely.</br>

Releases are available for: Linux 64bit, Windows (both 32bit and 64bit), MacOS. It will soon be available for Raspbian, Android then iOS...

Here are the main features and advantages of ngPost:
-   **full obfuscation of the Article Header** : the Subject will be a UUID (as the msg-id) and a random Poster will be used. **Be Careful**, using this, you won't be able to find your post on Usenet (or any Indexers) if you lose the NZB file. But using this method is **completely safe**, **no need to obfuscate your files or even use a password**.
-   **compress using RAR** (external command) with random **name obfuscation** and password and **generate par2** before posting
-   **par2 is included in the package** but you can use another tool (like Multipar) if you wish using the PAR2_CMD and PAR2_ARGS keywords in the config file
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

### How to use it
<pre>
Syntax: ngPost (options)? (-i &lt;file or directory to upload&gt;)+
	--help             : Help: display syntax
	-v or --version    : app version
	-c or --conf       : use configuration file (if not provided, we try to load $HOME/.ngPost)
	--disp_progress    : display cmd progress: NONE (default), BAR or FILES
	-d or --debug      : display some debug logs
	-i or --input      : input file to upload (single file or directory), you can use it multiple times
	-o or --output     : output file path (nzb)
	-t or --thread     : number of Threads (the connections will be distributed amongs them)
	-x or --obfuscate  : obfuscate the subjects of the articles (CAREFUL you won't find your post if you lose the nzb file)
	-g or --groups     : newsgroups where to post the files (coma separated without space)
	-m or --meta       : extra meta data in header (typically "password=qwerty42")
	-f or --from       : poster email (random one if not provided)
	-a or --article_size: article size (default one: 716800)
	-z or --msg_id     : msg id signature, after the @ (default one: ngPost)
	-r or --retry      : number of time we retry to an Article that failed (default: 5)

// for compression and par2 support
	--tmp_dir          : temporary folder where the compressed files and par2 will be stored
	--rar_path         : RAR absolute file path (external application)
	--rar_size         : size in MB of the RAR volumes (0 by default meaning NO split)
	--par2_pct         : par2 redundancy percentage (0 by default meaning NO par2 generation)
	--par2_path        : par2 absolute file path (in case of self compilation of ngPost)
	--compress         : compress inputs using RAR
	--gen_par2         : generate par2 (to be used with --compress)
	--rar_name         : provide the RAR file name (to be used with --compress)
	--rar_pass         : provide the RAR password (to be used with --compress)
	--gen_name         : generate random RAR name (to be used with --compress)
	--gen_pass         : generate random RAR password (to be used with --compress)
	--length_name      : length of the random RAR name (to be used with --gen_name), default: 17
	--length_pass      : length of the random RAR password (to be used with --gen_pass), default: 13

// without config file, you can provide all the parameters to connect to ONE SINGLE server
	-h or --host       : NNTP server hostname (or IP)
	-P or --port       : NNTP server port
	-s or --ssl        : use SSL
	-u or --user       : NNTP server username
	-p or --pass       : NNTP server password
	-n or --connection : number of NNTP connections

Examples:
  - with compression, filename obfuscation, random password and par2: ngPost -i /tmp/file1 -i /tmp/folder1 -o /nzb/myPost.nzb --compress --gen_name --gen_pass --gen_par2
  - with config file: ngPost -c ~/.ngPost -m "password=qwerty42" -f ngPost@nowhere.com -i /tmp/file1 -i /tmp/file2 -i /tmp/folderToPost1 -i /tmp/folderToPost2
  - with all params:  ngPost -t 1 -m "password=qwerty42" -m "metaKey=someValue" -h news.newshosting.com -P 443 -s -u user -p pass -n 30 -f ngPost@nowhere.com  -g "alt.binaries.test,alt.binaries.test2" -a 64000 -i /tmp/folderToPost -o /tmp/folderToPost.nzb

If you don't provide the output file (nzb file), we will create it in the nzbPath with the name of the first file or folder given in the command line.
so in the second example above, the nzb would be: /tmp/file1.nzb
</pre>

### Portable release (Linux)
if you don't want to build it and install the dependencies, you can also the portable release that includes everything.<br/>
- download [ngPost_v2.1-x86_64.AppImage](https://github.com/mbruel/ngPost/raw/master/release/ngPost_v2.1-x86_64.AppImage)
- chmod 755 ngPost_v2.1-x86_64.AppImage
- launch it using the same syntax than describe in the section above
- if you wish to keep the configuration file, edit the file **~/.ngPost** using [this model](https://raw.githubusercontent.com/mbruel/ngPost/master/ngPost.conf) (don't put the .conf extension)



### Windows installer
- just use the packager [ngPost_v2.1_x64_setup.exe](https://github.com/mbruel/ngPost/raw/master/release/ngPost_v2.1_x64_setup.exe) or [ngPost_v2.1_x86_setup.exe](https://github.com/mbruel/ngPost/raw/master/release/ngPost_v2.1_x86_setup.exe) for the 32bit version
- edit **ngPost.conf** (in the installation folder) to add your server settings (you can put several). 
- launch **ngPost.exe** (GUI version)
- or you can use it with the command line: **ngPost.exe** -i "your file or directory"

if you prefer, you can give all the server parameters in the command line (cf the above section)<br/>
By default:
- ngPost will load the configuration file 'ngPost.conf' that is in the directory
- it will write the nzb file inside this directory too. (it's name will be the one of the latest input file in the command line)


### MacOS release built on High Sierra 9v10.13)
- download [ngPost_v2.1.dmg](https://github.com/mbruel/ngPost/raw/master/release/ngPost_v2.1.dmg)
- launch it using the same syntax than describe in the section above
- if you wish to keep the configuration file, edit the file **~/.ngPost** using [this model](https://raw.githubusercontent.com/mbruel/ngPost/master/ngPost.conf) (don't put the .conf extension)


### Minimalist GUI
![ngPost_v2.1](https://raw.githubusercontent.com/mbruel/ngPost/master/pics/ngPost_v2.1.png)


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


### Thanks
- Uukrull for testing on an unstable version, providing the crash logs and backtraces, suggesting nice features to add to the app
- demanuel for the dev of NewsUP that was my first poster
- all ngPost users ;)


### Donations
I'm Freelance nowadays, working on several personal projects, so if you use the app and would like to contribute to the effort, feel free to donate what you can.<br/>
<br/>
[![](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=W2C236U6JNTUA&item_name=ngPost&currency_code=EUR)
