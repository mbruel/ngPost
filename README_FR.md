<img align="left" width="80" height="80" src="https://raw.githubusercontent.com/mbruel/ngPost/master/src/resources/icons/ngPost.png" alt="ngPost">

# ngPost v4.3

ngPost est un posteur pour Usenet en ligne de commande ou via une interface graphique développé en C++11/Qt5.<br/>
Il a été conçu pour être le plus rapide possible et offrir toutes les fonctionnalités utiles pour poster facilement et en toute sécurité.<br/>
<br/>
Voici la liste des principales fonctionnalités et atouts de ngPost:
  - **compression** (utilisant rar en tant qu'application externe) et **génération des par2** avant de poster!
  - **scan de dossier(s)** afin de poster chaque fichier/dossier individuellement après les avoir compressés
  - **surveillance de dossier(s)** afin de poster chaque nouveau fichier/dossier individuellement après les avoir compressés
  - **suppression automatique** des fichiers/dossiers une fois postés (uniquement avec --auto et --monitor)
  - génération du **fichier nzb** et écriture d'un **fichier csv d'historique des posts**
  - **mode invisible**: obfuscation complète des Articles : impossible de (re)trouver un post sans avoir le fichier nzb
  - possibilité **d'éteindre l'ordinateur** lorsque tous les posts sont finis
  - multi-langues (Français, Anglais, Allemand et Espagnol)
  - ...

![ngPost_v4.3](https://raw.githubusercontent.com/mbruel/ngPost/master/pics/ngPost_v4.3.png)


[Les versions pour chacun des OS sont disponibles ici](https://github.com/mbruel/ngPost/tree/master/release), pour: Linux 64bit, Windows (32bit et 64bit), MacOS et Raspbian (RPI 4). Bientôt pour Android et iOS...


### Fichier de configuration

Tout d'abord il vous faut éditer le fichier de configuration, basez vous [sur cette version FR](https://github.com/mbruel/ngPost/blob/master/ngPost_fr.conf).
  - sous Windows: il est dans le dossier d'installation de ngPost: ngPost.conf (remplacez la version anglaise par la française ci-dessus)
  - sous Linux et MacOS: $HOME/.ngPost (sans extension)

Si vous utilisez l'interface graphique, lancez l'application, changez la langue, ajoutez vos serveurs, indiquez tous les champs puis cliquer sur "sauver"

Sinon vous pouvez éditer le fichier à la main. Il vous faut remplir:
  - nzbPath (dossier de destination par défaut pour les fichiers nzb)
  - inputDir (surtout pour l'IHM, commentez le)
  - POST_HISTORY (Fichier historique des posts)
  - groups (liste des groupes sur lesquels vous postez)
  - TMP_DIR (dossier temporaire pour les archives et par2)
  - RAR_PATH (chemin d'accès complet de l'éxécutable RAR ou 7zip)
  - la ou les sections Server


### En ligne de commande
<pre>
Syntaxe: ngPost (options)* (-i <file or folder> | --auto <folder> | --monitor <folder>)+
	--help             : Aide: afficher la syntaxe
	-v or --version    : version de l'application
	-c or --conf       : utilisation d'un fichier de configuration autre que celui par défaut ($HOME/.ngPost ou ngPost.conf sous Windows)
	--disp_progress    : affichage de la progression en ligne de commande: NONE (défaut), BAR (barre de progression) ou FILES (log à chaque upload de fichier)
	-d or --debug      : debug mode (plus d'info)
	-l or --lang       : langue de l'application (EN, FR, ES ou DE)

// post automatique (scan et/ou surveillance du dossier auto)
	--auto             : Scan du dossier en paramètre et post de chaque fichier/dossier individuellement. L'option de compression est obligatoire
	--monitor          : Surveillance du dossier en paramètre et post de chaque nouveau fichier/dossier. L'option de compression est obligatoire
	--rm_posted        : supression des fichiers/dossiers une fois postés. Uniquement avec les option --auto ou --monitor.

// post rapide (plusieurs fichiers ou dossiers)
	-i or --input      : fichier(s) ou dossier(s) à poster. Post rapide. Si dossier, son contenu sera posté (sans recursivité)
	-o or --output     : chemin complet du fichier nzb
	-x or --obfuscate  : obfuscation des Articles. ATTENTION, avec cette option, il est impossible de (re)trouver un post sans le fichier nzb
	-g or --groups     : liste de newsgroup où poster (séparés par une virgule et sans espaces)
	-m or --meta       : extra metadata (typiquement "password=azerty42"). pour poster des archives avec mot de passe (fait avant ngPost)
	-f or --from       : posteur si vous ne voulez pas un généré aléatoirement
	-a or --article_size: article size (default one: 716800)
	-z or --msg_id     : msg id signature, after the @ (default one: ngPost)
	-r or --retry      : number of time we retry to an Article that failed (default: 5)
	-t or --thread     : nombre de Threads (les connexions sont distribuées parmis eux)

// pour la compression et le support des fichiers par2
	--tmp_dir          : dossier temporaire où les archives sont crées ainsi que les par2
	--rar_path         : chemin d'accès complet de l'exécutable RAR
	--rar_size         : taille en Mo des volumes RAR (0 équivalent à un seul fichier RAR)
	--rar_max          : nombre maximum de volumes RAR
	--par2_pct         : pourcentage de redondance des fichiers par2. (0 équivaut à pas de génération de par2)
	--par2_path        : chemin d'accès complet de l'application par2 ou alternative (parpar, multipar...)
	--compress         : compression des fichiers/dossiers avant le post
	--gen_par2         : génération des fichiers par2 (avec --compress)
	--rar_name         : nom des archives (avec --compress)
	--rar_pass         : mot de passe de l'archive (avec --compress)
	--gen_name         : génération aléatoire du nom de l'archive (avec --compress)
	--gen_pass         : génération aléatoire du mot de passe de l'archive (avec --compress)
	--length_name      : length of the random RAR name (to be used with --gen_name), default: 17
	--length_pass      : length of the random RAR password (to be used with --gen_pass), default: 13

// sans fichier de configuration, vous pouvez fournir tous les paramètres pour se connecter à UN SEUL serveur
	-h or --host       : serveur NNTP (DNS ou IP)
	-P or --port       : port du serveur NNTP
	-s or --ssl        : cryptage SSL
	-u or --user       : nom d'utilisateur du serveur NNTP
	-p or --pass       : mot de passe du serveur NNTP
	-n or --connection : nombre de connexions du serveur NNTP

Exemples:
  - surveillance d'un dossier: ngPost --monitor --rm_posted /Downloads/testNgPost --compress --gen_par2 --gen_name --gen_pass --rar_size 42 --disp_progress files
  - post automatique: ngPost --auto /Downloads/testNgPost --compress --gen_par2 --gen_name --gen_pass --rar_size 42 --disp_progress files
  - avec compression, obfuscation, password et par2: ngPost -i /tmp/file1 -i /tmp/folder1 -o /nzb/myPost.nzb --compress --gen_name --gen_pass --gen_par2
  - avec fichier de configuration: ngPost -c ~/.ngPost -m "password=qwerty42" -f ngPost@nowhere.com -i /tmp/file1 -i /tmp/file2 -i /tmp/folderToPost1 -i /tmp/folderToPost2
  - avec les paramètres d'UN serveur:  ngPost -t 1 -m "password=qwerty42" -m "metaKey=someValue" -h news.newshosting.com -P 443 -s -u user -p pass -n 30 -f ngPost@nowhere.com  -g "alt.binaries.test,alt.binaries.test2" -a 64000 -i /tmp/folderToPost -o /tmp/folderToPost.nzb

Si vous ne fournissez pas le fichier de sortie (nzb file avec l'option -o), il sera créé dans le dossier par défaut nzbPath avec le nom du premier fichier ou dossier donné dans la ligne de commande.
donc pour le second exemple ci-dessus, le nom du nzb serait: /tmp/file1.nzb
</pre>

Donc en gros il y a 3 façons de l'utiliser:
  - le mode surveillance de dossier **--monitor**: ngPost ce mets plus ou moins en mode deamon, vous pouvez d'ailleurs le lancer en arrière plan. Vous lui indiquez un ou des dossiers à surveiller et dès que vous y déplacerez u copierez un fichier ou un dossier, il sera alors automatiquement posté. Selon les options, la compression, nom et mot de passe aléatoire seront effectués avant le post. Dans le fichier de configuration, vous pouvez aussi utiliser les mots clefs MONITOR_EXTENSIONS et MONITOR_IGNORE_DIR pour limiter les extensions des fichiers à poster et ne pas poster les dossiers.

  - le mode automatique **--auto**: chaque fichier/dossier sera posté de manière indépendante. Il est possible d'utiliser --auto et --monitor sur un même dossier car --monitor ne postera pas les fichiers actuels du dossier mais uniquement les nouveaux fichiers que vous y ajouterez ;)

  - le mode classique **-i (fichier|dossier)**. À noter qu'avec cette option le dossier ne sera pas posté de façon récursive si vous n'utilisez pas la compression (--compress). Le but étant d'utiliser cette option si vous avez déjà fait la compression et la génération de par2 avec votre script externe et donc que vous utilisez ngPost en tant que simple posteur.



### L'interface graphique
Une fois les parties Serveurs et Paramètres remplies, on remarque un système d'onglet en dessous.<br/>
2 sont statiques:
  - Post Rapide: pour poster simplement un ou plusieurs fichiers
  - Post Auto: pour générer des Post Rapides ou surveiller un ou plusieurs dossiers
L'onglet **Nouveau** permet de créer d'autres Post Rapides.






### Comment compiler ngPost
#### les dépendances:
- build-essential (C++ compiler, libstdc++, make,...)
- qt5-default (Qt5 libraries and headers)
- qt5-qmake (to generate the moc files and create the Makefile)
- libssl (v1.0.2 or v1.1) but it should be already installed on your system

#### Compilation:
- allez dans le dossier **src** ou copiez le en **bin**
- qmake
- make (-j nombre_de_threads)?

C'est tout, vous devriez avoir l'exécutable **ngPost**</br>
vous pouvez le copiez quelque part accessible dans votre PATH.<br/>
<br />
Comme ngPost est écrit en C++/Qt, vous pouvez le compiler sur tous les OS (Linux / Windows / MacOS / Android)<br/>
Les dernieres versions packagées sont disponibles ici<br/>
Une alternative pour compiler est [d'installer QT](https://www.qt.io/download) et de charger le fichier ngPost.pro dans QtCreator<br/>


### Linux 64bit portable release (compiled with Qt v5.12.6)
if you don't want to build it and install the dependencies, you can also the portable release that includes everything.<br/>
- download [ngPost_v4.3-x86_64.AppImage](https://github.com/mbruel/ngPost/raw/master/release/ngPost_v4.3-x86_64.AppImage)
- chmod 755 ngPost_v4.3-x86_64.AppImage
- launch it using the same syntax than describe in the section above
- if you wish to keep the configuration file, edit the file **~/.ngPost** using [this model](https://raw.githubusercontent.com/mbruel/ngPost/master/ngPost.conf) (don't put the .conf extension)

PS: for older system with GLIBC < 3.4, here is a version compiled on Debian8 with GLIBC 2.19 and Qt v5.8.0: [ngPost_v4.3-x86_64_debian8.AppImage](https://github.com/mbruel/ngPost/raw/master/release/ngPost_v4.3-x86_64_debian8.AppImage)


### Raspbian release (armhf for Raspberry PI)
- download [ngPost_v4.3-armhf.AppImage](https://github.com/mbruel/ngPost/raw/master/release/ngPost_v4.3-armhf.AppImage)
- chmod 755 ngPost_v4.3-armhf.AppImage
- launch it using the same syntax than describe in the section above
- if you wish to keep the configuration file, edit the file **~/.ngPost** using [this model](https://raw.githubusercontent.com/mbruel/ngPost/master/ngPost.conf) (don't put the .conf extension)


### Windows installer
- just use the packager [ngPost_v4.3_x64_setup.exe](https://github.com/mbruel/ngPost/raw/master/release/ngPost_v4.3_x64_setup.exe) or [ngPost_v4.3_x86_setup.exe](https://github.com/mbruel/ngPost/raw/master/release/ngPost_v4.3_x86_setup.exe) for the 32bit version
- edit **ngPost.conf** (in the installation folder) to add your server settings (you can put several).
- launch **ngPost.exe** (GUI version)
- or you can use it with the command line: **ngPost.exe** -i "your file or directory"

if you prefer, you can give all the server parameters in the command line (cf the above section)<br/>
By default:
- ngPost will load the configuration file 'ngPost.conf' that is in the directory
- it will write the nzb file inside this directory too. (it's name will be the one of the latest input file in the command line)

**Know Issue with non SSL support:** you may need to install [msvc2015 redistribuables](https://www.microsoft.com/en-us/download/details.aspx?id=48145) as libssl depends on its APIs<br/>


### MacOS release built on High Sierra (v10.13)
- download [ngPost_v4.3.dmg](https://github.com/mbruel/ngPost/raw/master/release/ngPost_v4.3.dmg)
- launch it using the same syntax than describe in the section above
- if you wish to keep the configuration file, edit the file **~/.ngPost** using [this model](https://raw.githubusercontent.com/mbruel/ngPost/master/ngPost.conf) (don't put the .conf extension)


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
- Uukrull for his intensive testing and feedbacks and for building all the MacOS packages.
- awsms for his testing on proper server with a 10Gb/s connection that helped to improve ngPost's upload speed and the multi-threading support
- animetosho for having developped ParPar, the fasted par2 generator ever!
- demanuel for the dev of NewsUP that was my first poster
- noobcoder1983 for the German translation
- tiriclote for the Spanish translation
- all ngPost users ;)


### Donations
I'm Freelance nowadays, working on several personal projects, so if you use the app and would like to contribute to the effort, feel free to donate what you can.<br/>
<br/>
[![](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=W2C236U6JNTUA&item_name=ngPost&currency_code=EUR)
<br/> or in Bitcoin on this address: **3BGbnvnnBCCqrGuq1ytRqUMciAyMXjXAv6** ![](https://raw.githubusercontent.com/mbruel/ngPost/master/pics/btc_qr.gif)
