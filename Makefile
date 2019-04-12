CCFLAGS = -Wall -std=gnu99 -O3
LFLAGS = -lm libfftw3.a

all: mdfourier mdwave

mdfourier: mdfourier.o incbeta.o
	$(CC) $(CCFLAGS) -o $@ $^ $(LFLAGS)

mdwave: mdwave.o
	$(CC) $(CCFLAGS) -o $@ $^ $(LFLAGS)

.c.o:
	$(CC) -c $(CCFLAGS) $< -o $@

clean:
	rm -f *.o
	rm -f *.exe
