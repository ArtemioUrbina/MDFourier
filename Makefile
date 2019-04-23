CC = gcc
CCFLAGS = -Wfatal-errors -Wpedantic -Wall -std=gnu99 -O3
LFLAGS = -lm -lfftw3 -lplot

all: mdfourier mdwave

mdfourier: freq.o windows.o log.o diff.o plot.o cline.o incbeta.o mdfourier.o 
	$(CC) $(CCFLAGS) -o $@ $^ $(LFLAGS)

mdwave: freq.o windows.o log.o cline.o mdwave.o
	$(CC) $(CCFLAGS) -o $@ $^ $(LFLAGS)

.c.o:
	$(CC) -c $(CCFLAGS) $< -o $@

clean:
	rm -f *.o
	rm -f *.exe
