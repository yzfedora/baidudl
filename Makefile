PROGS	= bdpandl
TOOLS	= bcmp bsearch
OBJS	= dlinfo.o dlpart.o dlcommon.o scrolling_display.o
FLAGS	= -Wall -lpthread -lerr
CC	= gcc


ALL: $(PROGS) $(TOOLS)

debug: ALL
debug: FLAGS+=-g

%.o: %.c
	$(CC) -c $^ $(FLAGS)

bdpandl: bdpandl.c $(OBJS)
	$(CC) -o $@ $^ $(FLAGS)

bcmp: bcmp.c
	$(CC) -o $@ $^ -lerr

bsearch: bsearch.c
	$(CC) -o $@ $^ -lerr

.PHONY: clean
clean:
	$(RM) $(OBJS) $(PROGS) $(TOOLS) $(wildcard *.h.gch)

install:
	cp $(PROGS) /usr/bin

uninstall:
	$(RM) /usr/bin/$(PROGS)
