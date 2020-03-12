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

executable: mdfourier mdwave

#extra flags for static release
static: CCFLAGS = $(EXTRA_MINGW_CFLAGS) $(OPT) $(EXTRA_CFLAGS_STATIC) $(BASE_CCFLAGS)
static: LFLAGS = $(EXTRA_MINGW_LFLAGS) $(EXTRA_LFLAGS_STATIC) $(BASE_LFLAGS)
static: executable


#extra flags for debug
debug: CCFLAGS += -DDEBUG -g
debug: executable

mdfourier: sync.o freq.o windows.o log.o diff.o cline.o plot.o balance.o incbeta.o flac.o mdfourier.o 
	$(CC) $(CCFLAGS) -o $@ $^ $(LFLAGS)

mdwave: sync.o freq.o windows.o log.o diff.o cline.o plot.o incbeta.o balance.o flac.o mdwave.o
	$(CC) $(CCFLAGS) -o $@ $^ $(LFLAGS)

.c.o:
	$(CC) -c $(CCFLAGS) $< -o $@

clean:
	rm -f *.o
	rm -f *.exe
	rm mdfourier
	rm mdwave
