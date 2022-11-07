# Ubuntu 20.04
FROM ubuntu:20.04

ARG GITHUB_USER
ARG GIT_BRANCH
ENV GITHUB_USER=${GITHUB_USER}
ENV GIT_BRANCH=${GIT_BRANCH}

RUN echo ${GITHUB_USER}

RUN apt-get update && env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake=3.16.3-1ubuntu1 \
        g++=4:9.3.0-1ubuntu2 \
        git=1:2.25.1-1ubuntu3.6 \
        libboost-all-dev=1.71.0.0ubuntu2 \
        libgmp-dev=2:6.2.0+dfsg-4ubuntu0.1 \
        libgtest-dev=1.10.0-2 \
        make=4.2.1-1.2 \
        patch=2.7.6-6 \
 && rm -rf /var/lib/apt/lists/* 

RUN git config --global user.email ${GITHUB_USER} 

RUN git clone https://github.com/rpgoldman/smtrat.git \
 && cd smtrat \
 && git checkout ${GIT_BRANCH} \
 && mkdir build 

# this is in the code now.
# RUN sed -i '16 a GIT_TAG 22.06' smtrat/resources/carl/CMakeLists.txt

RUN cd smtrat/build \
 && cmake .. \
 && make -j $(nproc)
