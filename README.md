# MDFourier

MDFourier is an open source software solution created to compare audio signatures and generate a series of graphs that show how they differ. The software consists of two separate programs, one that produces the signal for recording from the console and the other that analyses and displays the audio comparisons.

The information gathered from the comparison results can be used in a variety of ways: to identify how audio signatures vary between systems, to detect if the audio signals are modified by audio equipment, to find if modifications resulted in audible changes, to help tune emulators, FPGA implementations or mods, etc.

## Compiling the source 
MDFourier needs a few libraries to be compiled. In Linux, UN*X based systems and MinGW2; you can link it against the latest versions of the libraries.

- Fastest Fourier Transform in the West. (fftw): http://www.fftw.org/download.html
- The GNU plotutils package: https://ftp.gnu.org/gnu/plotutils/
- PNG Reference Library: libpng http://www.libpng.org/pub/png/libpng.html
- FLAC Reference Library: libFLAC https://ftp.osuosl.org/pub/xiph/releases/flac/

The following implementations are also used and included with the source files:

- sort.h for tim sort.
- Incomplete Beta Function.

The pre-compiled executable of the Analysis Software for Windows is created with MSYS2 UCRT64, and statically linked for distribution against these libraries:

- fftw 3.3.10-5
- plotutils-2.6
- libpng 1.6.53-1
- flac-1.5.0

The makefiles to compile either version are provided with the source code. 

Please read the documentation available at http://junkerhq.net/MDFourier/

### Compiling on macOS
#### Intel and ARM architecture

If you haven't installed XCode or the Command Line Tools already, open a Terminal and just run `make`.

A prompt will ask you to install the Command Line Tools, say yes and it will install all the necessary development tools without installing the whole XCode package.

Install [Homebrew](https://brew.sh), follow the instructions.

Then install the following libraries using `brew install <package name>`
- fttw
- plotutils
- libpng
- flac
- libogg

Dependencies will be automatically resolved and installed by Homebrew.

Clone this repository and run `make`

#### PowerPC architecture

Install XCode 2.5

Install X11 from your Mac OS X installation DVD/CD (Look for "Optional Installs" package)

Run the system updater to make sure you have the last version of XCode and X11

Install [Tigerbrew](https://github.com/mistydemeo/tigerbrew/) and carefully follow the instructions.

Then install the following libraries using `brew install <package name>`
- fttw
- plotutils
- libpng
- flac
- libogg

Dependencies will be automatically resolved and installed by Tigerbrew.

Clone this repository and run `make`

#### Making redistributable static binaries on macOS

Please refer to [this post](https://donluca.theclassicgamer.net/compiling-static-binaries-on-macos/) for a guide on how to create a redistributable MDFourier binary with the libraries needed statically linked.

### Notes for compiling under MSYS2/MinGW

- Install MSYS2
- Use the UCRT64 version

#### Compiler and libraries
Run the following commands to install all tools and libraries that work for our static buils:

- pacman -Syu
- pacman -S make
- pacman -S mingw-w64-ucrt-x86_64-toolchain
- pacman -S mingw-w64-ucrt-x86_64-fftw
- pacman -S mingw-w64-ucrt-x86_64-libpng

#### plotutils-2.6

These need to be patched, I have a pre-patched version if you prefer.

##### Patching it yourself

Download from: https://ftp.gnu.org/gnu/plotutils/plotutils-2.6.tar.gz

Following the instructions from: https://stackoverflow.com/questions/39861615/plotutils-compilation-error-with-png-1-6-25-dereferencing-pointer-to-incomplete

Edit file plotutils-2.6\libplot\z_write.c and change both occurances of:

        png_ptr->jmpbuf

to:

        png_jmpbuf(png_ptr)

##### Pre-patched

Download from: https://github.com/ArtemioUrbina/Plotutils

And then run:

./configure --prefix=/ucrt64 --enable-static --disable-shared LDFLAGS=-static
make
make install


#### libFlac 1.5.0:

In order to build it statically:

- pacman -S autoconf automake pkg-config libtool
- Download libFLAC https://ftp.osuosl.org/pub/xiph/releases/flac/
- ./autogen.sh
- ./configure --prefix=/ucrt64 --enable-static --disable-shared --disable-ogg --disable-doxygen
- make
- make install
