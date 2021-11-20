<img align="left" width="80" height="80" src="https://raw.githubusercontent.com/mbruel/ngPost/master/src/resources/icons/ngPost.png" alt="ngPost">

# ngPost v4.16

ngPost est un posteur pour Usenet en ligne de commande ou via une interface graphique développé en C++11/Qt5.<br/>
Il a été conçu pour être le plus rapide possible et offrir toutes les fonctionnalités utiles pour poster facilement et en toute sécurité.<br/>
<br/>
Voici la liste des principales fonctionnalités et atouts de ngPost:
  - **compression** (utilisant rar en tant qu'application externe) et **génération des par2** avant de poster!
  - **scan de dossier(s)** afin de poster chaque fichier/dossier individuellement après les avoir compressés
  - **surveillance de dossier(s)** afin de poster chaque nouveau fichier/dossier individuellement après les avoir compressés
  - **parallélisation de l'upload avec le packing du Post suivant**
  - **suppression automatique** des fichiers/dossiers une fois postés (uniquement avec --auto et --monitor)
  - génération du **fichier nzb** et écriture d'un **fichier csv d'historique des posts**
  - **mode invisible**: obfuscation complète des Articles : impossible de (re)trouver un post sans avoir le fichier nzb
  - **exécution d'une commande ou un script une fois les nzb générés**
  - possibilité **d'éteindre l'ordinateur** lorsque tous les posts sont finis
  - multi-langues (Français, Allemand, Anglais, Chinois, Espagnol, Néerlandais, Portugais)
  - ...

![ngPost_v4.3](https://raw.githubusercontent.com/mbruel/ngPost/master/pics/ngPost_v4.3.png)


[Les versions pour chacun des OS sont disponibles ici](https://github.com/mbruel/ngPost/releases/tag/v4.16), pour: Linux 64bit, Windows (32bit et 64bit), MacOS et Raspbian (RPI 4). Bientôt pour Android et iOS...


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
	-d or --debug      : display extra information
	--fulldebug        : display full debug information
	-l or --lang       : langue de l'application (EN, FR, ES ou DE)
	--check            : check nzb file (if articles are available on Usenet) cf https://github.com/mbruel/nzbCheck
	-q or --quiet      : quiet mode (no output on stdout)

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
	--gen_from         : generate a new random email for each Post (--auto or --monitor)

// pour la compression et le support des fichiers par2
	--tmp_dir          : dossier temporaire où les archives sont crées ainsi que les par2
	--rar_path         : chemin d'accès complet de l'exécutable RAR
	--rar_size         : taille en Mo des volumes RAR (0 équivalent à un seul fichier RAR)
	--rar_max          : nombre maximum de volumes RAR
	--par2_pct         : pourcentage de redondance des fichiers par2. (0 équivaut à pas de génération de par2)
	--par2_path        : chemin d'accès complet de l'application par2 ou alternative (parpar, multipar...)
	--auto_compress    : compress inputs with random name and password and generate par2 (equivalent of --compress --gen_name --gen_pass --gen_par2)
	--compress         : compress inputs using RAR or 7z
	--gen_par2         : génération des fichiers par2 (avec --compress)
	--rar_name         : nom des archives (avec --compress)
	--rar_pass         : mot de passe de l'archive (avec --compress)
	--gen_name         : génération aléatoire du nom de l'archive (avec --compress)
	--gen_pass         : génération aléatoire du mot de passe de l'archive (avec --compress)
	--length_name      : length of the random RAR name (to be used with --gen_name), default: 17
	--length_pass      : length of the random RAR password (to be used with --gen_pass), default: 13

// il est possible de fournir plusieurs serveurs via l'option -S et/ou un UNIQUE serveur avec les paramètres séparés (ils écraseront ceux présent dans le fichier de configuration)
	-S or --server     : NNTP serveur suivant le format (&lt;user&gt;:&lt;pass&gt;@@@)?&lt;host&gt;:&lt;port&gt;:&lt;nbCons&gt;:(no)?ssl
	-h or --host       : serveur NNTP (DNS ou IP)
	-P or --port       : port du serveur NNTP
	-s or --ssl        : cryptage SSL
	-u or --user       : nom d'utilisateur du serveur NNTP
	-p or --pass       : mot de passe du serveur NNTP
	-n or --connection : nombre de connexions du serveur NNTP


Exemples:
  - surveillance d'un dossier: ngPost --monitor /Downloads/testNgPost --rm_posted --compress --gen_par2 --gen_name --gen_pass --rar_size 42 --disp_progress files
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
L'onglet **Nouveau** permet de créer d'autres Post Rapides.<br />
À noter que vous pouvez faire un click droit sur les onglets et vous avez l'option **Fermer tous les onglets des Posts finis**<br/>


#### le Post Rapide:

C'est assez intuitif... Ajoutez des fichiers / dossiers dans la liste en:
  - cliquant sur les boutons **Choisir Fichiers** ou **Choisir Dossier**
  - faisant un click droit (pour ajouter des fichiers) sur la zone de la liste
  - en drag and drop (glisser déposer depuis un explorateur)
  - le copiant / collant des fichiers (marche sous Linux, pas certain sous Windows)

Choisissez si vous voulez compresser, générer les par2,...
Puis il suffit de cliquer sur **Poster Fichiers**


#### le mode auto:

![ngPost_v4.3](https://raw.githubusercontent.com/mbruel/ngPost/master/pics/ngPost_v4.3_auto_fr.png)

- choisissez le **Dossier auto** (par défaut, c'est le inputDir du fichier de configuration)
- cliquez sur **Scanner**
- supprimez les fichiers du dossiers que vous ne souhaitez pas poster. (sélection dans la liste et touche SUPPR ou BackSpace du clavier)
- choisissez les options de compression / par2
- cochez ou non l'option **lancer tous les posts**
- cliquez sur **Créer Posts**

Des onglets de Posts Rapides seront alors créer. Ils seront démarrés automatiquement si vous avez coché l'option.<br/>
L'interface graphique sautera sur le Post Rapide courant.<br/>
À noter que vous pouvez faire un click droit sur les onglets et vous avez l'option **Fermer tous les onglets des Posts finis**<br/>


#### le mode surveillance:

![ngPost_v4.3](https://raw.githubusercontent.com/mbruel/ngPost/master/pics/ngPost_v4.3_monitor_fr.png)

- choisissez le **Dossier auto** (par défaut, c'est le inputDir du fichier de configuration)
- choisissez le **filtre sur les extensions** (ex: mp4,mkv,avi sans espaces) et si vous voulez poster les dossiers
- choisissez les options de compression / par2
- cliquez sur **Surveiller Dossier**

ngPost est alors en attente et dès que vous copierez ou déplacerez un fichier/dossier dans ce dossier Auto, alors il sera posté.<br/>
Vous pouvez ajouter d'autres dossiers de surveillance en cliquant sur le bouton **+** à côté du **Dossier auto**<br/>
Vous pouvez faire des Post Rapides durant la phase de Surveillance<br/>


### Comment compiler ngPost
#### les dépendances:
- build-essential (compilateur C++, libstdc++, make,...)
- qt5-default (librairies Qt5 et headers)
- qt5-qmake (pour générer les fichiers moc et créer le Makefile)
- libssl (v1.0.2 or v1.1) mais devrait déjà être installée sur votre système

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


### version Linux portable: AppImage compilée avec Qt v5.12.6, GLIBC 2.24
- téléchargez [ngPost_v4.16-x86_64.AppImage](https://github.com/mbruel/ngPost/releases/download/v4.16/ngPost_v4.16-x86_64.AppImage)
- chmod 755 ngPost_v4.16-x86_64.AppImage
- si vous le lancez sans paramètres, l'interface graphique s'ouvrira, sinon c'est en mode ligne de commande. (cf ngPost --help -l fr)
- pour le fichier de configuration, éditez le fichier **~/.ngPost** et copiez [ce modèle](https://raw.githubusercontent.com/mbruel/ngPost/master/ngPost_fr.conf) (ne pas mettre l'extension .conf!)

PS: pour des systèmes plus vieux GLIBC < 2.24, voici une version compilée sous Debian8 avec GLIBC 2.19 et Qt v5.8.0: [ngPost_v4.16-x86_64_debian8.AppImage](https://github.com/mbruel/ngPost/releases/download/v4.16/ngPost_v4.16-x86_64_debian8.AppImage)


### version Raspbian portable (armhf pour Raspberry PI)
- téléchargez [ngPost_v4.16-armhf.AppImage](https://github.com/mbruel/ngPost/releases/download/v4.16/ngPost_v4.16-armhf.AppImage)
- chmod 755 ngPost_v4.16-armhf.AppImage
- si vous le lancez sans paramètres, l'interface graphique s'ouvrira, sinon c'est en mode ligne de commande. (cf ngPost --help -l fr)
- pour le fichier de configuration, éditez le fichier **~/.ngPost** et copiez [ce modèle](https://raw.githubusercontent.com/mbruel/ngPost/master/ngPost_fr.conf) (ne pas mettre l'extension .conf!)

**Comme rar n'est pas disponible sur RPI, le support de 7zip a été ajouté.** Faites pointer RAR_PATH sur 7z et si besoin (pas utile) utilisez RAR_EXTRA pour les options de 7z. Vous devriez avoir ceci:
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
- Utilisez l'installeur [ngPost_v4.16_x64_setup.exe](https://github.com/mbruel/ngPost/releases/download/v4.16/ngPost_v4.16_x64_setup.exe) ou [ngPost_v4.16_x86_setup.exe](https://github.com/mbruel/ngPost/releases/download/v4.16/ngPost_v4.16_x86_setup.exe) pour la version 32bit
- lancez l'application **ngPost.exe**, l'interface graphique s'ouvrira. Changez tous vos paramètres dont la langue puis cliquez sur **sauver**
- vous pouvez bien sûr ensuite l'utiliser en ligne de commande. cf ngPost --help

**Know Issue with non SSL support:** you may need to install [msvc2015 redistribuables](https://www.microsoft.com/en-us/download/details.aspx?id=48145) as libssl depends on its APIs<br/>


### MacOS release built on High Sierra (v10.13)
- téléchargez [ngPost_v4.16.dmg](https://github.com/mbruel/ngPost/releases/download/v4.16/ngPost_v4.16.dmg)
- si vous le lancez sans paramètres, l'interface graphique s'ouvrira, sinon c'est en mode ligne de commande. (cf ngPost --help -l fr)
- pour le fichier de configuration, éditez le fichier **~/.ngPost** et copiez [ce modèle](https://raw.githubusercontent.com/mbruel/ngPost/master/ngPost_fr.conf) (ne pas mettre l'extension .conf!)


### Alternatives

Voici une liste des posteurs alternatif sur le [github de Nyuu](https://github.com/animetosho/Nyuu/wiki/Usenet-Uploaders).



### Licence
**ngPost** est publié sous licence **GPL v3 **
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
n'hésitez pas à m'envoyer un mail
- si vous avez un problème pour compiler ou lancer ngPost
- si vous avez des commentaires sur le code, des questions sur l'implémentation ou des propositions d'amélioration.
- si vous avez des idées pour de nouvelles fonctionalités.
<br/>
Voici mon email: Matthieu.Bruel@gmail.com


### Supported Languages
Pour l'instant ngPost est traduit en Anglais, Allemand, Chinois, Espagnol, Français et Portugais.<br/>
Si vous shouhaitez le traduire dans une autre langue, c'est très simple (Qt fourni un GUI pour cela: QtLinguist), contactez moi pour plus d'information (Matthieu.Bruel@gmail.com)


### Thanks
- Uukrull for his intensive testing and feedbacks and for building all the MacOS packages.
- awsms for his testing on proper server with a 10Gb/s connection that helped to improve ngPost's upload speed and the multi-threading support
- animetosho for having developped ParPar, the fasted par2 generator ever!
- demanuel for the dev of NewsUP that was my first poster
- noobcoder1983 for the German translation
- tiriclote for the Spanish translation
- hunesco for the Portuguese translation
- Peng for the Chinese translation
- all ngPost users ;)


### Donations
Je suis Freelance (auto-entrepreneur) depuis fin 2019, travaillant sur plusieurs projets perso. Si vous utilisez ngPost et que vous souhaitez contribuer à l'effort et sa future évolution, merci de penser à faire une petite donation.<br/>
<br/>
<a href="https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=W2C236U6JNTUA&item_name=ngPost&currency_code=EUR"><img align="left" src="https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif" alt="ex0days"></a>
 ou en Bitcoin à cette adresse: **3BGbnvnnBCCqrGuq1ytRqUMciAyMXjXAv6**
<img align="right" align="bottom" width="120" height="120" src="https://raw.githubusercontent.com/mbruel/ngPost/master/pics/btc_qr.gif" alt="ngPost_QR">

