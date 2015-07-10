PROGS	= bdpandl
OBJS	= err_handler.o
FLAGS	= -Wall -lpthread
CC	= gcc


ALL: $(PROGS)

debug: ALL
debug: FLAGS+=-g -D__DEBUG__

%.o: %.c %.h
	$(CC) -c $^ $(FLAGS)

bdpandl: bdpandl.c $(OBJS)
	$(CC) -o $@ $^ $(FLAGS)

.PHONY: clean
clean:
	$(RM) $(OBJS) $(PROGS) $(wildcard *.h.gch)

install:
	cp $(PROGS) /usr/bin

uninstall:
	$(RM) /usr/bin/$(PROGS)
