export CC=gcc
export CXX="g++"
export CFLAGS="-Wall -O2 -fomit-frame-pointer -pipe"
export CXXFLAGS="$CFLAGS"

(./configure --enable-smp \
              --enable-cpu-level=6 \
              --enable-all-optimizations \
              --enable-x86-64 \
              --enable-pci \
              --enable-vmx \
              --enable-debugger \
              --enable-disasm \
              --enable-debugger-gui \
              --enable-logging \
              --enable-fpu \
              --enable-3dnow \
              --enable-sb16=dummy \
              --enable-cdrom \
              --enable-x86-debugger \
              --enable-iodebug \
              --disable-plugins \
              --disable-docbook \
              --with-x --with-x11 --with-term --with-sdl2) || exit

make && make install
