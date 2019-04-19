CCFLAGS = -Wfatal-errors -Wpedantic -Wall -std=gnu99 -g
LFLAGS = -lm libfftw3.a

all: mdfourier mdwave

mdfourier: freq.o windows.o log.o incbeta.o cline.o mdfourier.o 
	$(CC) $(CCFLAGS) -o $@ $^ $(LFLAGS)

mdwave: mdwave.o
	$(CC) $(CCFLAGS) -o $@ $^ $(LFLAGS)

.c.o:
	$(CC) -c $(CCFLAGS) $< -o $@

clean:
	rm -f *.o
	rm -f *.exe
