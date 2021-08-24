# MDFourier

MDFourier is an open source software solution created to compare audio signatures and generate a series of graphs that show how they differ. The software consists of two separate programs, one that produces the signal for recording from the console and the other that analyses and displays the audio comparisons.

The information gathered from the comparison results can be used in a variety of ways: to identify how audio signatures vary between systems, to detect if the audio signals are modified by audio equipment, to find if modifications resulted in audible changes, to help tune emulators, FPGA implementations or mods, etc.

## COmpiling the source 
MDFourier needs a few libraries to be compiled. In Linux, UN*X based systems and MinGW2; you can link it against the latest versions of the libraries.

- Fastest Fourier Transform in the West. (fftw)
- The GNU plotutils package.
- PNG Reference Library: libpng.
- FLAC Reference Library: libFLAC.

The following implementations are also used and included with the source files:

- sort.h for tim sort.
- Incomplete Beta Function.

The pre-compiled executable of the Analysis Software for Windows is created with MinGW, and statically linked for distribution against these libraries:

- fftw-3.3.8
- plotutils-2.6
- libpng-1.5.30

The makefiles to compile either version are provided with the source code. 

Please read the documentation available at http://junkerhq.net/MDFourier/
