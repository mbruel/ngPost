# Use the official Debian Bullseye slim image
FROM debian:bullseye-slim

#################################################
# Few variables that will be updated with time! #
#################################################
ENV NGPOST_URL "https://github.com/mbruel/ngPost.git"
ENV NGPOST_BRANCH "dev_v5.0"



#################################################
# Nothing to touch from here :)                 #
#  - ngPost will be clone from github           #
#  - checkout the expected version (if not main)#

#################################################


# Add deb-src lines to /etc/apt/sources.list
RUN echo "deb http://deb.debian.org/debian bullseye main" >> /etc/apt/sources.list && \
    echo "deb-src http://deb.debian.org/debian bullseye main" >> /etc/apt/sources.list && \
    echo "deb http://security.debian.org/debian-security bullseye-security main" >> /etc/apt/sources.list && \
    echo "deb-src http://security.debian.org/debian-security bullseye-security main" >> /etc/apt/sources.list && \
    echo "deb http://deb.debian.org/debian bullseye-updates main" >> /etc/apt/sources.list && \
    echo "deb-src http://deb.debian.org/debian bullseye-updates main" >> /etc/apt/sources.list

# Update package lists again after adding deb-src lines
RUN apt-get update

# [Optional] Uncomment this section to install additional packages.
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends aptitude vim less wget \
    build-essential make g++ git qt5-qmake ca-certificates \ 
    qtbase5-dev qtbase5-dev-tools libqt5sql5 libqt5sql5-sqlite libssl-dev \
    && apt-get clean

# Set environment variables
ENV QT_DIR=/usr/lib/x86_64-linux-gnu/qt5/
ENV QT_QMAKE_EXECUTABLE=/usr/bin/qmake

# Update CA certificates to ensure SSL connections work
RUN update-ca-certificates




# Use a non-root user for the builds. Note: after the "USER" command is executed all commands
# are executed by that user. So dpkg, apt, and other commands that require root access are not
# allowed below this.
RUN useradd --create-home --shell /bin/bash --uid 1000 mb
USER mb

# Set the environment variable of the user (following the default ~/.ngPost)
ENV HOME_DIR /home/mb
ENV BIN_DIR $HOME_DIR/apps/bin/
ENV NZB_DIR $HOME_DIR/Downloads/nzb/

# Set the working directory to the home directory of the user
WORKDIR $HOME_DIR


# Create folders
RUN mkdir -p $BIN_DIR && mkdir -p $NZB_DIR


# Clone the repository and check out a specific version
RUN git clone $NGPOST_URL && \
    cd $HOME_DIR/ngPost && \
    git checkout $NGPOST_BRANCH
    
# build and make ngPost in path ($BIN_DIR)
RUN mkdir $HOME_DIR/ngPost/build && cd $HOME_DIR/ngPost/build && $QT_DIR/bin/qmake -spec linux-g++ CONFIG+=release ../src/ngPost.pro && make -j4 && cp ngPost $BIN_DIR


# copy other files executables (rar and parpar) to BIN_DIR
COPY ./parpar $BIN_DIR
COPY ./rar $BIN_DIR

# copy the configs
COPY ./.bashrc $HOME_DIR/
COPY ./.ngPost $HOME_DIR/


# copy folder to test with: ngPost --pack --auto ~/testNgPost
RUN mkdir -p $HOME_DIR/testNgPost/
COPY ./testNgPost/ ./testNgPost/

# copy to test nzbcheck : ngPost --check ~/qt-unified-linux-x64-4.4.2-online.nzb
COPY ./qt-unified-linux-x64-4.4.2-online.nzb .




