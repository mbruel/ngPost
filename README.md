# ngPost

command line usenet poster for binaries developped in C++/QT</br>
it is designed to be as fast as possible and offer all the main features to post data easily and safely.

Here are the main features and advantages of ngPost:

-   it can use **several servers** (using config file) with each **several connections** (supporting ssl)
-   it is spreading those connection on **several threads**. By default the number of cores of the station but you can set the number if you fancy.
-   it is using **asynchronous sockets** on the upload threads
-   it is **preparing the Articles on the main Thread** (Yenc encoding) so the upload threads are **always ready to write** when they can. (each connections has 2 Articles ready in advance)
-   it is **limiting the disk access** to the minimum by opening the files to post only once in the main Thread and process it sequentially
-   it is doing **full obfuscation of the Article Header** (subject and msg-id)
-   it is generating a **random uploader for each Article Header** (from) but can use a fixed one if desired
-   it is of course **generating the nzb file**
-   it can support **multiple files** and **multiple folders**
-   there is an **handler on interruption** which means that if you stop it, you'll close properly and **generate the nzb for what has been posted**
-   in case of interruption, it will **list the files that havn't been uploaded** so you can repost only those ones and then manually concatenate the nzb files
-   you can **add meta in the header of the nzb** (typically for a password)
-   ...

What it does not:
- compress or generate the par2 for a single files (use a script to do it ;))
- retry posting articles (not really needed as the msg-id of **the articles are generated using UUID** (followed by @<signature> that you can configure (by default @ngPost))
- retry to connect when a connection is lost as they are all created with the **KeepAliveOption** set. If we loose all the connections, ngPost close properly and write the nzb file

RQ: you don't need to obfuscate the file names as they will only be used in the nzb file as **the obfuscation is done for each Article with a UUID as subject and msg-id**.


### How to build and dependencies
To build it:
- you just need a C++ compiler, QT (latest the better), libssl-dev (v1.0.2 or v1.1)

To run it:
- you need these packages: libssl, libqtcore, libqtnetwork
- or you can use the portable release where all the libs are included
 
As it is made in C++/QT, you can run it on any OS (Linux / Windows / MacOS / Android)
releases have only been made for Linux x64 and Windows x64 (for 7 and above)



### How to use it
<pre>
Syntax: ngPost (options)? (-i <file or directory to upload>)+
	-help              : Help: display syntax
	-v or -version     : app version
	-c or -conf        : use configuration file
	-i or -input       : input file to upload (single file
	-o or -output      : output file path (nzb)
	-t or -thread      : number of Threads (the connections will be distributed amongs them)
	-r or -real_name   : real yEnc name in Article Headers (no obfuscation)
	-m or -meta        : extra meta data in header (typically "password=qwerty42")
	-f or -from        : uploader (in nzb file, article one is random)
	-a or -article_size: article size (default one: 716800)
	-z or -msg_id      : msg id signature, after the @ (default one: ngPost)

// without config file, you can provide all the parameters to connect to ONE SINGLE server
	-h or -host        : NNTP server hostname (or IP)
	-P or -port        : NNTP server port
	-s or -ssl         : use SSL
	-u or -user        : NNTP server username
	-p or -pass        : NNTP server password
	-n or -connection  : number of NNTP connections

Examples:
  - with config file: ngPost -c ~/.ngPost -m "password=qwerty42"  -i /tmp/file1 -i /tmp/file2 -i /tmp/folderToPost1 -i /tmp/folderToPost2
  - with all params:  ngPost  -t 1 -m "password=qwerty42" -m "metaKey=someValue" -h news.newshosting.com -P 443 -s -u user -p pass -n 30 -f ngPost@nowhere.com  -g "alt.binaries.test,alt.binaries.misc" -a 64000 -i /tmp/folderToPost -o /tmp/folderToPost.nzb

If you don't provide the output file (nzb file), we will create it in the nzbPath with the name of the last file or folder given in the command line.
so in the first example above, the nzb would be: /tmp/folderToPost2.nzb
</pre>



### Alternatives

A list of Usenet posters from Nyuu github [can be found
here](https://github.com/animetosho/Nyuu/wiki/Usenet-Uploaders).



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
