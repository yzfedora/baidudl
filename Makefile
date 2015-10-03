PROGS	= bdpandl
OBJS	= dlinfo.o dlpart.o dlcommon.o scrolling_display.o
FLAGS	= -Wall -lpthread -lerr
CC	= gcc


ALL: $(PROGS)

debug: ALL
debug: FLAGS+=-g

%.o: %.c
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
