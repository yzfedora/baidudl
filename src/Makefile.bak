PROGS	:= bdpandl
TOOLS	:= bcmp bsearch repair bdpandl-decode
OBJS	:= dlinfo.o dlpart.o dlcommon.o dlscrolling.o dlbuffer.o \
	   dllist.o dlssl.o err_handler.o
CFLAGS	:= -Wall -lpthread -lcurl
CC	:= gcc


ALL: $(PROGS) $(TOOLS)

debug: ALL
debug: CFLAGS+=-g


%.o: %.c
	$(CC) -c $^ $(CFLAGS)

bdpandl: bdpandl.c $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

bcmp: bcmp.c $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)
bsearch: bsearch.c $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)
repair: repair.c $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)
bdpandl-decode: bdpandl-decode.c $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: ALL debug clean install uninstall
clean:
	$(RM) $(OBJS) $(PROGS) $(TOOLS) $(wildcard *.h.gch)

install:
	cp $(PROGS) /usr/bin

uninstall:
	$(RM) /usr/bin/$(PROGS)
