# Usage:
# Assuming you want to share the "files" subdirectory
# and your ngPost config is ngPost.docker.conf.
# $ docker build -t ngpost .
# $ docker run -it -v $PWD/files:/root/files -v $PWD/ngPost.docker.conf:/root/.ngPost ngpost ARGUMENTS

FROM debian:10

RUN sed -i 's/main$/main non-free/' /etc/apt/sources.list
RUN apt-get update && apt-get install --no-install-recommends -y \
    git build-essential qt5-qmake qt5-default par2 rar ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY . /usr/src/ngPost
WORKDIR /usr/src/ngPost/src

ENV QT_SELECT=qt5-x86_64-linux-gnu
RUN git clean -fx && qmake && make -j$(nproc)
RUN ln -s /usr/src/ngPost/src/ngPost /usr/local/bin/ngPost

WORKDIR /root
VOLUME /root/files

ENTRYPOINT [ "ngPost" ]
