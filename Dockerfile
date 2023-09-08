FROM ubuntu:22.04

# disable tzdata questions
ENV DEBIAN_FRONTEND=noninteractive

# use bash
SHELL ["/bin/bash", "-c"]

# install apt-utils
RUN apt-get update -y && \
  apt-get install -y apt-utils 2> >( grep -v 'debconf: delaying package configuration, since apt-utils is not installed' >&2 ) \
  && apt-get clean && rm -rf /var/lib/apt/lists/*

# essential tools
RUN apt-get update -y && apt-get install -y --no-install-recommends \
  ca-certificates \
  netbase \
  curl \
  git \
  gnupg \
  && apt-get clean && rm -rf /var/lib/apt/lists/*

# copy files
COPY . /tmp/intel-matrix-market
WORKDIR /tmp/intel-matrix-market

# install intel MKL
#RUN bash install-intel-mkl-linux.bash

# download matrix market files
#RUN bash download-from-matrix-market.bash

# configure image for remote development
#RUN bash common-debian.sh
