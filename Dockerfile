FROM ubuntu:20.04

WORKDIR /srv

RUN apt-get update && apt-get install -y \
  build-essential automake libzmq3-dev libncurses-dev

COPY . ./

RUN autoreconf -i && mkdir build && cd build && CXXFLAGS="-O2 -DNDEBUG" ../configure && make

RUN install /srv/build/motrix /usr/local/bin

ENTRYPOINT ["motrix"]
