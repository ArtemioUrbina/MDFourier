CC = gcc
OPT = -O3

BASE_CCFLAGS = -Wfatal-errors -Wpedantic -Wall -std=gnu99
BASE_LFLAGS = -lm -lfftw3 -lplot -lpng -lz -lFLAC

#For local builds
EXTRA_MINGW_CFLAGS = -I/usr/local/include 
EXTRA_MINGW_LFLAGS = -L/usr/local/lib 
EXTRA_CFLAGS_STATIC = -fdata-sections -ffunction-sections 
EXTRA_LFLAGS_STATIC = -Wl,-Bstatic -Wl,--gc-sections -Wl,--strip-all

#extra flags for release
all: CCFLAGS = $(BASE_CCFLAGS) $(OPT)
all: LFLAGS = $(BASE_LFLAGS)
all: executable

#extra flags for static release
static: CCFLAGS = $(EXTRA_MINGW_CFLAGS) $(OPT) $(EXTRA_CFLAGS_STATIC) $(BASE_CCFLAGS)
static: LFLAGS = $(EXTRA_MINGW_LFLAGS) $(EXTRA_LFLAGS_STATIC) $(BASE_LFLAGS)
static: executable

#extra flags for mac
mac: CCFLAGS = $(BASE_CCFLAGS) $(OPT)
mac: LFLAGS = $(BASE_LFLAGS) -Wl,-no_compact_unwind -logg
mac: executable

#extra flags for debug
debug: CCFLAGS = $(EXTRA_MINGW_CFLAGS) -g $(BASE_CCFLAGS)
debug: LFLAGS = $(EXTRA_MINGW_LFLAGS) $(BASE_LFLAGS)
debug: executable

executable: mdfourier mdwave


#extra flags for debug
debug: CCFLAGS += -DDEBUG -g
debug: executable

mdfourier: profile.o sync.o freq.o windows.o log.o diff.o cline.o plot.o balance.o incbeta.o loadfile.o flac.o mdfourier.o 
	$(CC) $(CCFLAGS) -o $@ $^ $(LFLAGS)

mdwave: profile.o sync.o freq.o windows.o log.o diff.o cline.o plot.o incbeta.o balance.o loadfile.o flac.o mdwave.o
	$(CC) $(CCFLAGS) -o $@ $^ $(LFLAGS)

.c.o:
	$(CC) -c $(CCFLAGS) $< -o $@

clean:
	rm -f *.o
	rm -f *.exe
	rm mdfourier
	rm mdwave
