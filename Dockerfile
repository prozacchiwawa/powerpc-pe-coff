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

RUN /bin/bash install.sh /build

ENV INSTALLDIR=/build

RUN (cd gnu/mingw-w64 && patch -p1 < ../mingw-w64.diff)
RUN (cd gnu/mingw-w64 && cp -r mingw-w64-headers /build/lib/)

RUN (cd gnu/mingw-w64/mingw-w64-headers && ./configure --prefix=/build --target=powerpcle-unknown-elf)
RUN cp -r gnu/mingw-w64/mingw-w64-headers/* /build/lib/mingw-w64-headers
RUN (PATH="${PATH}:/build/ovr:/build/bin" && cd gnu/mingw-w64 && CFLAGS="-I/build/lib/mingw-w64-headers/include -I/build/lib/mingw-w64-headers/crt -I/build/mingw-w64-headers" ./configure --prefix=/build --target=powerpcle-unknown-elf --disable-lib64)

RUN mkdir -p /build/lib/mingw-w64-headers/sdks && touch /build/lib/mingw-w64-headers/sdks/_mingw_directx.h /build/lib/mingw-w64-headers/sdks/_mingw_ddk.h
RUN mkdir -p gnu/mingw-w64/mingw-w64-crt/lib32
RUN (PATH="${PATH}:/build/ovr:/build/bin" && cd gnu/mingw-w64 && make CC=powerpcle-unknown-elf-gcc AR=powerpcle-unknown-elf-ar RANLIB=powerpcle-unknown-elf-ranlib CFLAGS="-I/build/lib/mingw-w64-headers/direct-x/include/" DLLTOOL=powerpcle-unknown-elf-dlltool CCAS="powerpcle-unknown-elf-gcc -D__powerpc__" DLLTOOLFLAGS32="" LD=powerpcle-unknown-elf-ld AS=powerpcle-unknown-elf-as install)
RUN cp /build/lib32/libcrtdll.a /build/lib32/libcrtdll_save_mingw.a
RUN cp /build/lib32/libmsvcrt.a /build/lib32/libmsvcrt_save_mingw.a
ADD gnu/crtdll.def /rosbe/gnu
RUN (PATH="${PATH}:/build/ovr:/build/bin" && powerpcle-unknown-elf-dlltool -d gnu/crtdll.def -l /build/lib32/libcrtdll.a)
ADD gnu/msvcrt.def /rosbe/gnu
RUN (PATH="${PATH}:/build/ovr:/build/bin" && powerpcle-unknown-elf-dlltool -d gnu/msvcrt.def -l /build/lib32/libmsvcrt.a)
