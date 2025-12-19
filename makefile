CC     = gcc
CFLAGS = -O2 -lX11 -lXpm -lXft -lfontconfig -I/usr/include/freetype2 
PREFIX = /usr

all: sillywm sillyc

sillywm: wm.c ipc.h 
	@$(CC) $< -o $@ $(CFLAGS) 

sillyc: ctl.c ipc.h
	@$(CC) $< -o $@ $(CFLAGS)

install: sillywm
	@mkdir -p $(PREFIX)/bin
	@cp sillywm $(PREFIX)/bin/
	@cp sillyc $(PREFIX)/bin/

clean:
	@rm -f sillywm
	@rm -f sillyc
