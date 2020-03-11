CC = gcc
OPT = -O3

MINGW_CFLAGS = -I/usr/local/include
MINGW_LIB = -L/usr/local/lib -Wl,-Bstatic

#Uncomment the next two lines for MINGW
#EXTRA_CFLAGS = $(MINGW_CFLAGS)
#EXTRA_LFLAGS = $(MINGW_LIB)

CCFLAGS = $(EXTRA_CFLAGS) -Wfatal-errors -Wpedantic -Wall -std=gnu99
LFLAGS = $(EXTRA_LFLAGS) -lm -lfftw3 -lplot -lpng -lz -lFLAC

#extra flags for release
all: CCFLAGS += -fdata-sections -ffunction-sections $(OPT)
all: LFLAGS += -Wl,--gc-sections -Wl,--strip-all
all: executable

#extra flags for debug
debug: CCFLAGS += -DDEBUG -g
debug: executable

executable: mdfourier mdwave

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
