#OS and architecture check
CC     = gcc
ARCH = $(shell uname -p)

ifeq ($(shell uname),Darwin)
        ifeq ($(ARCH),arm)
                UNAME = DarwinARM
        endif
        ifeq ($(ARCH),powerpc)
                UNAME = DarwinPPC
        endif
        ifeq ($(ARCH),i386)
                UNAME = Darwin
        endif
else
        UNAME := $(shell uname -o)
		ifeq ($(UNAME),Msys)
			MSYSENV := $(shell echo $$MSYSTEM)
			ifeq ($(MSYSENV),CLANG64)
				CC = clang
				MSYS_LD_CLANG = -lwinpthread
			endif
		endif
endif

$(info Building MDFourier for $(UNAME))

ifeq ($(UNAME),GNU/Linux)
all:linux
endif

ifeq ($(UNAME),Msys)
all:msys-s
endif

ifeq ($(UNAME),Cygwin)
all:cygwin
endif

ifeq ($(UNAME),Darwin)
all:mac
endif

ifeq ($(UNAME),DarwinARM)
all:macarm
endif

ifeq ($(UNAME),DarwinPPC)
all:macppc
endif

OPT    = -O3
OPENMP = -DOPENMP_ENABLE -fopenmp

BASE_CCFLAGS    = -Wstrict-prototypes -Wfatal-errors -Wpedantic -Wall -Wextra -std=gnu99
BASE_LIBS       = -lm -lfftw3 -lplot -lpng -lz -lFLAC $(MSYS_LD_CLANG)

#-Wfloat-equal -Wconversion

#For local builds
EXTRA_CFLAGS_SYMBOLS  = -fdata-sections -ffunction-sections
EXTRA_FLAC_STATIC     = -DFLAC__NO_DLL
EXTRA_LFLAGS_SYMBOLS  = -Wl,--gc-sections -Wl,--strip-all
EXTRA_LFLAGS_STATIC   = -Wl,-Bstatic

#generic release
all: CCFLAGS    = $(BASE_CCFLAGS) $(OPT)
all: LFLAGS     = $(BASE_LIBS)
all: executable

#extra flags for static release
msys-s: CCFLAGS = $(BASE_CCFLAGS) $(OPT) $(EXTRA_FLAC_STATIC) $(EXTRA_CFLAGS_SYMBOLS) $(OPENMP)
msys-s: LFLAGS  = $(EXTRA_LFLAGS_STATIC) $(EXTRA_LFLAGS_SYMBOLS) $(BASE_LIBS) 
msys-s: executable

#extra flags for static with checks for testing in MSYS
check: CCFLAGS  = $(BASE_CCFLAGS) $(OPT) $(EXTRA_FLAC_STATIC) $(EXTRA_CFLAGS_SYMBOLS) $(OPENMP) -DDEBUG
check: LFLAGS   = $(EXTRA_LFLAGS_STATIC) $(EXTRA_LFLAGS_SYMBOLS) $(BASE_LIBS) 
check: executable

#Linux/Un*x release
linux: CCFLAGS  = $(BASE_CCFLAGS) $(OPT) $(EXTRA_CFLAGS_SYMBOLS) $(OPENMP)
linux: LFLAGS   = $(EXTRA_LFLAGS_SYMBOLS) $(BASE_LIBS) 
linux: executable

#Cygwin release
cygwin: CCFLAGS = $(BASE_CCFLAGS) $(OPT) $(OPENMP)
cygwin: LFLAGS = $(BASE_LIBS) -Wl,--strip-all
cygwin: executable

#flags for mac
mac: CCFLAGS    = $(BASE_CCFLAGS) $(OPT)
mac: LFLAGS     = -Wl,-no_compact_unwind -logg $(BASE_LIBS)
mac: executable

#flags for apple silicon mac
macarm: CCFLAGS    = -I/opt/homebrew/include $(BASE_CCFLAGS) $(OPT)
macarm: LFLAGS     = -L/opt/homebrew/lib -Wl,-no_compact_unwind -logg $(BASE_LIBS)
macarm: executable

#flags for powerpc mac
macppc: CCFLAGS    = -I/usr/X11R6/include/ $(filter-out -Wpedantic,$(BASE_CCFLAGS)) $(OPT)
macppc: LFLAGS     = -L/usr/X11R6/lib/ -Wl,-logg $(BASE_LIBS) -lx11 -lxext -lxt -lmx -lxaw -lsm -lice -lxmu -lxpm
macppc: executable

#flags for debug
debug: CCFLAGS  = $(BASE_CCFLAGS) -DDEBUG -g 
debug: LFLAGS   = $(BASE_LIBS)
debug: executable

#flags for debug
debugsan: CCFLAGS  = $(BASE_CCFLAGS) -DDEBUG -g -fsanitize=address
debugsan: LFLAGS   = $(BASE_LIBS)
debugsan: executable

executable: mdfourier
executable: mdwave

mdfourier: profile.o sync.o freq.o windows.o log.o diff.o cline.o plot.o balance.o incbeta.o loadfile.o flac.o mdfourier.o 
	$(CC) $(CCFLAGS) -o $@ $^ $(LFLAGS)

mdwave: profile.o sync.o freq.o windows.o log.o diff.o cline.o plot.o incbeta.o balance.o loadfile.o flac.o mdwave.o
	$(CC) $(CCFLAGS) -o $@ $^ $(LFLAGS)

.c.o:
	$(CC) -c $(CCFLAGS) $< -o $@

clean:
	rm -f *.o
	rm -f mdfourier.exe
	rm -f mdwave.exe
	rm -f mdfourier
	rm -f mdwave
