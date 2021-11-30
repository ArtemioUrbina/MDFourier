UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
	all:linux
endif

ifeq ($(UNAME_S),Msys)
	all:static
endif

ifeq ($(UNAME_S),Darwin)
	all:mac
endif

CC = gcc
OPT = -O3
OPENMP = -DOPENMP_ENABLE -fopenmp

BASE_CCFLAGS = -Wfatal-errors -Wpedantic -Wall -Wextra -std=gnu99
BASE_LFLAGS = -lm -lfftw3 -lplot -lpng -lz -lFLAC

#-Wstrict-prototypes -Wfloat-equal -Wconversion -Wfloat-equal

#For local builds
EXTRA_MINGW_CFLAGS = -I/usr/local/include
EXTRA_MINGW_LFLAGS = -L/usr/local/lib
EXTRA_CFLAGS_STATIC = -fdata-sections -ffunction-sections
EXTRA_LFLAGS_STATIC = -Wl,-Bstatic -Wl,--gc-sections -Wl,--strip-all

#extra flags for static release
static: CCFLAGS = $(EXTRA_MINGW_CFLAGS) $(OPT) $(EXTRA_CFLAGS_STATIC) $(BASE_CCFLAGS) $(OPENMP)
static: LFLAGS = $(EXTRA_MINGW_LFLAGS) $(EXTRA_LFLAGS_STATIC) $(BASE_LFLAGS)
static: executable

#extra flags for static with checks for testing
check: CCFLAGS = $(EXTRA_MINGW_CFLAGS) $(OPT) $(EXTRA_CFLAGS_STATIC) $(BASE_CCFLAGS) $(OPENMP) -DDEBUG
check: LFLAGS = $(EXTRA_MINGW_LFLAGS) $(EXTRA_LFLAGS_STATIC) $(BASE_LFLAGS)
check: executable

#generic release
all: CCFLAGS = $(BASE_CCFLAGS) $(OPT)
all: LFLAGS = $(BASE_LFLAGS)
all: executable

#Linux/Un*x release
linux: CCFLAGS = $(BASE_CCFLAGS) $(OPT) $(OPENMP)
linux: LFLAGS = $(BASE_LFLAGS)
linux: executable

#flags for mac
mac: CCFLAGS = $(BASE_CCFLAGS) $(OPT)
mac: LFLAGS = $(BASE_LFLAGS) -Wl,-no_compact_unwind -logg
mac: executable

#flags for debug
debug: CCFLAGS = $(EXTRA_MINGW_CFLAGS) $(BASE_CCFLAGS) -DDEBUG -g
debug: LFLAGS = $(EXTRA_MINGW_LFLAGS) $(BASE_LFLAGS)
debug: executable

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
