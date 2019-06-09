CC = gcc
OPT = -O3

MINGW_CFLAGS = -I/usr/local/include
MINGW_LIB = -L/usr/local/lib -Wl,-Bstatic

#Uncomment the next two lines for MINGW
#EXTRA_CFLAGS = $(MINGW_CFLAGS)
#EXTRA_LFLAGS = $(MINGW_LIB)

CCFLAGS = $(EXTRA_CFLAGS) -Wfatal-errors -Wpedantic -Wall -std=gnu99 $(OPT)
LFLAGS = $(EXTRA_LFLAGS) -lm -lfftw3 -lplot -lpng -lz

all: mdfourier mdwave

mdfourier: sync.o freq.o windows.o log.o diff.o cline.o plot.o incbeta.o balance.o mdfourier.o 
	$(CC) $(CCFLAGS) -o $@ $^ $(LFLAGS)

mdwave: sync.o freq.o windows.o log.o cline.o plot.o incbeta.o balance.o mdwave.o
	$(CC) $(CCFLAGS) -o $@ $^ $(LFLAGS)

.c.o:
	$(CC) -c $(CCFLAGS) $< -o $@

clean:
	rm -f *.o
	rm -f *.exe
