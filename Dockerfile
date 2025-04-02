FROM ubuntu:24.10
RUN mkdir -p /rosbe
RUN mkdir -p /rosbe/build
WORKDIR /rosbe

RUN ln -fs /usr/share/zoneinfo/America/Los_Angeles /etc/localtime
RUN apt-get update -y
RUN apt-get install -y --no-install-recommends tzdata
RUN apt-get install -y gcc g++ autoconf automake info git make libgmp-dev curl bzip2 libmpfr-dev ssh libelf-dev

RUN mkdir -p gnu

RUN curl -o gnu/binutils-2.16.1.tar.bz2 https://ftp.gnu.org/pub/gnu/binutils/binutils-2.16.1.tar.bz2
RUN curl -o gnu/gcc-4.1.0.tar.bz2 https://ftp.gnu.org/pub/gnu/gcc/gcc-4.1.0/gcc-4.1.0.tar.bz2
RUN (cd gnu && git clone https://github.com/Alexpux/mingw-w64)
RUN (cd gnu/mingw-w64 && git checkout d0d7f78)
RUN (cd gnu && bzip2 -d < binutils-2.16.1.tar.bz2 | tar xvf -)
RUN (cd gnu && bzip2 -d < gcc-4.1.0.tar.bz2 | tar xvf -)

COPY gnu/binutils-2.16.1.diff gnu
COPY gnu/gcc-4.1.0.diff gnu
COPY gnu/mingw-w64.diff gnu

RUN (cd gnu/gcc-4.1.0 && patch -p1 < ../gcc-4.1.0.diff)
RUN (cd gnu/binutils-2.16.1 && patch -p1 < ../binutils-2.16.1.diff)

COPY ldscript /rosbe
COPY install.sh /rosbe
ADD elfpe /rosbe/elfpe
ADD gnu/crtdll.def /rosbe/gnu
ADD gnu/msvcrt.def /rosbe/gnu

RUN /bin/bash install.sh /build binutils
RUN /bin/bash install.sh /build gcc
RUN /bin/bash install.sh /build mingw
RUN /bin/bash install.sh /build finish

ENV INSTALLDIR=/build
