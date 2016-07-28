CC := gcc
CFLAGS := -O3 -std=gnu11
CFLAGS += -march=native -mtune=native -fomit-frame-pointer
LDFLAGS :=

all:	occupy

occupy.o:	occupy.c makefile
	$(CC) $(CFLAGS) $< -c -o $@

occupy:	occupy.o
	$(CC) $< -o $@ $(LDFLAGS) 

clean:
	rm -f occupy *.o

