## Launch the creation of the docker image to build the ngPost appImage on Debian11
mb@deb12:~/hpmg/docker$ ./build_docker.sh <br />
or directly (in ngPost/src folder)
```
docker build -t ngpost_deb11 .
```
## List the images to get its ID (here 4bd163abe4bd)
```
mb@mbruelGT:~/Documents/github/docker_build_image$ docker images
REPOSITORY                       TAG           IMAGE ID       CREATED             SIZE
ngpost_deb11                     latest        7936bc8d34e5   About an hour ago   849MB
mbruel/ngpost_deb11              dev_v5.0      7936bc8d34e5   About an hour ago   849MB
```

## Jump in the docker mounting a local folder (by example ${HOME}/Downloads folder /down) 
use the ngpost_deb11 IMAGE ID (7936bc8d34e5 in the example)<br />
with this command you won't be root

`mb@deb12:~/hpmg/docker$ docker run -it --name ngpost_dev -v ${HOME}/Downloads:/down 7936bc8d34e5`<br />

**To be root:** you must add **--user root**<br />
`docker run -it --user root --name ngpost_root -v ${HOME}/Downloads:/down 7936bc8d34e5`

**Usefull bash function and aliases:**
```
function docker_launch()
{
	# Syntax (2 arguments)
	if [ $# -ne 2 ]; then
		echo "Error Syntax: docker_launch <IMAGE_REPO> <CONTAINER_NAME>"
		echo "(if the CONTAINER_NAME finishes by _root, --user root will be added ;)"
		echo "  - ex as user: docker_launch ngpost_deb11 ngpost_dev"
		echo "  - ex as root: docker_launch ngpost_deb11 ngpost_root"
		return
	fi

	IMAGE_REPO=$1
	CONTAINER_NAME=$2
	AS_ROOT=""

	# Check if we wish to be root
	if [ "${CONTAINER_NAME: -5}" = "_root" ]; then
		echo "the container will run as root"
		AS_ROOT="--user root"
	else
		echo "the container will run as user"
	fi

	# get the latest image id of the repo
	image_id=`docker images | grep $IMAGE_REPO | grep latest | awk '{print $3;}'`

	# is the container running already?
	nbRunningContainer=`docker ps -a | grep $CONTAINER_NAME | wc -l`

	if [ $nbRunningContainer -eq 1 ]; then
		echo "Restart and attach container $CONTAINER_NAME for image $IMAGE_REPO (id: $image_id)"
		docker start $CONTAINER_NAME && docker attach $CONTAINER_NAME
	else
		echo "Creating container $CONTAINER_NAME for image $IMAGE_REPO (id: $image_id)"
		docker run -it $AS_ROOT --name $CONTAINER_NAME -v /home/mb/Documents/github/:/github -v ${HOME}/Downloads:/down $image_id
	fi
}

alias dockers_list='docker ps -a'
alias dockers_start='docker ps --filter status=exited -q | xargs docker start'
alias dockers_rm='docker rm $(docker ps --filter status=exited -q)'
```

## Examples of use
```
mb@mbruelGT:~/Downloads$ docker_launch ngpost_deb11 ngpost_dev
the container will run as user
Creating container ngpost_dev for image ngpost_deb11 (id: 7936bc8d34e5)
mb@09582036d2c5:~$ exit
exit
mb@mbruelGT:~/Downloads$ docker_launch ngpost_deb11 ngpost_dev
the container will run as user
Restart and attach container ngpost_dev for image ngpost_deb11 (id: 7936bc8d34e5)
ngpost_dev
mb@09582036d2c5:~$ ngPost --check qt-unified-linux-x64-4.4.2-online.nzb 
Using default config file: /home/mb/.ngPost
Group Policy EACH_POST: each post on a different group
Generate new random poster for each post
[2024-06-02 21:04:17] ngPost starts logging
qt-unified-linux-x64-4.4.2-online.nzb has 92 articles
Start checking the nzb with 17 connections on 1 servers)
There is a new version available on GitHUB: v "4.16"  (visit https://github.com/mbruel/ngPost/ to get it)

[==================================================] 100 % (92 / 92) missing articles: 0


Nb Missing Article(s): 0/92 (check done in 00:00:01.034 (1 sec) using 17 connections on 1 server(s))
mb@09582036d2c5:~$ 
```


