# Ubuntu 22.04
FROM ubuntu:22.04

ARG GITHUB_USER
ARG GIT_BRANCH
ENV GITHUB_USER=${GITHUB_USER}
ENV GIT_BRANCH=${GIT_BRANCH}
ENV CARL_REQUIRED_VERSION=22.12

RUN echo ${GITHUB_USER}

RUN apt-get update && env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        build-essential \
        git \
        libboost-all-dev \
        libgmp-dev \
        libgtest-dev \
        make \
        patch \
 && rm -rf /var/lib/apt/lists/* 

RUN git config --global user.email ${GITHUB_USER} 

RUN git clone https://github.com/rpgoldman/smtrat.git \
 && cd smtrat \
 && git checkout ${GIT_BRANCH} \
 && mkdir build 

#RUN sed -i '16 a GIT_TAG 22.12' smtrat/resources/carl/CMakeLists.txt

RUN cd smtrat/build \
 && cmake .. \
 && make -j $(nproc)
