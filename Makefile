PROGS	:= bdpandl
TOOLS	:= bcmp bsearch repair
OBJS	:= dlinfo.o dlpart.o dlcommon.o scrolling_display.o
CFLAGS	:= -Wall -lpthread -lerr
CC	:= gcc


ALL: $(PROGS) $(TOOLS)

debug: ALL
debug: CFLAGS+=-g


%.o: %.c
	$(CC) -c $^ $(CFLAGS)

bdpandl: bdpandl.c $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

bcmp: bcmp.c
	$(CC) -o $@ $^ -lerr
bsearch: bsearch.c
	$(CC) -o $@ $^ -lerr
repair: repair.c dlcommon.o
	$(CC) -o $@ $^ -lerr

.PHONY: ALL debug clean install uninstall
clean:
	$(RM) $(OBJS) $(PROGS) $(TOOLS) $(wildcard *.h.gch)

install:
	cp $(PROGS) /usr/bin

uninstall:
	$(RM) /usr/bin/$(PROGS)
