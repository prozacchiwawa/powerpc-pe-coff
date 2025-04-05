#!/bin/sh

EXE=
if [ x`uname -o` = xCygwin ] ; then
	EXE=.exe
fi

if [ "x$1" = xclean ] ; then
    rm -rf binutils-configure gcc-configure
    exit 0
fi

targets="binutils gcc mingw finish all"
if [ "x$1" = x ] ; then
    echo usage: "$0" "[install-dir] [${targets} | tr ' ' '|']"
    exit 1
fi

idir="$1"
shift

# Get the install steps to do
if [ "x$1" = x ] ; then
    STEP="all"
else
    STEP="$1"
fi

libdir="$idir"/lib
export INSTALLDIR="${idir}"

mkdir -p "${INSTALLDIR}/lib"
mkdir -p "${INSTALLDIR}/ovr"
mkdir -p ./ovr

# Build and install binutils
case "$STEP" in
    binutils)
        if [ ! -d build/binutils-configure ] ; then
            mkdir -p build/binutils-configure || exit 1
        fi
        cd build/binutils-configure || exit 1
        CFLAGS="-Wno-implicit-int -Wno-implicit-function-declaration -Wno-unused-but-set-variable -Wno-pedantic -Wno-incompatible-pointer-types -Wno-expansion-to-defined -Wno-implicit-fallthrough -Wno-shift-negative-value" sh ../../gnu/binutils-2.16.1/configure \
              --prefix="$idir" \
              --target=powerpcle-unknown-elf && \
            make && \
            make -C binutils CFLAGS="-g -DDLLTOOL_PPC" dlltool$EXE && \
            make -C binutils CFLAGS="-g -DDLLTOOL_PPC" windres$EXE && \
            make install && \
            cp binutils/dlltool "$idir"/bin/powerpcle-unknown-elf-dlltool && \
            cp binutils/windres "$idir"/bin/powerpcle-unknown-elf-windres || exit 1
        cd ../..
        # Copy extra files
        cp -r ldscript gnu/mingw-w64/mingw-w64-headers "$libdir" || exit 1
        ;;

    gcc)
        # Build and install gcc
        if [ ! -d build/gcc-configure ] ; then
            mkdir build/gcc-configure || exit 1
        fi
        export PATH="$idir/bin":$PATH
        (cd build/gcc-configure &&

             CFLAGS="-Wno-implicit-int -Wno-implicit-function-declaration -Wno-unused-but-set-variable -Wno-pedantic -Wno-incompatible-pointer-types -Wno-expansion-to-defined -Wno-implicit-fallthrough -Wno-shift-negative-value" sh ../../gnu/gcc-4.1.0/configure \
                   --prefix="$idir" \
                   --target=powerpcle-unknown-elf \
                   --disable-ssp \
                   --disable-threads \
                   --without-headers \
                   --disable-multilib \
                   --enable-languages=c &&

             make configure-host &&
             make all-stage1-gcc &&
             make -C libiberty &&
             make -C libcpp &&
             make -C gcc cpp cc1 xgcc
        )

        (cd build/gcc-configure/gcc && CFLAGS_FOR_TARGET="-fPIE -mlittle" sh ../../../gnu/gcc-4.1.0/gcc/configure --target=powerpcle-unknown-elf --prefix=/build --disable-ssp --disable-threads --without-headers --disable-multilib --enable-languages=c)

        # Make elfpe
        make -C elfpe && cp elfpe/elfpe "${INSTALLDIR}/ovr/powerpcle-unknown-elf-gcc" || exit 1

        (PATH="${PATH}:${INSTALLDIR}/bin" && cd build/gcc-configure/gcc && make && make install)
        ;;

    mingw)
        (cd gnu/mingw-w64 && patch -p1 < ../mingw-w64.diff)
        (cd gnu/mingw-w64 && cp -r mingw-w64-headers /build/lib/)
        (cd gnu/mingw-w64/mingw-w64-headers && ./configure --prefix="${INSTALLDIR}" --target=powerpcle-unknown-elf)
        cp -r gnu/mingw-w64/mingw-w64-headers/* "${INSTALLDIR}/lib/mingw-w64-headers"

        (PATH="${PATH}:${INSTALLDIR}/ovr:${INSTALLDIR}/bin" && cd gnu/mingw-w64 && CFLAGS="-I${INSTALLDIR}/lib/mingw-w64-headers/include -I${INSTALLDIR}/lib/mingw-w64-headers/crt -I${INSTALLDIR}/mingw-w64-headers" ./configure --prefix=${INSTALLDIR} --target=powerpcle-unknown-elf --disable-lib64)

        mkdir -p ${INSTALLDIR}/lib/mingw-w64-headers/sdks && touch ${INSTALLDIR}/lib/mingw-w64-headers/sdks/_mingw_directx.h ${INSTALLDIR}/lib/mingw-w64-headers/sdks/_mingw_ddk.h
        mkdir -p gnu/mingw-w64/mingw-w64-crt/lib32

        (PATH="${PATH}:/build/ovr:/build/bin" && cd gnu/mingw-w64 && make CC=powerpcle-unknown-elf-gcc AR=powerpcle-unknown-elf-ar RANLIB=powerpcle-unknown-elf-ranlib CFLAGS="-I/build/lib/mingw-w64-headers/direct-x/include/" DLLTOOL=powerpcle-unknown-elf-dlltool CCAS="powerpcle-unknown-elf-gcc -D__powerpc__" DLLTOOLFLAGS32="" LD=powerpcle-unknown-elf-ld AS=powerpcle-unknown-elf-as install)

        cp /build/lib32/libcrtdll.a /build/lib32/libcrtdll_save_mingw.a
        cp /build/lib32/libmsvcrt.a /build/lib32/libmsvcrt_save_mingw.a
        (PATH="${PATH}:/build/ovr:/build/bin" && powerpcle-unknown-elf-dlltool -d gnu/crtdll.def -l /build/lib32/libcrtdll.a)
        (PATH="${PATH}:/build/ovr:/build/bin" && powerpcle-unknown-elf-dlltool -d gnu/msvcrt.def -l /build/lib32/libmsvcrt.a)
        ;;

    finish)
        # Make env script
        echo '#!/bin/sh' > "$idir"/rosbe
        echo 'THISDIR=`dirname $0`' >> "$idir"/rosbe
        echo export PATH='"$THISDIR/ovr:$THISDIR/bin:$PATH"' >> "$idir"/rosbe
        echo export 'INSTALLDIR="$THISDIR"' >> "$idir"/rosbe
        echo export OLDMAKE=`which make` >> "$idir"/rosbe
        echo echo "\"Run make in your reactos dir to build\"" >> "$idir"/rosbe
        echo exec $SHELL >> "$idir"/rosbe
        chmod +x "$idir"/rosbe
        echo Run "\"$idir/rosbe\"" to get a shell with the right settings
        # Make wrappers
        cp -r ovr "$idir"
        ;;

    all)
        for target in ${targets} ; do
            ./install.sh "${idir}" "${target}"
        done
        ;;
esac
