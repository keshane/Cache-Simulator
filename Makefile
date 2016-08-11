CC = gcc
CFLAGS = -std=c99 -g3 -Wall  -pedantic
CRYPT = -lssl -lcrypto -lz
ifdef OPT
	ifeq ($(OPT), 1)
		CFLAGS += -DOPT1
	endif

	ifeq ($(OPT), 2)
		CFLAGS += -DOPT2
	endif
endif
ifdef DEBUG
	CFLAGS += -DDEBUG
endif
all: cache

cache: cache.o lruqueue.o RHcache.h zcache.h
	$(CC) $(CFLAGS) $^ -o $@  $(CRYPT)

lruqueue.o: lruqueue.c lruqueue.h
	$(CC) $(CFLAGS) -c $^

cache.o: cache.c
	$(CC) $(CFLAGS) -c $^ $(CRYPT)

clean:
	rm cache cache.o lruqueue.o 
