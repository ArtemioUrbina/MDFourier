UNAME_O := $(shell uname -o)
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Darwin)
ifeq ($(UNAME_M),arm64)
all:macarm
else
all:mac
endif
endif

ifeq ($(UNAME_O),GNU/Linux)
all:linux
endif

ifeq ($(UNAME_O),Msys)
all:msys-s
endif

ifeq ($(UNAME_O),Cygwin)
all:cygwin
endif

CC     = gcc
OPT    = -O3
OPENMP = -DOPENMP_ENABLE -fopenmp

BASE_CCFLAGS    = -Wfatal-errors -Wpedantic -Wall -Wextra -std=gnu99
BASE_LIBS       = -lm -lfftw3 -lplot -lpng -lz -lFLAC

#-Wstrict-prototypes -Wfloat-equal -Wconversion

#For local builds
LOCAL_INCLUDE         = -I/usr/local/include
LOCAL_LINK            = -L/usr/local/lib
EXTRA_CFLAGS_SYMBOLS  = -fdata-sections -ffunction-sections
EXTRA_LFLAGS_SYMBOLS  = -Wl,--gc-sections -Wl,--strip-all
EXTRA_LFLAGS_STATIC   = -Wl,-Bstatic

#generic release
all: CCFLAGS    = $(LOCAL_INCLUDE) $(BASE_CCFLAGS) $(OPT)
all: LFLAGS     = $(LOCAL_LINK) $(BASE_LIBS)
all: executable

#extra flags for static release
msys-s: CCFLAGS = $(LOCAL_INCLUDE) $(BASE_CCFLAGS) $(OPT) $(EXTRA_CFLAGS_SYMBOLS) $(OPENMP)
msys-s: LFLAGS  = $(LOCAL_LINK) $(EXTRA_LFLAGS_STATIC) $(EXTRA_LFLAGS_SYMBOLS) $(BASE_LIBS) 
msys-s: executable

#extra flags for static with checks for testing in MSYS
check: CCFLAGS  = $(LOCAL_INCLUDE) $(BASE_CCFLAGS) $(OPT) $(EXTRA_CFLAGS_SYMBOLS) $(OPENMP) -DDEBUG
check: LFLAGS   = $(LOCAL_LINK) $(EXTRA_LFLAGS_STATIC) $(EXTRA_LFLAGS_SYMBOLS) $(BASE_LIBS) 
check: executable

#Linux/Un*x release
linux: CCFLAGS  = $(BASE_CCFLAGS) $(OPT) $(EXTRA_CFLAGS_SYMBOLS) $(OPENMP)
linux: LFLAGS   = $(EXTRA_LFLAGS_SYMBOLS) $(BASE_LIBS) 
linux: executable

#Cygwin release
cygwin: CCFLAGS = $(LOCAL_INCLUDE) $(BASE_CCFLAGS) $(OPT) $(OPENMP)
cygwin: LFLAGS = $(LOCAL_LINK) $(BASE_LIBS) -Wl,--strip-all
cygwin: executable

#flags for mac
mac: CCFLAGS    = $(BASE_CCFLAGS) $(OPT)
mac: LFLAGS     = -Wl,-no_compact_unwind -logg $(BASE_LIBS)
mac: executable

#flags for apple silicon mac
macarm: CCFLAGS    = -I/opt/homebrew/include $(BASE_CCFLAGS) $(OPT)
macarm: LFLAGS     = -L/opt/homebrew/lib -Wl,-no_compact_unwind -logg $(BASE_LIBS)
macarm: executable

#flags for debug
debug: CCFLAGS  = $(LOCAL_INCLUDE) $(BASE_CCFLAGS) -DDEBUG -g
debug: LFLAGS   = $(LOCAL_LINK) $(BASE_LIBS)
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
