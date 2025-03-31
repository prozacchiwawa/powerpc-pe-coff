#!/bin/sh

EXE=
if [ x`uname -o` = xCygwin ] ; then
	EXE=.exe
fi

if [ "x$1" = xclean ] ; then
    rm -rf binutils-configure gcc-configure
    exit 0
fi

if [ "x$1" = x ] ; then
    echo usage: "$0" "[install-dir]"
    exit 1
fi

idir="$1"
libdir="$idir"/lib

# Build and install binutils
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
# # Make elfpe
# make -C elfpe && cp elfpe/elfpe ovr/powerpcle-unknown-elf-gcc || exit 1
# # Make env script
# echo '#!/bin/sh' > "$idir"/rosbe
# echo 'THISDIR=`dirname $0`' >> "$idir"/rosbe
# echo export PATH='"$THISDIR/ovr:$THISDIR/bin:$PATH"' >> "$idir"/rosbe
# echo export 'INSTALLDIR="$THISDIR"' >> "$idir"/rosbe
# echo export OLDMAKE=`which make` >> "$idir"/rosbe
# echo echo "\"Run make in your reactos dir to build\"" >> "$idir"/rosbe
# echo exec $SHELL >> "$idir"/rosbe
# chmod +x "$idir"/rosbe
# echo Run "\"$idir/rosbe\"" to get a shell with the right settings
# # Make wrappers
# cp -r ovr "$idir"
